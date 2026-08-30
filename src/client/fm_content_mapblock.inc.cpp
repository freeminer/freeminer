// Freeminer scaled LOD and far-mesh implementation.
// Included at the end of content_mapblock.cpp so it can reuse its private
// mesh-generator helpers without changing the upstream implementation.


#include "irr_v3d.h"
#include "fm_mapblock_mesh.cpp.inc"

namespace
{
static bool setFarHostNode(MapNode &node, const std::string &host_name,
		const NodeDefManager *nodedef)
{
	content_t host = CONTENT_IGNORE;
	if (!nodedef->getId(host_name, host))
		return false;

	const auto &host_features = nodedef->get(host);
	if (host_features.drawtype != NDT_NORMAL ||
			host_features.visuals->solidness != 2)
		return false;

	node.setContent(host);
	return true;
}

MapNode simplifyFarNode(MapNode node, const NodeDefManager *nodedef)
{
	const auto &features = nodedef->get(node);

	// Plantlike grass is mostly transparent. Enlarging it to a far cell makes
	// the cutout pixels into terrain-sized holes, so use opaque grass-covered
	// ground from the same namespace as its far representation.
	const std::string grass_marker = ":grass_";
	const auto grass = features.name.rfind(grass_marker);
	if (features.drawtype == NDT_PLANTLIKE && grass != std::string::npos &&
			grass + grass_marker.size() < features.name.size() &&
			std::all_of(features.name.begin() + grass + grass_marker.size(),
					features.name.end(),
					[](unsigned char c) { return c >= '0' && c <= '9'; })) {
		const auto host_name =
				features.name.substr(0, grass) + ":dirt_with_grass";
		if (setFarHostNode(node, host_name, nodedef))
			return node;
	}

	const auto marker = features.name.rfind("_with_");
	if (marker == std::string::npos)
		return node;

	// Nodes named e.g. "default:stone_with_copper" are embedded variants of
	// the node before "_with_". A coarse sample loses the original faces and
	// can make an enclosed ore look exposed. Render its host material instead;
	// omitting the whole coarse cell would create another hole. Restrict this
	// heuristic to stone hosts: surface nodes such as dirt_with_grass and
	// dirt_with_snow deliberately use the same naming pattern for their cover.
	const auto host_name = features.name.substr(0, marker);
	content_t host = CONTENT_IGNORE;
	if (nodedef->getId(host_name, host) && nodedef->get(host).getGroup("stone") > 0)
		setFarHostNode(node, host_name, nodedef);
	return node;
}
}

bool MapblockMeshGenerator::drawFmScaledNode()
{
	if (data->fscale <= 1)
		return false;
	if (cur_node.f->drawtype == NDT_AIRLIKE)
		return true;


	const bool is_far = data->far_step >= 1;
	u8 faces = 0;
	static const v3pos_t tile_dirs[6] = {v3pos_t(0, 1, 0), v3pos_t(0, -1, 0),
			v3pos_t(1, 0, 0), v3pos_t(-1, 0, 0), v3pos_t(0, 0, 1), v3pos_t(0, 0, -1)};
	TileSpec tiles[6];
	u16 lights[6];
	const content_t n1 = cur_node.n.getContent();

	for (int face = 0; face < 6; ++face) {
		const auto p2 = blockpos_nodes + cur_node.p + tile_dirs[face] * data->fscale;
		const MapNode neighbor = data->m_vmanip.getNodeNoEx(p2);
		const content_t n2 = neighbor.getContent();
		bool backface_culling = true;

		if (is_far) {
			// Missing far data must not occlude a known cell. The historical
			// fast-face path also exposed solid/ignore boundaries.
			if (!isFmFarEmpty(n2))
				continue;
		} else {
			if (n2 == n1 || n2 == CONTENT_IGNORE)
				continue;

			bool liquid_needs_top_face =
					face == 0 && cur_node.f->drawtype == NDT_LIQUID &&
					cur_node.f->waving == 3 && data->m_enable_waving_water;
			if (liquid_needs_top_face) {
				liquid_needs_top_face = false;
				static const v3pos_t horizontal_dirs[4] = {v3pos_t(1, 0, 0),
						v3pos_t(-1, 0, 0), v3pos_t(0, 0, 1), v3pos_t(0, 0, -1)};
				for (const auto &dir : horizontal_dirs) {
					const ContentFeatures &side =
							nodedef->get(data->m_vmanip.getNodeNoEx(p2 + dir));
					const bool translucent =
							!(side.visuals->solidness_far || side.visuals->solidness || side.visuals->visual_solidness);
					const bool same_flowing_liquid = side.drawtype == NDT_FLOWINGLIQUID &&
													 cur_node.f->sameLiquidRender(side);
					if (translucent && !same_flowing_liquid) {
						liquid_needs_top_face = true;
						break;
					}
				}
			}

			if (n2 != CONTENT_AIR) {
				const ContentFeatures &f2 = nodedef->get(n2);
				if ((f2.visuals->solidness == 2 ||
						f2.visuals->solidness_far == 2) &&
						!liquid_needs_top_face)
					continue;
				if (cur_node.f->drawtype == NDT_LIQUID) {
					if (cur_node.f->sameLiquidRender(f2))
						continue;
					backface_culling =
							!liquid_needs_top_face &&
							(f2.visuals->solidness || f2.visuals->visual_solidness ||
									f2.visuals->solidness_far);
				}
			}
		}

		faces |= 1 << face;
		getTile(tile_dirs[face], &tiles[face]);
		for (auto &layer : tiles[face].layers) {
			if (backface_culling)
				layer.material_flags |= MATERIAL_FLAG_BACKFACE_CULLING;
		}
		if (!data->m_smooth_lighting)
			lights[face] = getFaceLight(cur_node.n, neighbor, nodedef);
	}

	if (!faces)
		return true;

	const u8 mask = faces ^ 0b0011'1111;
	const auto scaled = data->fscale * BS;
	// Far samples represent a cell ending at the sampled Y level. Keep the
	// original downward Y anchoring; expanding upward exposes underground
	// samples (notably ores) above the surrounding stone surface.

	// aabb3f box(v3f(-HBS, 1.5f * BS - scaled, -HBS), v3f(scaled - HBS, 1.5f * BS, scaled - HBS));

    auto box = aabb3f(v3f(-0.5 * BS), v3f(0.5 * BS));
    if (data->fscale > 1) {
            // TODO: maybe possibe make simpler?/
            box.MinEdge += v3f(HBS, 0, HBS);
            box.MinEdge *= v3f(data->fscale, data->fscale, data->fscale);
            box.MinEdge += v3f(-HBS, -HBS * (data->fscale) + HBS + BS, -HBS);
            box.MaxEdge += v3f(HBS, 0, HBS);
            box.MaxEdge *= v3f(data->fscale, data->fscale, data->fscale);
            box.MaxEdge += v3f(-HBS, -HBS * (data->fscale) + HBS + BS, -HBS);
    }

	box.MinEdge += cur_node.origin;
	box.MaxEdge += cur_node.origin;
	if (is_far) {
		const v3f center = (box.MinEdge + box.MaxEdge) * 0.5f / BS;
		const v3f scale = (box.MaxEdge - box.MinEdge) / BS;
		const v3f texture_pos = v3f::from(cur_node.p) /
				static_cast<float>(data->fscale);
		for (int face = 0; face < 6; ++face) {
			if (mask & (1 << face))
				continue;

			u16 face_lights[4];
			if (data->m_smooth_lighting) {
				v3s16 corners[4];
				getFmNodeVertexDirs(posToS16(tile_dirs[face]), corners);
				for (int vertex = 0; vertex < 4; ++vertex) {
					face_lights[vertex] = getSmoothLightSolid(blockpos_nodes + cur_node.p,
							tile_dirs[face], s16ToPos(corners[vertex]), data);
				}
			} else {
				std::fill_n(face_lights, 4, lights[face]);
			}

			const auto fast_face = makeFastFace(tiles[face], face_lights, texture_pos,
					center, posToS16(tile_dirs[face]), scale, data->fscale,
					cur_node.f->light_source);
			const u16 *indices = fast_face.vertex_0_2_connected
					? quad_indices_02
					: quad_indices_13;
			collector->append(fast_face.tile, fast_face.vertices, 4, indices, 6);
		}
		return true;
	}

	if (data->m_smooth_lighting) {
		LightPair smooth_lights[6][4];
		for (int face = 0; face < 6; ++face) {
			if (mask & (1 << face))
				continue;
			for (int corner_index = 0; corner_index < 4; ++corner_index) {
				const auto corner = light_dirs[light_indices[face][corner_index]];
				smooth_lights[face][corner_index] = LightPair(getSmoothLightSolid(
						blockpos_nodes + cur_node.p, tile_dirs[face], corner, data));
			}
		}

		drawCuboid(box, tiles, 6, nullptr, mask,
				[&](int face, video::S3DVertex vertices[4]) {
					const auto final_lights = smooth_lights[face];
					for (int vertex_index = 0; vertex_index < 4; ++vertex_index) {
						auto &vertex = vertices[vertex_index];
						vertex.Color = encode_light(
								final_lights[vertex_index], cur_node.f->light_source);
						if (!cur_node.f->light_source)
							applyFacesShading(vertex.Color, vertex.Normal);
					}
					return lightDiff(final_lights[1], final_lights[3]) <
										   lightDiff(final_lights[0], final_lights[2])
								   ? QuadDiagonal::Diag13
								   : QuadDiagonal::Diag02;
				});
	} else {
		drawCuboid(box, tiles, 6, nullptr, mask,
				[&](int face, video::S3DVertex vertices[4]) {
					video::SColor color =
							encode_light(lights[face], cur_node.f->light_source);
					if (!cur_node.f->light_source)
						applyFacesShading(color, vertices[0].Normal);
					for (int vertex_index = 0; vertex_index < 4; ++vertex_index)
						vertices[vertex_index].Color = color;
					return QuadDiagonal::Diag02;
				});
	}

	return true;
}

bool MapblockMeshGenerator::generateFm()
{
	if (data->fscale <= 1)
		return false;
	if (data->far_step >= 1 && g_settings->getBool("farmesh_fast_faces"))
		return generateFmFarFastFaces();

	const auto lod_stride = 1 << data->lod_step;
	const auto far_stride = 1 << data->far_step;
	v3pos_t far_pos;
	v3pos_t regular_pos;

	for (far_pos.Z = regular_pos.Z = 0; regular_pos.Z < data->side_length_data;
			regular_pos.Z += lod_stride, far_pos.Z += far_stride)
		for (far_pos.X = regular_pos.X = 0; regular_pos.X < data->side_length_data;
				regular_pos.X += lod_stride, far_pos.X += far_stride)
			for (far_pos.Y = regular_pos.Y = 0; regular_pos.Y < data->side_length_data;
					regular_pos.Y += lod_stride, far_pos.Y += far_stride) {
				cur_node.p = data->far_step ? far_pos : regular_pos;
				cur_node.n =
						data->m_vmanip.getNodeRefAndVisible(blockpos_nodes + cur_node.p)
								.first;
				if (data->far_step)
					cur_node.n = simplifyFarNode(cur_node.n, nodedef);
				cur_node.f = &nodedef->get(cur_node.n);
				drawNode();
			}

	return true;
}

bool MapblockMeshGenerator::generateFmFarFastFaces()
{
	std::vector<s16> coords;
	const auto lod_stride = 1 << data->lod_step;
	const auto far_stride = 1 << data->far_step;
	for (s16 regular = 0, far_v = 0; regular < data->side_length_data;
			regular += lod_stride, far_v += far_stride)
		coords.push_back(far_v);

	static const v3pos_t face_dirs[6] = {
			{0, 1, 0}, {0, -1, 0}, {1, 0, 0},
			{-1, 0, 0}, {0, 0, 1}, {0, 0, -1}};
	// Match the historical fast-face row directions: horizontal rows for
	// top/bottom and Z faces, depth rows for X faces.
	static const int merge_axes[6] = {0, 0, 2, 2, 0, 0};

	const auto set_axis = [](auto &pos, int axis, const auto &value) {
		if (axis == 0)
			pos.X = value;
		else if (axis == 1)
			pos.Y = value;
		else
			pos.Z = value;
	};
	const auto get_face = [&](const auto &pos, int face) {
		FmFarFace result;
		cur_node.p = pos;
		cur_node.n = simplifyFarNode(
				data->m_vmanip.getNodeRefAndVisible(blockpos_nodes + pos).first,
				nodedef);
		cur_node.f = &nodedef->get(cur_node.n);
		if (isFmFarEmpty(cur_node.n.getContent()) ||
				cur_node.f->drawtype == NDT_AIRLIKE)
			return result;

		const auto &dir = face_dirs[face];
		const MapNode neighbor = data->m_vmanip.getNodeNoEx(
				blockpos_nodes + pos + dir * data->fscale);
		// CONTENT_IGNORE/UNKNOWN means that the adjacent far sample is not
		// available. Hiding this known face would leave a visible mesh hole.
		if (!isFmFarEmpty(neighbor.getContent()))
			return result;

		result.visible = true;
		result.pos = pos;
		result.emissive_light = cur_node.f->light_source;
		getTile(dir, &result.tile);
		for (auto &layer : result.tile.layers)
			layer.material_flags |= MATERIAL_FLAG_BACKFACE_CULLING;

		if (data->m_smooth_lighting) {
			v3s16 corners[4];
			getFmNodeVertexDirs(dir, corners);
			for (int vertex = 0; vertex < 4; ++vertex) {
				result.lights[vertex] = getSmoothLightSolid(blockpos_nodes + pos,
						dir, s16ToPos(corners[vertex]), data);
			}
		} else {
			std::fill_n(result.lights, 4,
					getFaceLight(cur_node.n, neighbor, nodedef));
		}
		return result;
	};
	const auto append_run = [&](const FmFarFace &face, const auto &dir,
			int merge_axis, size_t count) {
		v3f scale(data->fscale);
		if (merge_axis == 0)
			scale.X *= count;
		else if (merge_axis == 1)
			scale.Y *= count;
		else
			scale.Z *= count;

		const float fscale = data->fscale;
		v3f center = v3f::from(face.pos) +
				v3f((fscale - 1.0f) * 0.5f, 1.5f - fscale * 0.5f,
						(fscale - 1.0f) * 0.5f);
		v3f row_dir;
		if (merge_axis == 0)
			row_dir.X = 1.0f;
		else if (merge_axis == 1)
			row_dir.Y = 1.0f;
		else
			row_dir.Z = 1.0f;
		center += row_dir * (fscale * (count - 1) * 0.5f);

		const auto fast_face = makeFastFace(face.tile, face.lights,
				v3f::from(face.pos) / fscale, center, posToS16(dir), scale,
				data->fscale, face.emissive_light);
		collector->append(fast_face.tile, fast_face.vertices, 4,
				fast_face.vertex_0_2_connected ? quad_indices_02 : quad_indices_13, 6);
	};

	for (int face = 0; face < 6; ++face) {
		const int merge_axis = merge_axes[face];
		const int fixed_axis_0 = merge_axis == 0 ? 1 : 0;
		const int fixed_axis_1 = merge_axis == 2 ? 1 : 2;
		for (const auto fixed_0 : coords)
		for (const auto fixed_1 : coords) {
			FmFarFace run;
			size_t run_length = 0;
			for (const auto& merged : coords) {
				v3pos_t pos;
				set_axis(pos, merge_axis, merged);
				set_axis(pos, fixed_axis_0, fixed_0);
				set_axis(pos, fixed_axis_1, fixed_1);
				auto next = get_face(pos, face);
				auto expected_pos = run.pos;
				set_axis(expected_pos, merge_axis,
						merge_axis == 0 ? run.pos.X + run_length * data->fscale :
						merge_axis == 1 ? run.pos.Y + run_length * data->fscale :
								run.pos.Z + run_length * data->fscale);
				if (run_length && (next.pos != expected_pos ||
						!canMergeFmFarFaces(run, next))) {
					append_run(run, face_dirs[face], merge_axis, run_length);
					run_length = 0;
				}
				if (!run_length && next.visible) {
					run = std::move(next);
					run_length = 1;
				} else if (run_length) {
					++run_length;
				}
			}
			if (run_length)
				append_run(run, face_dirs[face], merge_axis, run_length);
		}
	}
	return true;
}

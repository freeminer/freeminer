// Freeminer scaled LOD and far-mesh implementation.
// Included at the end of content_mapblock.cpp so it can reuse its private
// mesh-generator helpers without changing the upstream implementation.


#include "irr_v3d.h"
bool MapblockMeshGenerator::drawFmScaledNode()
{
	if (data->fscale <= 1)
		return false;
	if (cur_node.f->drawtype == NDT_AIRLIKE)
		return true;

	const bool far = data->far_step >= 1;
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

		if (far) {
			if (n2 != CONTENT_AIR)
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
				if ( f2.visuals->solidness == 2 || f2.visuals->solidness_far == 2 && !liquid_needs_top_face)
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
	aabb3f box(v3f(-HBS, 1.5f * BS - scaled, -HBS),
			v3f(scaled - HBS, 1.5f * BS, scaled - HBS));
	box.MinEdge += cur_node.origin;
	box.MaxEdge += cur_node.origin;

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
				cur_node.f = &nodedef->get(cur_node.n);
				drawNode();
			}

	return true;
}

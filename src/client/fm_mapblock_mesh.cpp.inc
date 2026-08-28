// Far-mesh fast face builder, adapted from mapblock_mesh.cpp at 356205cd5.

namespace
{
struct FastFace
{
	TileSpec tile;
	video::S3DVertex vertices[4];
	bool vertex_0_2_connected{};
};

struct FmFarFace
{
	bool visible{};
	TileSpec tile;
	u16 lights[4]{};
	v3s16 pos;
	u8 emissive_light{};
};

static bool isFmFarEmpty(content_t content)
{
	return content == CONTENT_AIR || content == CONTENT_IGNORE ||
			content == CONTENT_UNKNOWN;
}

static bool canMergeFmFarFaces(const FmFarFace &first, const FmFarFace &second)
{
	if (!first.visible || !second.visible ||
			first.tile.rotation != TileRotation::None ||
			second.tile.rotation != first.tile.rotation ||
			second.tile.world_aligned != first.tile.world_aligned ||
			second.emissive_light != first.emissive_light ||
			!std::equal(std::begin(first.lights), std::end(first.lights),
					std::begin(second.lights)))
		return false;

	for (int layer = 0; layer < MAX_TILE_LAYERS; ++layer) {
		const auto &a = first.tile.layers[layer];
		const auto &b = second.tile.layers[layer];
		if (a != b || a.texture_layer_idx != b.texture_layer_idx ||
				a.scale != b.scale || a.isTransparent() ||
				!(a.material_flags & MATERIAL_FLAG_TILEABLE_HORIZONTAL) ||
				!(a.material_flags & MATERIAL_FLAG_TILEABLE_VERTICAL))
			return false;
	}
	return true;
}

static const v3s16 fm_vertex_dirs[] = {
		{ 1, -1,  1}, { 1, -1, -1}, { 1,  1, -1}, { 1,  1,  1}, // +X
		{ 1,  1, -1}, {-1,  1, -1}, {-1,  1,  1}, { 1,  1,  1}, // +Y
		{-1, -1,  1}, { 1, -1,  1}, { 1,  1,  1}, {-1,  1,  1}, // +Z
		{}, {}, {}, {},
		{ 1, -1, -1}, {-1, -1, -1}, {-1,  1, -1}, { 1,  1, -1}, // -Z
		{ 1, -1,  1}, {-1, -1,  1}, {-1, -1, -1}, { 1, -1, -1}, // -Y
		{-1, -1, -1}, {-1, -1,  1}, {-1,  1,  1}, {-1,  1, -1}, // -X
};

static void getFmNodeVertexDirs(const v3s16 &dir, v3s16 vertex_dirs[4])
{
	assert(dir.X * dir.X + dir.Y * dir.Y + dir.Z * dir.Z == 1);
	const u8 index = (((dir.X + 2 * dir.Y + 3 * dir.Z) & 7) - 1) * 4;
	std::copy_n(&fm_vertex_dirs[index], 4, vertex_dirs);
}

static void getFmNodeTextureCoords(
		v3f base, const v3f &scale, const v3s16 &dir, float *u, float *v)
{
	if (dir.X > 0 || dir.Y != 0 || dir.Z < 0)
		base -= scale;
	if (dir == v3s16(0, 0, 1)) {
		*u = -base.X;
		*v = -base.Y;
	} else if (dir == v3s16(0, 0, -1)) {
		*u = base.X + 1;
		*v = -base.Y - 1;
	} else if (dir == v3s16(1, 0, 0)) {
		*u = base.Z + 1;
		*v = -base.Y - 1;
	} else if (dir == v3s16(-1, 0, 0)) {
		*u = -base.Z;
		*v = -base.Y;
	} else if (dir == v3s16(0, 1, 0)) {
		*u = base.X + 1;
		*v = -base.Z - 1;
	} else {
		*u = base.X + 1;
		*v = base.Z + 1;
	}
}

static FastFace makeFastFace(const TileSpec &tile, const u16 input_lights[4],
		const v3f &texture_pos, const v3f &center, const v3s16 &dir,
		const v3f &scale, int texture_step, u8 emissive_light)
{
	float x0 = 0.0f;
	float y0 = 0.0f;
	float w = 1.0f;
	float h = 1.0f;
	v3s16 vertex_dirs[4];
	getFmNodeVertexDirs(dir, vertex_dirs);
	u16 lights[4] = {
			input_lights[0], input_lights[1], input_lights[2], input_lights[3]};

	const v3f logical_scale = scale / static_cast<float>(texture_step);
	if (tile.world_aligned)
		getFmNodeTextureCoords(texture_pos, logical_scale, dir, &x0, &y0);

	auto rotate_once = [&]() {
		const auto vertex = vertex_dirs[0];
		vertex_dirs[0] = vertex_dirs[3];
		vertex_dirs[3] = vertex_dirs[2];
		vertex_dirs[2] = vertex_dirs[1];
		vertex_dirs[1] = vertex;
		const auto light = lights[0];
		lights[0] = lights[3];
		lights[3] = lights[2];
		lights[2] = lights[1];
		lights[1] = light;
	};

	switch (tile.rotation) {
	case TileRotation::None:
		break;
	case TileRotation::R90:
		rotate_once();
		break;
	case TileRotation::R180:
		rotate_once();
		rotate_once();
		break;
	case TileRotation::R270:
		rotate_once();
		rotate_once();
		rotate_once();
		break;
	}

	float texture_scale = 1.0f;
	if (logical_scale.X < 0.999f || logical_scale.X > 1.001f)
		texture_scale = logical_scale.X;
	else if (logical_scale.Y < 0.999f || logical_scale.Y > 1.001f)
		texture_scale = logical_scale.Y;
	else if (logical_scale.Z < 0.999f || logical_scale.Z > 1.001f)
		texture_scale = logical_scale.Z;
	texture_scale *= texture_step;

	const v2f32 texture_coords[4] = {
			{x0 + w * texture_scale, y0 + h}, {x0, y0 + h},
			{x0, y0}, {x0 + w * texture_scale, y0}};
	const v3f normal = v3f::from(dir);
	FastFace face;
	face.tile = tile;
	for (u8 i = 0; i < 4; ++i) {
		v3f position(BS * 0.5f * vertex_dirs[i].X * scale.X,
				BS * 0.5f * vertex_dirs[i].Y * scale.Y,
				BS * 0.5f * vertex_dirs[i].Z * scale.Z);
		position += center * BS;
		auto color = encode_light(lights[i], emissive_light);
		if (!emissive_light)
			applyFacesShading(color, normal);
		face.vertices[i] =
				video::S3DVertex(position, normal, color, texture_coords[i]);
	}

	const auto light_diff = [](u16 first, u16 second) {
		const LightPair a(first);
		const LightPair b(second);
		return std::abs(a.lightDay - b.lightDay) +
				std::abs(a.lightNight - b.lightNight);
	};
	face.vertex_0_2_connected =
			light_diff(lights[0], lights[2]) < light_diff(lights[1], lights[3]);
	return face;
}
}

// SPDX-License-Identifier: GPL-3.0-or-later
// Included by node_visuals.cpp after its tile-loading helpers.

static void fillOpaqueFarTile(NodeVisuals &visuals, u8 index, const TileDef &definition,
		TileAttribContext context, MaterialType material, const GetShaderCallback &shader)
{
	if (!visuals.fm_far_tiles)
		visuals.fm_far_tiles = std::make_unique<TileLayer[]>(6);
	auto &far_tile = visuals.fm_far_tiles[index];
	fillTileAttribs(&far_tile, context, visuals.tiles[index],
			farmesh::opaqueFarTileDef(definition), material, shader);
	far_tile.need_polygon_offset = visuals.tiles[index].layers[0].need_polygon_offset;
}

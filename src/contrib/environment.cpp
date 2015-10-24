/*
 * Epixel
 * Copyright (C) 2015 nerzhul, Loic Blot <loic.blot@unix-experience.fr>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
**/

#include "environment.h"
/*
#include "chathandler.h"
#include "content_sao.h"
#include "daynightratio.h"
#include "filesys.h"
*/
#include "gamedef.h"
#include "inventory.h"
/*
#include "mg_schematic.h"
#include "player.h"
#include "scripting_game.h"
*/
#include "server.h"
/*
#include "script/lua_api/l_item.h"
*/
#include "contrib/fallingsao.h"
#include "contrib/itemsao.h"
/*
#include "contrib/playersao.h"
#include <math.h>

void ServerEnvironment::contrib_player_globalstep(RemotePlayer* player, float dtime)
{
	PlayerSAO* sao = player->getPlayerSAO();
	Inventory* inv = sao->getInventory();
	m_area_mgr->updatePlayerHud(player);

	if (sao && sao->getHP()) {
		v3f pos = sao->getBasePosition();
		pos.Y += 0.5;

		for (const auto &i: m_active_objects) {
			ServerActiveObject* obj = i.second;
			if (obj->getType() == ACTIVEOBJECT_TYPE_LUAITEM && !obj->m_removed) {
				contrib_lookupitemtogather(player, pos, inv, obj);
			}
		}
	}
}

void ServerEnvironment::contrib_lookupitemtogather(RemotePlayer* player, v3f playerPos,
		Inventory* inv, ServerActiveObject* obj)
{
	Server* srv = ((Server*)m_gamedef);
	epixel::ItemSAO* entity = (epixel::ItemSAO*)obj;

	// We can loot item only after a certain amount of time
	// This permit players to remove items outside of their inventory
	// on the ground
	if (!entity->canBeLooted())
		return;

	v3f pos2 = obj->getBasePosition();
	float dist = playerPos.getDistanceFrom(pos2) / BS;
	InventoryList* invList = inv->getList("main");
	if (dist <= 3.0f) {
		ItemStack item ;

		// If items are attached on SAO (function added by nrz)
		// Get them instead of deserialize item
		if (entity->getAttachedItems().count) {
			item = entity->getAttachedItems();
		}

		if (invList->roomForItem(item)) {
			if (dist <= 1.0f) {
				inv->addItem("main", item);
				SimpleSoundSpec spec;
				spec.name = "item_drop_pickup";

				ServerSoundParams params;
				params.gain = 0.4f;
				params.to_player = player->getName();

				srv->playSound(spec, params);
				InventoryLocation loc;
				loc.type = InventoryLocation::PLAYER;
				loc.setPlayer(player->getName());
				srv->setInventoryModified(loc);
				obj->m_removed = true;
			}
			else {
				v3f pos1 = playerPos;
				pos1.Y += 0.6f;
				v3f vel = pos1 - pos2;
				vel *= 3;
				entity->setVelocity(vel);
			}
		}
	}
}
*/

epixel::ItemSAO* ServerEnvironment::spawnItemActiveObject(const std::string &itemName,
		v3f pos, const ItemStack &items)
{
	epixel::ItemSAO* obj = new epixel::ItemSAO(this, pos, "__builtin:item", "");
	if (addActiveObject(obj)) {
		IItemDefManager* idef = m_gamedef->getItemDefManager();
		float s = 0.2 + 0.1 * (items.count / items.getStackMax(idef));
		ObjectProperties* objProps = obj->accessObjectProperties();

		objProps->is_visible = true;
		objProps->visual = "wielditem";
		objProps->mesh = "empty.obj";
		objProps->textures.clear();
		objProps->textures.push_back(itemName);
		objProps->physical = true;
		objProps->collideWithObjects = false;
		objProps->visual_size = v2f(s, s);
		objProps->collisionbox = core::aabbox3d<f32>(-s,-s,-s,s,s,s);
		objProps->automatic_rotate = 3.1415 * 0.5;
		obj->notifyObjectPropertiesModified();
		obj->attachItems(items);
		return obj;
	}
	return nullptr;
}


epixel::FallingSAO* ServerEnvironment::spawnFallingActiveObject(const std::string &nodeName,
		v3f pos, const MapNode n, int fast)
{
	epixel::FallingSAO* obj = new epixel::FallingSAO(this, pos, "__builtin:falling_node", "", fast);
	if (addActiveObject(obj)) {
		ObjectProperties* objProps = obj->accessObjectProperties();
		if (!objProps)
			return nullptr;
		objProps->is_visible = true;
		objProps->visual = "wielditem";
		objProps->textures.clear();
		objProps->textures.push_back(nodeName);
		objProps->physical = true;
		objProps->collideWithObjects = false;
		obj->notifyObjectPropertiesModified();
		obj->attachNode(n);
		return obj;
	}
	return nullptr;
}

#if 0
void ServerEnvironment::removeNodesInArea(v3s16 minp, v3s16 maxp, std::set<content_t> &filter)
{
	for (s16 x = minp.X; x <= maxp.X; x++) {
		for (s16 y = minp.Y; y <= maxp.Y; y++) {
			for (s16 z = minp.Z; z <= maxp.Z; z++) {
				v3s16 p(x, y, z);
				content_t c = m_map->getNode(p).getContent();
				if (filter.count(c) != 0) {
					removeNode(p);
				}
			}
		}
	}
}

bool ServerEnvironment::findIfNodeInArea(v3s16 minp, v3s16 maxp, const std::set<content_t> &filter)
{
	for (s16 x = minp.X; x <= maxp.X; x++) {
		for (s16 y = minp.Y; y <= maxp.Y; y++) {
			for (s16 z = minp.Z; z <= maxp.Z; z++) {
				v3s16 p_found(x, y, z);
				content_t c = m_map->getNode(p_found).getContent();
				if (filter.count(c) != 0) {
					return true;
				}
			}
		}
	}
	return false;
}

bool ServerEnvironment::findNodeNear(const v3s16 pos, const s32 radius,
		const std::set<content_t> &filter, v3s16 &found_pos)
{
	for(int d = 1; d <= radius; d++){
		std::vector<v3s16> list = FacePositionCache::getFacePositions(d);
		for(std::vector<v3s16>::iterator i = list.begin();
				i != list.end(); ++i){
			v3s16 p = pos + (*i);
			content_t c = m_map->getNode(p).getContent();
			if(filter.count(c) != 0) {
				found_pos = p;
				return true;
			}
		}
	}

	return false;
}

bool ServerEnvironment::findNodeNear(const v3s16 pos, const s32 radius,
		const std::set<content_t> &filter)
{
	v3s16 p;
	return findNodeNear(pos, radius, filter, p);
}

const u8 ServerEnvironment::getNodeLight(const v3s16 pos)
{
	u32 time_of_day = getTimeOfDay();
	time_of_day %= 24000;
	u32 dnr = time_to_daynight_ratio(time_of_day, true);

	MapNode n = m_map->getNode(pos);
	return n.getLightBlend(dnr, m_gamedef->ndef());
}

#endif

void ServerEnvironment::nodeUpdate(const v3s16 pos, int recurse, int fast)
{
	if (recurse-- <= 0)
		return;
	INodeDefManager* ndef = m_gamedef->getNodeDefManager();
/*
	IItemDefManager* idef = m_gamedef->getItemDefManager();
*/
	// update nodes around
	for (s32 x = pos.X - 1; x <= pos.X + 1; x++) {
		for (s32 y = pos.Y - 1; y <= pos.Y + 1; y++) {
			for (s32 z = pos.Z - 1; z <= pos.Z + 1; z++) {
				MapNode n = m_map->getNode(v3s16(x,y,z));
				ContentFeatures f = ndef->get(n);
				ItemGroupList groups = f.groups;

				// Check is the node is considered valid to fall
				if (itemgroup_get(groups, "falling_node")) {
					MapNode n_bottom = m_map->getNode(v3s16(x, y - 1, z));
					ContentFeatures f_under = ndef->get(n_bottom);

					if ((itemgroup_get(groups, "float") == 0 || f_under.liquid_type == LIQUID_NONE) &&
						(f.name != f_under.name || (f_under.leveled &&
							n_bottom.getLevel(ndef) < n_bottom.getMaxLevel(ndef))) &&
						(!f_under.walkable || f_under.buildable_to)) {
						removeNode(v3s16(x,y,z), fast);
						spawnFallingActiveObject(f.name, intToFloat(v3s16(x,y,z),BS), n, fast);
						nodeUpdate(v3s16(x,y,z), recurse, fast);
					}
				}
			}
		}
	}
}

#if 0

namespace epixel
{

/**
 * @brief ChatHandler::handleCommand_we_createscheme
 * @param args
 * @param peer_id
 * @return returns true
 *
 * Handle command /we createscheme
 * This create a scheme to <worlddir>/schems/ directory
 */
bool ChatHandler::handleCommand_we_createscheme(std::string args, u16 peer_id)
{
	PlayerSAO* sao = m_server->getPlayerSAO(peer_id);
	if (!sao) {
		return true;
	}

	std::vector<std::string> commandline;
	std::stringstream ss(args);
	std::string item;
	while (std::getline(ss, item, ' ')) {
		commandline.push_back(item);
	}

	bool compress = true;
	if (commandline.size() < 1 || commandline.size() > 2) {
		m_server->SendChatMessage(peer_id, L"Invalid usage, see /help we createscheme");
		return true;
	}
	else if (commandline.size() == 2) {
		compress = is_yes(commandline[1]);
	}

	const std::string schemdir = m_server->getAreaMgr()->getWorldPath() + "/schems";

	// If dir doesn't exist create it
	if (!fs::PathExists(schemdir)) {
		fs::CreateDir(schemdir);
	}
	else if (fs::PathExists(schemdir) && !fs::IsDir(schemdir)) {
		m_server->SendChatMessage(peer_id, L"FATAL ERROR: the scheme directory is a file. Could not continue");
		return true;
	}

	std::string filename = m_server->getAreaMgr()->getWorldPath() + "/schems/" + commandline[0] + ".mts";

	v3s16 pos1 = sao->getAreaPos(0);
	v3s16 pos2 = sao->getAreaPos(1);
	if (pos1 == v3s16(0,0,0) && pos2 == v3s16(0,0,0)) {
		m_server->SendChatMessage(peer_id, L"No area selected. Please set a pos1 and a pos2 with /we pos1 and /we pos2");
		return true;
	}
	sortBoxVerticies(pos1, pos2);

	Schematic schem;
	schem.getSchematicFromMap(&(m_server->getMap()), pos1, pos2);

	std::wstringstream ws;

	// Catch the file exception, don't crash the whole server
	try {
		schem.saveSchematicToFile(filename, m_server->getNodeDefManager(), compress);
	}
	catch (VersionMismatchException &e) {
		ws << e.what();
		m_server->SendChatMessage(peer_id, ws.str());
		return true;
	}
	catch (SerializationError &e) {
		ws << e.what();
		m_server->SendChatMessage(peer_id, ws.str());
		return true;
	}

	ws << L"Schematic saved to file {world_dir}/" << commandline[0].c_str() << L".mts";
	actionstream << "Player " << m_server->getPlayerName(peer_id) << ": saved schematic to file '"
		<< filename << "'." << std::endl;

	m_server->SendChatMessage(peer_id, ws.str());
	return true;
}

bool ChatHandler::handleCommand_we_importscheme(std::string args, u16 peer_id)
{
	PlayerSAO* sao = m_server->getPlayerSAO(peer_id);
	if (!sao) {
		return true;
	}

	std::vector<std::string> commandline;
	std::stringstream ss(args);
	std::string item;
	while (std::getline(ss, item, ' ')) {
		commandline.push_back(item);
	}

	if (commandline.size() < 1 || commandline.size() > 3) {
		m_server->SendChatMessage(peer_id, L"Invalid usage, see /help we createscheme");
		return true;
	}

	bool compress = true;
	Rotation rt = ROTATE_0;
	if (commandline.size() >= 2) {
		u32 tmpint = std::atoi(commandline[1].c_str());
		if (tmpint > 3) {
			m_server->SendChatMessage(peer_id, L"Invalid usage, see /help we createscheme");
			return true;
		}
		rt = (Rotation) tmpint;
	}

	if (commandline.size() == 3) {
		compress = is_yes(commandline[2]);
	}

	std::wstringstream ws;
	std::string filename = m_server->getAreaMgr()->getWorldPath() + "/schems/" + commandline[0] + ".mts";
	// If file doesn't exist error
	if (!fs::PathExists(filename)) {
		ws << L"File " << commandline[0].c_str() << " doesn't exist.";
		m_server->SendChatMessage(peer_id, ws.str());
		return true;
	}

	Schematic *schem = NULL;
	schem = SchematicManager::create(SCHEMATIC_NORMAL);

	try {
		if (!schem->loadSchematicFromFile(filename, m_server->getNodeDefManager(), NULL, compress)) {
			delete schem;
			return true;
		}
	} catch (VersionMismatchException &e) {
		delete schem;
		ws << e.what();
		m_server->SendChatMessage(peer_id, ws.str());
		return true;
	} catch (SerializationError &e) {
		delete schem;
		ws << e.what();
		m_server->SendChatMessage(peer_id, ws.str());
		return true;
	}

	v3s16 pos = sao->getAreaPos(0);

	schem->placeStructure((ServerMap*)&(m_server->getMap()), pos, 0, rt, true);

	ws << L"Schematic " << commandline[0].c_str() << L".mts loaded and placed.";
	actionstream << "Player " << m_server->getPlayerName(peer_id) << ": place schematic '"
		<< filename << "' on pos (" << pos.X << "," << pos.Y << "," << pos.Z << ")" << std::endl;

	m_server->SendChatMessage(peer_id, ws.str());
	return true;
}

}

#endif

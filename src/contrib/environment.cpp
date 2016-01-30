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
#if 0
#include "chathandler.h"
#include "content_sao.h"
#include "daynightratio.h"
#include "emerge.h"
#include "util/filesys.h"
#endif
#include "gamedef.h"
#include "inventory.h"
#if 0
#include "mg_schematic.h"
#include "player.h"
#include "scripting_game.h"
#endif
#include "server.h"
#if 0
#include "script/lua_api/l_item.h"
#endif
#include "contrib/fallingsao.h"
#include "contrib/itemsao.h"
#if 0
#include "contrib/playersao.h"
#include "contrib/projectilesao.h"
#include "contrib/spell.h"
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

#endif

void ServerEnvironment::contrib_globalstep(const float dtime)
{
#if 0
	// Map autogenerator & updater
	// This is only triggered when nobody is connected
	if (g_settings->get(BOOLSETTING_ENABLE_AUTOGENERATING_MAP_WHEN_INACTIVE)) {
		m_autogenerator_timer -= dtime;
		if (m_autogenerator_timer <= 0.0f) {
			m_autogenerator_timer = 10.0f;
			if (((Server*)m_gamedef)->getClientsCount() == 0) {
				if(m_current_autogen_offset < MAX_MAP_GENERATION_LIMIT / MAP_BLOCKSIZE) {
					s16 x = m_current_autogen_offset, y = m_current_autogen_offset, z = m_current_autogen_offset;
					m_map->getEmergeManager()->enqueueBlockEmerge(PEER_ID_INEXISTENT, v3s16(x,y,z), true, true);
					m_map->getEmergeManager()->enqueueBlockEmerge(PEER_ID_INEXISTENT, v3s16(x,-y,z), true, true);
					m_map->getEmergeManager()->enqueueBlockEmerge(PEER_ID_INEXISTENT, v3s16(x,y,-z), true, true);
					m_map->getEmergeManager()->enqueueBlockEmerge(PEER_ID_INEXISTENT, v3s16(-x,y,z), true, true);
					m_map->getEmergeManager()->enqueueBlockEmerge(PEER_ID_INEXISTENT, v3s16(-x,-y,z), true, true);
					m_map->getEmergeManager()->enqueueBlockEmerge(PEER_ID_INEXISTENT, v3s16(-x,y,-z), true, true);
					m_map->getEmergeManager()->enqueueBlockEmerge(PEER_ID_INEXISTENT, v3s16(x,-y,-z), true, true);
					m_map->getEmergeManager()->enqueueBlockEmerge(PEER_ID_INEXISTENT, v3s16(-x,-y,-z), true, true);
					logger.debug("[MapBlock AutoGenerator] MapBlock (%d,%d,%d),"
							" and all negative variantes enqueued for generation.",
							x * MAP_BLOCKSIZE, y * MAP_BLOCKSIZE, z * MAP_BLOCKSIZE);
					m_current_autogen_offset++;
				}
				else {
					m_current_autogen_offset = 0;
				}
			}
		}
	}

	u16 explosion_limit = (m_explosion_queue.size() >= 10 ? 10 : m_explosion_queue.size());
	for (s16 i = 0; i < explosion_limit && !m_explosion_queue.empty(); i++) {
		v3s16 pos = m_explosion_queue.front();
		m_explosion_queue.pop_front();

		MapNode n = m_map->getNode(pos);
		content_t c = n.getContent();
		const ContentFeatures &f = m_gamedef->getNodeDefManager()->get(c);

		// If current node is not air, it should explode
		if (c != CONTENT_AIR) {
			makeExplosion(pos, f.explosionRadius);
		}
		// Else it already explode, it doesn't count for the loop limit, try next node
		else {
			i--;
		}
	}
#endif

	// Differed nodeupdates
	//if (m_nodeupdate_queue.size() > 0) {
		std::deque<v3s16> nuqueue; 
		int i = 0;
		while(++i < 1000 && !m_nodeupdate_queue.empty()) {nuqueue.emplace_back(m_nodeupdate_queue.front()); m_nodeupdate_queue.pop_front();}
		//m_nodeupdate_queue.clear();
		std::sort(nuqueue.begin(), nuqueue.end());
		nuqueue.erase(std::unique(nuqueue.begin(), nuqueue.end()), nuqueue.end());

		for (const auto &pos: nuqueue) {
			nodeUpdate(pos, 10);
		}
	//}
}

#if 0
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
#endif

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
epixel::ProjectileSAO* ServerEnvironment::spawnProjectileActiveObject(const u32 spellId,
		const u32 casterId, v3f pos)
{
	epixel::Spell* spell = ((Server*)m_gamedef)->getSpell(spellId);
	if (!spell) {
		return nullptr;
	}

	epixel::ProjectileSAO* obj = new epixel::ProjectileSAO(this, pos, spell->name, "", spell, casterId);
	if (addActiveObject(obj)) {
		ObjectProperties* objProps = obj->accessObjectProperties();
		objProps->physical = false;
		objProps->collisionbox = core::aabbox3d<f32>(0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
		objProps->visual = spell->visual;
		objProps->visual_size = spell->visual_size;
		objProps->textures = {spell->texture};
		objProps->is_visible = true;
		objProps->collideWithObjects = false;
		obj->notifyObjectPropertiesModified();
		return obj;
	}
	return nullptr;
}

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

bool ServerEnvironment::findIfNodeInArea(v3s16 minp, v3s16 maxp, const std::vector<content_t> &filter)
{
	for (s16 x = minp.X; x <= maxp.X; x++) {
		for (s16 y = minp.Y; y <= maxp.Y; y++) {
			for (s16 z = minp.Z; z <= maxp.Z; z++) {
				v3s16 p_found(x, y, z);
				content_t c = m_map->getNode(p_found).getContent();
				if (std::find(filter.begin(), filter.end(), c) != filter.end()) {
					return true;
				}
			}
		}
	}
	return false;
}

bool ServerEnvironment::findNodeNear(const v3s16 pos, const s32 radius,
		const std::vector<content_t> &filter, v3s16 &found_pos)
{
	for(int d = 1; d <= radius; d++) {
		std::vector<v3s16> list = FacePositionCache::getFacePositions(d);
		for(const auto &pl: list) {
			v3s16 p = pos + pl;
			content_t c = m_map->getNode(p).getContent();
			if(std::find(filter.begin(), filter.end(), c) != filter.end()) {
				found_pos = p;
				return true;
			}
		}
	}

	return false;
}

bool ServerEnvironment::findNodeNear(const v3s16 pos, const s32 radius,
		const std::vector<content_t> &filter)
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

/**
 * @brief ServerEnvironment::nodeUpdate
 * @param pos
 *
 * When node is modified, update node and directly
 * linked nodes
 */
void ServerEnvironment::nodeUpdate(const v3s16 pos, u16 recursion_limit, int fast, bool destroy)
{
	// Limit nodeUpdate recursion & differ updates to avoid stack overflow
	if (--recursion_limit <= 0) {
		m_nodeupdate_queue.push_back(pos);
		return;
	}

	INodeDefManager* ndef = m_gamedef->getNodeDefManager();
	MapNode n, n_bottom;
	ContentFeatures f;
	ItemGroupList groups;

	// update nodes around
	for (s32 x = pos.X - 1; x <= pos.X + 1; x++) {
		for (s32 y = pos.Y - 1; y <= pos.Y + 1; y++) {
			for (s32 z = pos.Z - 1; z <= pos.Z + 1; z++) {
				v3s16 n_pos = v3s16(x,y,z);

				// If it's current node, ignore
				if (x == 0 && y == 0 && z == 0) {
					continue;
				}

				n = m_map->getNode(n_pos);
				// Current mapblock not loaded, ignore
				if (n.getContent() == CONTENT_IGNORE) {
					continue;
				}

				f = ndef->get(n);
				groups = f.groups;
				n_bottom = m_map->getNode(v3s16(x, y - 1, z));

				// Check is the node is considered valid to fall
				if (n_bottom.getContent() != CONTENT_IGNORE && (destroy || itemgroup_get(groups, "falling_node"))) {
					const ContentFeatures &f_under = ndef->get(n_bottom);

					if ((itemgroup_get(groups, "float") == 0 || f_under.liquid_type == LIQUID_NONE) &&
						(f.name.compare(f_under.name) != 0 || (f_under.leveled &&
							n_bottom.getLevel(ndef) < n_bottom.getMaxLevel(ndef))) &&
						(!f_under.walkable || f_under.buildable_to)) {
						removeNode(n_pos, fast);
						spawnFallingActiveObject(f.name, intToFloat(v3s16(x,y,z),BS), n, fast);
						nodeUpdate(n_pos, recursion_limit, fast, destroy);
					}
				}

/*
				if (itemgroup_get(groups, "attached_node")) {
					if (!checkAttachedNode(n_pos, n, f)) {
						removeNode(n_pos, fast);
						//handleNodeDrops(f, intToFloat(n_pos, BS));
					}
				}
*/
			}
		}
	}
}

#if 0
static const char* nodeIdMapping[ENVIRONMENT_NODECACHE_ID_MAX] = {
	"group:water",
	"default:chest",
	"default:chest_locked",
	"default:obsidian",
	"default:obsidianbrick",
	"default:furnace",
	"default:furnace_active",
};
/**
 * @brief ServerEnvironment::generateNodeIdCache
 *
 * Create a global nodeid cache to improve the core
 * performance and reduce some memory usage on
 * newly used objects inside core
 */
void ServerEnvironment::generateNodeIdCache()
{
	for (u16 i = 0; i < ENVIRONMENT_NODECACHE_ID_MAX; i++) {
		m_gamedef->getNodeDefManager()->getIds(nodeIdMapping[i], m_nodeid_cache[i]);
	}
}

/**
 * @brief ServerEnvironment::makeExplosion
 * @param pos
 * @param radius
 *
 * Generate an explosion on pos with radius.
 */
bool ServerEnvironment::makeExplosion(const v3s16 pos, const float radius,
		const u16 objId, const std::string &who)
{
	// Limit explosion per Server step.
	if (m_explosion_loop_counter >= 10) {
		// Look if the position to explode is in the current explosion queue
		// Return (objId != 0) to remove properly the creature/spell if explosion is in queue
		for (const auto &p: m_explosion_queue) {
			// if yes, cancel this enqueue
			if (p == pos) {
				return (objId != 0);
			}
		}
		m_explosion_queue.push_back(pos);
		m_explosion_loop_counter++;
		return (objId != 0);
	}

	std::string owner = who;
	// If we disable the target <=> owner matching for explosions, areas will be protected
	// incoditionnaly
	if (!g_settings->get(BOOLSETTING_ENABLE_EXPLOSION_PROTECTION_TARGETOWNER))  {
		owner = "null";
	}

	// This is not a mob explosion but a node explosion, explode the node itself
	if (objId == 0) {
		explodeNode(pos);
	}

	bool hasExploded = false;
	for (float x = -radius; x <= radius; x++) {
		for (float y = -radius; y <= radius; y++) {
			for (float z = -radius; z <= radius; z++) {
				v3s16 posToExplode(pos.X + x, pos.Y + y, pos.Z + z);

				// If it's a node explosion and it's the source node, ignore
				// It already explode
				if (objId == 0 && posToExplode == pos) {
					continue;
				}

				// Do a beautiful sphere
				if (posToExplode.getDistanceFrom(pos) > radius) {
					continue;
				}

				// If target cannot interact in area, stop immediately to prevent
				// damage on other players areas
				// But let the TNT node itself (comparing with null name)
				if ((posToExplode != pos || owner.compare("null") != 0) &&
						!((Server*)m_gamedef)->getAreaMgr()->
							canInteract(intToFloat(posToExplode, 1), owner)) {
					continue;
				}

				if (!hasExploded) {
					// count one more explosion
					m_explosion_loop_counter++;
					// And declare and explosion
					hasExploded = true;
				}
				explodeNode(posToExplode);
			}
		}
	}

	if (hasExploded) {
		((Server*)m_gamedef)->playSound("tnt_explode", pos, 2 * 64 * BS);

		std::vector<ServerActiveObject*> objects;
		getObjectsInsideRadius(objects, intToFloat(pos, BS), radius * BS);
		for (const auto &obj: objects) {
			if ((obj->getType() != ACTIVEOBJECT_TYPE_PLAYER &&
					obj->getType() != ACTIVEOBJECT_TYPE_LUACREATURE) ||
					obj->getId() == objId) {
				continue;
			}

			float dist = pos.getDistanceFrom(floatToInt(obj->getBasePosition(), BS));
			if (dist == 0.0f) {
				dist = 1.0f;
			}

			obj->setHP(obj->getHP() - 6 / dist * radius);
			if (obj->getType() == ACTIVEOBJECT_TYPE_PLAYER) {
				((Server*)m_gamedef)->SendPlayerHPOrDie((PlayerSAO *)obj);
			}
		}
	}

	return hasExploded;
}

/**
 * @brief ServerEnvironment::explodeNode
 * @param pos
 *
 * Explode not at pos
 */
void ServerEnvironment::explodeNode(const v3s16 pos)
{
	MapNode n = m_map->getNode(pos);
	content_t c = n.getContent();
	const ContentFeatures &f = m_gamedef->getNodeDefManager()->get(c);

	// Some nodes cannot explode
	if (c != CONTENT_IGNORE &&
			c != CONTENT_AIR &&
			m_nodeid_cache[ENVIRONMENT_NODECACHE_CHEST_LOCKED].count(c) == 0 &&
			m_nodeid_cache[ENVIRONMENT_NODECACHE_OBSIDIAN].count(c) == 0 &&
			m_nodeid_cache[ENVIRONMENT_NODECACHE_OBSIDIAN_BRICK].count(c) == 0 &&
			!f.unbreakable) {
		v3f f_pos = intToFloat(pos, BS);
		// Chest explosion: spawn all items on ground
		if (m_nodeid_cache[ENVIRONMENT_NODECACHE_CHEST].count(c) > 0) {
			if (NodeMetadata* meta = m_map->getNodeMetadata(pos)) {
				if (InventoryList* inv_list = meta->getInventory()->getList("main")) {
					u16 listSize = inv_list->getSize();
					for (u16 i = 0; i < listSize; i++) {
						ItemStack stack = inv_list->getItem(i);
						spawnItemActiveObject(stack.getDefinition(m_gamedef->getItemDefManager()).name,
								f_pos, stack);
					}
				}
			}
		}

		if (m_nodeid_cache[ENVIRONMENT_NODECACHE_FURNACE].count(c) > 0 ||
				m_nodeid_cache[ENVIRONMENT_NODECACHE_FURNACE_ACTIVE].count(c) > 0) {
			if (NodeMetadata* meta = m_map->getNodeMetadata(pos)) {
				if (InventoryList* inv_list = meta->getInventory()->getList("src")) {
					spawnInventoryOnGround(inv_list, f_pos);
				}
				if (InventoryList* inv_list = meta->getInventory()->getList("dst")) {
					spawnInventoryOnGround(inv_list, f_pos);
				}
				if (InventoryList* inv_list = meta->getInventory()->getList("fuel")) {
					spawnInventoryOnGround(inv_list, f_pos);
				}
			}
		}

		// If it's not a chain explosion or if chain explosion + will explose
		if (!f.explodeInChain || m_explosion_loop_counter < 10) {
			spawnEffect(f_pos, 15, "tnt_smoke.png", 2.0f);
			removeNode(pos);
		}

		if (f.explodeInChain) {
			makeExplosion(pos, f.explosionRadius);
		}
	}
}

/**
 * @brief ServerEnvironment::spawnInventoryOnGround
 * @param inv_list
 */
void ServerEnvironment::spawnInventoryOnGround(InventoryList *inv_list, const v3f &pos)
{
	s16 inv_size = inv_list->getSize();

	// First try all the non-empty slots
	for (s16 i = 0; i < inv_size; i++) {
		ItemStack s = inv_list->getItem(i);
		if (!s.empty()) {
			spawnItemActiveObject(s.name, pos, s);
		}
	}

	inv_list->clearItems();
}

/**
 * @brief ServerEnvironment::spawnEffect
 * @param pos
 * @param amount
 * @param texture
 * @param max_size
 *
 * spawn amount effect on active object position
 * using texture with max_size
 */
void ServerEnvironment::spawnEffect(v3f pos, u32 amount, const std::string &texture, float max_size)
{
	v3f pos_n = pos / BS;
	((Server*)m_gamedef)->addParticleSpawner(amount, 0.25,
		pos_n, pos_n, v3f(0,-2,0), v3f(2,2,2),
		v3f(-4,-4,-4), v3f(4,4,4), 0.1, 1, 0.5, max_size,
		false, false, texture, "");
}


/**
 * @brief ServerEnvironment::handleNodeDrops
 * @param f
 * @param pos
 * @param player
 *
 * Drops node loots on the ground
 */
void ServerEnvironment::handleNodeDrops(const ContentFeatures &f, v3f pos, PlayerSAO *player)
{
	std::vector<ContentFeatureSingleDrop> drops;
	if (f.drops.items.size() == 0) {
		drops.push_back({f.name,1});
	}
	else {
		drops = f.drops.items;
	}

	IItemDefManager* idef = m_gamedef->getItemDefManager();

	// Handle drops on the ground
	for (const auto &drop: drops) {
		if (myrand() % drop.rarity == 0) {
			ItemStack stack;
			stack.deSerialize(drop.item, idef);
			if (f.drops.max_items > 0)
				stack.add(myrand_range(0,f.drops.max_items - 1));
			if (!g_settings->get(BOOLSETTING_CREATIVE_MODE) || !player) {
				if (epixel::ItemSAO* obj = spawnItemActiveObject(stack.getDefinition(idef).name, pos, stack)) {
					obj->setVelocity(v3f(myrand_range(-1, 1) * BS, 5 * BS, myrand_range(-1, 1) * BS));
				}
			}
			else if (player && player->getInventory()){
				player->getInventory()->addItem("main", stack);
			}
		}
	}
}

#endif

bool ServerEnvironment::checkAttachedNode(const v3s16 pos, MapNode n, const ContentFeatures &f)
{
	v3s16 d(0,0,0);
	if (f.param_type_2 == CPT2_WALLMOUNTED) {
		switch (n.param2) {
			case 0: d.Y = 1; break;
			case 1: d.Y = -1; break;
			case 2: d.X = 1; break;
			case 3: d.X = -1; break;
			case 4: d.Z = 1; break;
			case 5: d.Z = -1; break;
		}
	}
	else {
		d.Y = -1;
	}

/*
	bool exists = false;
*/
	MapNode n2 = m_map->getNode(pos + d);
	INodeDefManager* ndef = m_gamedef->getNodeDefManager();
	if (n2.getContent() != CONTENT_IGNORE && !ndef->get(n2).walkable) {
		return false;
	}
	return true;
}

#if 0
void ServerEnvironment::getUnitsInsideRadius(std::vector<UnitSAO*> &objects, v3f pos, float radius)
{
	for(std::map<u16, ServerActiveObject*>::iterator
			i = m_active_objects.begin();
			i != m_active_objects.end(); ++i) {
		ServerActiveObject* obj = i->second;

		// Player & Creatures are valid for this call
		if (obj->getType() != ACTIVEOBJECT_TYPE_PLAYER &&
				obj->getType() != ACTIVEOBJECT_TYPE_LUACREATURE) {
			continue;
		}

		v3f objectpos = obj->getBasePosition();
		if(objectpos.getDistanceFrom(pos) > radius)
			continue;
		objects.push_back((UnitSAO*)obj);
	}
}

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
bool ChatHandler::handleCommand_we_createscheme(const std::string &args, const u16 peer_id)
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
	if (commandline.empty() || commandline.size() > 2) {
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
	logger.notice("Player %s saved schematic to file '%s'.",
			m_server->getPlayerName(peer_id).c_str(), filename.c_str());

	m_server->SendChatMessage(peer_id, ws.str());
	return true;
}

bool ChatHandler::handleCommand_we_importscheme(const std::string &args, const u16 peer_id)
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
	logger.notice("Player %s placed schematic '%s' on pos (%d,%d,%d)",
			m_server->getPlayerName(peer_id).c_str(), filename.c_str(), pos.X, pos.Y, pos.Z);

	m_server->SendChatMessage(peer_id, ws.str());
	return true;
}

}

#endif

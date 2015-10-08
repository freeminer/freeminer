/*
rollback_interface.cpp
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
*/

/*
This file is part of Freeminer.

Freeminer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Freeminer  is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Freeminer.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "rollback_interface.h"
#include <sstream>
#include "util/serialize.h"
#include "util/string.h"
#include "util/numeric.h"
#include "map.h"
#include "gamedef.h"
#include "nodedef.h"
#include "nodemetadata.h"
#include "exceptions.h"
#include "log.h"
#include "inventorymanager.h"
#include "inventory.h"
#include "mapblock.h"

#define PP(x) "("<<(x).X<<","<<(x).Y<<","<<(x).Z<<")"


RollbackNode::RollbackNode(Map *map, v3s16 p, IGameDef *gamedef)
{
	INodeDefManager *ndef = gamedef->ndef();
	MapNode n = map->getNodeNoEx(p);
	name = ndef->get(n).name;
	param1 = n.param1;
	param2 = n.param2;
	NodeMetadata *metap = map->getNodeMetadata(p);
	if (metap) {
		std::ostringstream os(std::ios::binary);
		metap->serialize(os);
		meta = os.str();
	}
}


std::string RollbackAction::toString() const
{
	std::ostringstream os(std::ios::binary);
	switch (type) {
	case TYPE_SET_NODE:
		os << "set_node " << PP(p);
		os << ": (" << serializeJsonString(n_old.name);
		os << ", " << itos(n_old.param1);
		os << ", " << itos(n_old.param2);
		os << ", " << serializeJsonString(n_old.meta);
		os << ") -> (" << serializeJsonString(n_new.name);
		os << ", " << itos(n_new.param1);
		os << ", " << itos(n_new.param2);
		os << ", " << serializeJsonString(n_new.meta);
		os << ')';
	case TYPE_MODIFY_INVENTORY_STACK:
		os << "modify_inventory_stack (";
		os << serializeJsonString(inventory_location);
		os << ", " << serializeJsonString(inventory_list);
		os << ", " << inventory_index;
		os << ", " << (inventory_add ? "add" : "remove");
		os << ", " << serializeJsonString(inventory_stack.getItemString());
		os << ')';
	default:
		return "<unknown action>";
	}
	return os.str();
}


bool RollbackAction::isImportant(IGameDef *gamedef) const
{
	if (type != TYPE_SET_NODE)
		return true;
	// If names differ, action is always important
	if(n_old.name != n_new.name)
		return true;
	// If metadata differs, action is always important
	if(n_old.meta != n_new.meta)
		return true;
	INodeDefManager *ndef = gamedef->ndef();
	// Both are of the same name, so a single definition is needed
	const ContentFeatures &def = ndef->get(n_old.name);
	// If the type is flowing liquid, action is not important
	if (def.liquid_type == LIQUID_FLOWING)
		return false;
	// Otherwise action is important
	return true;
}


bool RollbackAction::getPosition(v3s16 *dst) const
{
	switch (type) {
	case TYPE_SET_NODE:
		if (dst) *dst = p;
		return true;
	case TYPE_MODIFY_INVENTORY_STACK: {
		InventoryLocation loc;
		loc.deSerialize(inventory_location);
		if (loc.type != InventoryLocation::NODEMETA) {
			return false;
		}
		if (dst) *dst = loc.p;
		return true; }
	default:
		return false;
	}
}


bool RollbackAction::applyRevert(Map *map, InventoryManager *imgr, IGameDef *gamedef) const
{
	try {
		switch (type) {
		case TYPE_NOTHING:
			return true;
		case TYPE_SET_NODE: {
			INodeDefManager *ndef = gamedef->ndef();
			// Make sure position is loaded from disk
			map->emergeBlock(getContainerPos(p, MAP_BLOCKSIZE), false);
			// Check current node
			MapNode current_node = map->getNodeNoEx(p);
			std::string current_name = ndef->get(current_node).name;
			// If current node not the new node, it's bad
			if (current_name != n_new.name) {
				return false;
			}
			// Create rollback node
			MapNode n(ndef, n_old.name, n_old.param1, n_old.param2);
			// Set rollback node
			try {
				if (!map->addNodeWithEvent(p, n)) {
					infostream << "RollbackAction::applyRevert(): "
						<< "AddNodeWithEvent failed at "
						<< PP(p) << " for " << n_old.name
						<< std::endl;
					return false;
				}
				if (n_old.meta.empty()) {
					map->removeNodeMetadata(p);
				} else {
					NodeMetadata *meta = map->getNodeMetadata(p);
					if (!meta) {
						meta = new NodeMetadata(gamedef->idef());
						if (!map->setNodeMetadata(p, meta)) {
							delete meta;
							infostream << "RollbackAction::applyRevert(): "
								<< "setNodeMetadata failed at "
								<< PP(p) << " for " << n_old.name
								<< std::endl;
							return false;
						}
					}
					std::istringstream is(n_old.meta, std::ios::binary);
					meta->deSerialize(is);
				}
				// Inform other things that the meta data has changed
				v3s16 blockpos = getContainerPos(p, MAP_BLOCKSIZE);
				MapEditEvent event;
				event.type = MEET_BLOCK_NODE_METADATA_CHANGED;
				event.p = blockpos;
				map->dispatchEvent(&event);
				// Set the block to be saved
				MapBlock *block = map->getBlockNoCreateNoEx(blockpos);
				if (block) {
					block->raiseModified(MOD_STATE_WRITE_NEEDED,
						MOD_REASON_REPORT_META_CHANGE);
				}
			} catch (InvalidPositionException &e) {
				infostream << "RollbackAction::applyRevert(): "
					<< "InvalidPositionException: " << e.what()
					<< std::endl;
				return false;
			}
			// Success
			return true; }
		case TYPE_MODIFY_INVENTORY_STACK: {
			InventoryLocation loc;
			loc.deSerialize(inventory_location);
			std::string real_name = gamedef->idef()->getAlias(inventory_stack.name);
			Inventory *inv = imgr->getInventory(loc);
			if (!inv) {
				infostream << "RollbackAction::applyRevert(): Could not get "
					"inventory at " << inventory_location << std::endl;
				return false;
			}
			InventoryList *list = inv->getList(inventory_list);
			if (!list) {
				infostream << "RollbackAction::applyRevert(): Could not get "
					"inventory list \"" << inventory_list << "\" in "
					<< inventory_location << std::endl;
				return false;
			}
			if (list->getSize() <= inventory_index) {
				infostream << "RollbackAction::applyRevert(): List index "
					<< inventory_index << " too large in "
					<< "inventory list \"" << inventory_list << "\" in "
					<< inventory_location << std::endl;
				return false;
			}
			// If item was added, take away item, otherwise add removed item
			if (inventory_add) {
				// Silently ignore different current item
				if (list->getItem(inventory_index).name != real_name)
					return false;
				list->takeItem(inventory_index, inventory_stack.count);
			} else {
				list->addItem(inventory_index, inventory_stack);
			}
			// Inventory was modified; send to clients
			imgr->setInventoryModified(loc);
			return true; }
		default:
			errorstream << "RollbackAction::applyRevert(): type not handled"
				<< std::endl;
			return false;
		}
	} catch(SerializationError &e) {
		errorstream << "RollbackAction::applyRevert(): n_old.name=" << n_old.name
				<< ", SerializationError: " << e.what() << std::endl;
	}
	return false;
}


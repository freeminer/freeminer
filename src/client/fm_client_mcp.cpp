/*
Copyright (C) 2026 proller <proler@gmail.com>
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

#include "client.h"
#include "itemdef.h"
#include "client/inputhandler.h"

#include "config.h"
#if USE_CLIENT_MCP

#include <boost/asio/ip/address.hpp>
#include <boost/asio/post.hpp>
#include <websocketpp/http/constants.hpp>

#include "chat.h"
#include "chatmessage.h"
#include "client/localplayer.h"
#include "client/clientobject.h"
#include "client/content_cao.h"
#include "clientmap.h"
#include "constants.h"
#include "inventory.h"
#include "inventorymanager.h"
#include "irr_v3d.h"
#include "log.h"
#include "mapblock.h"
#include "mapnode.h"
#include "nodedef.h"
#include "settings.h"
#include "util/numeric.h"
#include "util/pointedthing.h"
#include "version.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <random>
#include <sstream>
#include <unordered_map>

static bool getMCPGameKey(const std::string &name, GameKeyType &key)
{
	static const std::unordered_map<std::string, GameKeyType> key_map = {
			{"FORWARD", KeyType::FORWARD},
			{"BACKWARD", KeyType::BACKWARD},
			{"LEFT", KeyType::LEFT},
			{"RIGHT", KeyType::RIGHT},
			{"JUMP", KeyType::JUMP},
			{"AUX1", KeyType::AUX1},
			{"SNEAK", KeyType::SNEAK},
			{"AUTOFORWARD", KeyType::AUTOFORWARD},
			{"DIG", KeyType::DIG},
			{"PLACE", KeyType::PLACE},
			{"ESC", KeyType::ESC},
			{"CAMERA_YAW_LEFT", KeyType::CAMERA_YAW_LEFT},
			{"CAMERA_YAW_RIGHT", KeyType::CAMERA_YAW_RIGHT},
			{"CAMERA_PITCH_UP", KeyType::CAMERA_PITCH_UP},
			{"CAMERA_PITCH_DOWN", KeyType::CAMERA_PITCH_DOWN},
			{"DROP", KeyType::DROP},
			{"INVENTORY", KeyType::INVENTORY},
			{"CHAT", KeyType::CHAT},
			{"CMD", KeyType::CMD},
			{"CMD_LOCAL", KeyType::CMD_LOCAL},
			{"CONSOLE", KeyType::CONSOLE},
			{"MINIMAP", KeyType::MINIMAP},
			{"FREEMOVE", KeyType::FREEMOVE},
			{"PITCHMOVE", KeyType::PITCHMOVE},
			{"FASTMOVE", KeyType::FASTMOVE},
			{"NOCLIP", KeyType::NOCLIP},
			{"HOTBAR_PREV", KeyType::HOTBAR_PREV},
			{"HOTBAR_NEXT", KeyType::HOTBAR_NEXT},
			{"MUTE", KeyType::MUTE},
			{"INC_VOLUME", KeyType::INC_VOLUME},
			{"DEC_VOLUME", KeyType::DEC_VOLUME},
			{"CINEMATIC", KeyType::CINEMATIC},
			{"SCREENSHOT", KeyType::SCREENSHOT},
			{"TOGGLE_BLOCK_BOUNDS", KeyType::TOGGLE_BLOCK_BOUNDS},
			{"TOGGLE_HUD", KeyType::TOGGLE_HUD},
			{"TOGGLE_CHAT", KeyType::TOGGLE_CHAT},
			{"TOGGLE_FOG", KeyType::TOGGLE_FOG},
			{"TOGGLE_UPDATE_CAMERA", KeyType::TOGGLE_UPDATE_CAMERA},
			{"TOGGLE_DEBUG", KeyType::TOGGLE_DEBUG},
			{"TOGGLE_PROFILER", KeyType::TOGGLE_PROFILER},
			{"CAMERA_MODE", KeyType::CAMERA_MODE},
			{"INCREASE_VIEWING_RANGE", KeyType::INCREASE_VIEWING_RANGE},
			{"DECREASE_VIEWING_RANGE", KeyType::DECREASE_VIEWING_RANGE},
			{"RANGESELECT", KeyType::RANGESELECT},
			{"ZOOM", KeyType::ZOOM},
			{"QUICKTUNE_NEXT", KeyType::QUICKTUNE_NEXT},
			{"QUICKTUNE_PREV", KeyType::QUICKTUNE_PREV},
			{"QUICKTUNE_INC", KeyType::QUICKTUNE_INC},
			{"QUICKTUNE_DEC", KeyType::QUICKTUNE_DEC},
			{"PLAYERLIST", KeyType::PLAYERLIST},
	};
	if (const auto it = key_map.find(name); it != key_map.end()) {
		key = it->second;
		return true;
	}
	for (int slot = 1; slot <= 32; ++slot) {
		if (name == "SLOT_" + std::to_string(slot)) {
			key = static_cast<GameKeyType>(KeyType::SLOT_1 + slot - 1);
			return true;
		}
	}
	return false;
}

static Json::Value makeMCPObjectSchema()
{
	Json::Value schema;
	schema["type"] = "object";
	schema["properties"] = Json::Value(Json::objectValue);
	return schema;
}

static void addMCPSchemaProperty(
		Json::Value &schema, const char *name, const char *type, const char *description)
{
	schema["properties"][name]["type"] = type;
	schema["properties"][name]["description"] = description;
}

static void addMCPRequired(Json::Value &schema, const char *name)
{
	schema["required"].append(name);
}

static Json::Value makeMCPTool(const char *name, const char *description,
		const Json::Value &input_schema = makeMCPObjectSchema())
{
	Json::Value tool;
	tool["name"] = name;
	tool["description"] = description;
	tool["inputSchema"] = input_schema;
	return tool;
}

static Json::Value makeMCPPositionSchema()
{
	Json::Value schema = makeMCPObjectSchema();
	addMCPSchemaProperty(schema, "x", "integer", "World X position in nodes.");
	addMCPSchemaProperty(schema, "y", "integer", "World Y position in nodes.");
	addMCPSchemaProperty(schema, "z", "integer", "World Z position in nodes.");
	addMCPRequired(schema, "x");
	addMCPRequired(schema, "y");
	addMCPRequired(schema, "z");
	return schema;
}

static Json::Value makeMCPAreaSchema()
{
	Json::Value schema = makeMCPObjectSchema();
	addMCPSchemaProperty(
			schema, "min_x", "integer", "Minimum world X position in nodes.");
	addMCPSchemaProperty(
			schema, "min_y", "integer", "Minimum world Y position in nodes.");
	addMCPSchemaProperty(
			schema, "min_z", "integer", "Minimum world Z position in nodes.");
	addMCPSchemaProperty(
			schema, "max_x", "integer", "Maximum world X position in nodes.");
	addMCPSchemaProperty(
			schema, "max_y", "integer", "Maximum world Y position in nodes.");
	addMCPSchemaProperty(
			schema, "max_z", "integer", "Maximum world Z position in nodes.");
	addMCPRequired(schema, "min_x");
	addMCPRequired(schema, "min_y");
	addMCPRequired(schema, "min_z");
	addMCPRequired(schema, "max_x");
	addMCPRequired(schema, "max_y");
	addMCPRequired(schema, "max_z");
	return schema;
}

static Json::Value nodeToMCPJson(
		v3pos_t pos, MapNode node, bool is_valid, const NodeDefManager *ndef)
{
	Json::Value node_obj;
	node_obj["pos"]["x"] = pos.X;
	node_obj["pos"]["y"] = pos.Y;
	node_obj["pos"]["z"] = pos.Z;
	node_obj["valid"] = is_valid;
	node_obj["content"] = static_cast<int>(node.getContent());
	node_obj["param1"] = static_cast<int>(node.param1);
	node_obj["param2"] = static_cast<int>(node.param2);

	const ContentFeatures &features = ndef->get(node);
	node_obj["name"] = features.name;
	node_obj["walkable"] = features.walkable;
	node_obj["diggable"] = features.diggable;
	node_obj["buildable_to"] = features.buildable_to;
	node_obj["rightclickable"] = features.rightclickable;
	node_obj["liquid"] = features.isLiquid();
	node_obj["pointable"] = (int)features.pointable;
	return node_obj;
}

static void setMCPTextResult(
		Json::Value &response, const Json::Value &value, bool is_error = false)
{
	Json::Value content_array(Json::arrayValue);
	Json::Value content_item;
	content_item["type"] = "text";
	content_item["text"] = Json::writeString(Json::StreamWriterBuilder(), value);
	content_array.append(content_item);

	Json::Value result;
	result["content"] = content_array;
	if (is_error)
		result["isError"] = true;
	response["result"] = result;
}

static void setMCPStatusResult(Json::Value &response, const Json::Value &status)
{
	setMCPTextResult(
			response, status, status.isMember("success") && !status["success"].asBool());
}

static void setMCPError(Json::Value &response, int code, const std::string &message)
{
	Json::Value error_obj;
	error_obj["code"] = code;
	error_obj["message"] = message;
	response["error"] = error_obj;
}

static bool selectMCPWieldedItem(
		Client *client, LocalPlayer *player, const Json::Value &args, Json::Value &status)
{
	if (!player) {
		status["success"] = false;
		status["error"] = "No local player";
		return false;
	}

	const InventoryList *mainlist = player->inventory.getList("main");
	const u16 hotbar_size = player->getMaxHotbarItemcount();
	status["hotbar_size"] = static_cast<int>(hotbar_size);

	if (!mainlist || hotbar_size == 0) {
		status["success"] = false;
		status["error"] = "Player hotbar is not available";
		return false;
	}

	if (args.isMember("slot")) {
		int slot = args["slot"].asInt();
		if (slot < 0 || slot >= (int)hotbar_size) {
			status["success"] = false;
			status["error"] = "Hotbar slot is out of range";
			return false;
		}

		client->setPlayerItem((u16)slot);
		client->pressMCPKey(static_cast<int>(GameKeyType::SLOT_1) + slot);
		status["success"] = true;
		status["slot"] = static_cast<int>(slot);
		status["item"] = mainlist->getItem(slot).getItemString();
		return true;
	}

	if (!args.isMember("item")) {
		u16 slot = player->getWieldIndex();
		if (slot >= mainlist->getSize()) {
			status["success"] = false;
			status["error"] = "Current wield slot is out of range";
			return false;
		}
		status["success"] = true;
		status["slot"] = slot;
		status["item"] = mainlist->getItem(slot).getItemString();
		return true;
	}

	std::string item_name = args["item"].asString();
	for (u16 i = 0; i < hotbar_size; i++) {
		const ItemStack &stack = mainlist->getItem(i);
		if (!stack.empty() && stack.name == item_name) {
			client->setPlayerItem(i);
			client->pressMCPKey(static_cast<int>(GameKeyType::SLOT_1) + i);
			status["success"] = true;
			status["slot"] = static_cast<int>(i);
			status["item"] = stack.getItemString();
			return true;
		}
	}

	status["success"] = false;
	status["error"] = "Item is not present in the hotbar";
	status["item"] = item_name;
	return false;
}

static bool makeMCPPlacePointedThing(Client *client, v3pos_t target,
		const Json::Value &args, PointedThing &pointed, Json::Value &status)
{
	static const v3pos_t dirs[] = {
			v3pos_t(0, -1, 0),
			v3pos_t(0, 1, 0),
			v3pos_t(-1, 0, 0),
			v3pos_t(1, 0, 0),
			v3pos_t(0, 0, -1),
			v3pos_t(0, 0, 1),
	};

	ClientMap &map = client->getEnv().getClientMap();
	const NodeDefManager *ndef = client->getNodeDefManager();
	auto try_under = [&](const auto &under) -> bool {
		auto normal_i = target - under;
		if (std::abs(normal_i.X) + std::abs(normal_i.Y) + std::abs(normal_i.Z) != 1)
			return false;

		bool under_ok = false;
		MapNode under_node = map.getNode(under, &under_ok);
		if (!under_ok)
			return false;

		const ContentFeatures &under_features = ndef->get(under_node);
		if (under_features.buildable_to ||
				under_features.pointable == PointabilityType::POINTABLE_NOT)
			return false;

		v3f normal((f32)normal_i.X, (f32)normal_i.Y, (f32)normal_i.Z);
		auto point = intToFloat(under, BS) + v3fToOpos(normal * (BS * 0.5f));
		LocalPlayer *player = client->getEnv().getLocalPlayer();
		f32 distance_sq = player ? player->getPosition().getDistanceFromSQ(point) : 0.0f;
		pointed = PointedThing(under, target, under, point, normal, 0, distance_sq,
				under_features.pointable);

		status["under"]["x"] = under.X;
		status["under"]["y"] = under.Y;
		status["under"]["z"] = under.Z;
		status["under_name"] = under_features.name;
		return true;
	};

	if (args.isMember("under_x") && args.isMember("under_y") &&
			args.isMember("under_z")) {
		v3pos_t under(args["under_x"].asInt(), args["under_y"].asInt(),
				args["under_z"].asInt());
		if (try_under(under))
			return true;

		status["success"] = false;
		status["error"] = "Specified under node is not adjacent and pointable";
		return false;
	}

	for (const auto &dir : dirs) {
		if (try_under(target + dir))
			return true;
	}

	status["success"] = false;
	status["error"] = "No adjacent pointable support node found";
	return false;
}

static PointedThing makeMCPNodePointedThing(
		Client *client, v3pos_t pos, const ContentFeatures &features)
{
	LocalPlayer *player = client->getEnv().getLocalPlayer();
	auto point = intToFloat(pos, BS);
	v3f normal(0.0f, 1.0f, 0.0f);
	f32 distance_sq = player ? player->getPosition().getDistanceFromSQ(point) : 0.0f;
	return PointedThing(pos, pos, pos, point, normal, 0, distance_sq, features.pointable);
}

static void setMCPCameraTarget(Client *client, const v3opos_t &target)
{
	LocalPlayer *player = client->getEnv().getLocalPlayer();
	if (!player)
		return;

	const auto eye = player->getEyePosition();
	const auto delta = target - eye;
	const auto horizontal = std::sqrt(delta.X * delta.X + delta.Z * delta.Z);
	if (delta.getLengthSQ() < 0.0001f)
		return;

	constexpr f32 rad_to_deg = 180.0f / static_cast<f32>(M_PI);
	const f32 pitch = -std::atan2(delta.Y, horizontal) * rad_to_deg;
	const f32 yaw = -std::atan2(delta.X, delta.Z) * rad_to_deg;
	client->setMCPRotationTarget(pitch, yaw);
}

static void setMCPCameraTarget(Client *client, v3pos_t pos)
{
	setMCPCameraTarget(client, intToFloat(pos, BS));
}

// MCP world actions should use normal movement and stay inside the server's
// interaction reach. The caller retries the action after the short movement
// pulse; that lets normal physics and collision determine whether the target
// can actually be reached.
static bool approachMCPInteractionTarget(
		Client *client, const v3opos_t &target, Json::Value &status)
{
	LocalPlayer *player = client->getEnv().getLocalPlayer();
	if (!player) {
		status["success"] = false;
		status["error"] = "No local player";
		return false;
	}

	const auto position = player->getPosition();
	const auto delta = target - position;
	const auto distance = delta.getLength() / BS;
	constexpr f32 interaction_reach = 3.5f;
	if (distance <= interaction_reach)
		return true;

	setMCPCameraTarget(client, target);
	constexpr f32 rad_to_deg = 180.0f / static_cast<f32>(M_PI);
	PlayerControl control = player->control;
	control.yaw = -std::atan2(delta.X, delta.Z) * rad_to_deg;
	control.up = 1.0f;
	control.down = 0.0f;
	control.left = 0.0f;
	control.right = 0.0f;
	control.dig = false;
	control.place = false;
	control.setMovementFromKeys();
	client->setMCPPlayerControl(control, 750);
	status["success"] = true;
	status["state"] = "approaching";
	status["distance_nodes"] = distance;
	status["interaction_reach_nodes"] = interaction_reach;
	status["note"] =
			"Walking toward target with normal controls. Retry the interaction after movement.";
	return false;
}

static bool approachMCPInteractionNode(Client *client, v3pos_t pos, Json::Value &status)
{
	return approachMCPInteractionTarget(client, intToFloat(pos, BS), status);
}

static Json::Value inventoryListToMCPJson(const InventoryList *list)
{
	Json::Value list_obj;
	if (!list)
		return list_obj;

	list_obj["name"] = list->getName();
	list_obj["size"] = static_cast<int>(list->getSize());
	list_obj["width"] = static_cast<int>(list->getWidth());
	Json::Value items(Json::arrayValue);
	for (u32 i = 0; i < list->getSize(); i++) {
		const ItemStack &stack = list->getItem(i);
		Json::Value item;
		item["index"] = static_cast<int>(i);
		item["empty"] = stack.empty();
		item["name"] = stack.name;
		item["count"] = static_cast<int>(stack.count);
		item["wear"] = static_cast<int>(stack.wear);
		item["itemstring"] = stack.getItemString();
		items.append(item);
	}
	list_obj["items"] = items;
	return list_obj;
}

static Json::Value chatLineToMCPJson(const ChatLine &line, u32 index)
{
	Json::Value line_obj;
	line_obj["index"] = static_cast<int>(index);
	line_obj["age"] = line.age;
	line_obj["name"] = wide_to_utf8(line.name.getString());
	line_obj["text"] = wide_to_utf8(line.text.getString());
	if (!line.name.empty())
		line_obj["formatted"] =
				"<" + line_obj["name"].asString() + "> " + line_obj["text"].asString();
	else
		line_obj["formatted"] = line_obj["text"].asString();
	return line_obj;
}

static Json::Value chatBufferToMCPJson(const ChatBuffer &buffer, int count)
{
	Json::Value chat_obj;
	Json::Value messages(Json::arrayValue);
	const u32 total = buffer.getLineCount();

	if (count < 0 || count > static_cast<int>(total))
		count = static_cast<int>(total);

	const u32 start = total - static_cast<u32>(count);
	for (u32 i = start; i < total; i++)
		messages.append(chatLineToMCPJson(buffer.getLine(i), i));

	chat_obj["success"] = true;
	chat_obj["total"] = static_cast<int>(total);
	chat_obj["messages"] = messages;
	return chat_obj;
}

static const char *mcpChatMessageTypeName(ChatMessageType type)
{
	switch (type) {
	case CHATMESSAGE_TYPE_RAW:
		return "raw";
	case CHATMESSAGE_TYPE_NORMAL:
		return "player";
	case CHATMESSAGE_TYPE_ANNOUNCE:
		return "announce";
	case CHATMESSAGE_TYPE_SYSTEM:
		return "system";
	default:
		return "unknown";
	}
}

void Client::recordMCPChatMessage(const ChatMessage &message)
{
	Json::Value item;
	item["id"] = Json::UInt64(m_mcp_chat_next_id++);
	item["type"] = mcpChatMessageTypeName(message.type);
	item["sender"] = wide_to_utf8(unescape_enriched(message.sender));
	item["text"] = wide_to_utf8(unescape_enriched(message.message));
	item["timestamp"] = Json::Int64(message.timestamp);
	if (!item["sender"].asString().empty())
		item["formatted"] =
				"<" + item["sender"].asString() + "> " + item["text"].asString();
	else
		item["formatted"] = item["text"];

	m_mcp_chat_history.push_back(std::move(item));
	while (m_mcp_chat_history.size() > 1000)
		m_mcp_chat_history.pop_front();
}

Json::Value Client::getMCPChatMessages(u64 after_id, u32 count) const
{
	count = rangelim<u32>(count, 1, 200);
	Json::Value result;
	Json::Value messages(Json::arrayValue);
	size_t start = 0;

	if (after_id == 0 && m_mcp_chat_history.size() > count) {
		start = m_mcp_chat_history.size() - count;
	} else if (after_id != 0) {
		while (start < m_mcp_chat_history.size() &&
				m_mcp_chat_history[start]["id"].asUInt64() <= after_id)
			start++;
	}

	size_t index = start;
	for (; index < m_mcp_chat_history.size() && messages.size() < count; index++)
		messages.append(m_mcp_chat_history[index]);

	result["success"] = true;
	result["messages"] = messages;
	result["returned"] = static_cast<Json::UInt>(messages.size());
	result["has_more"] = index < m_mcp_chat_history.size();
	result["oldest_id"] = m_mcp_chat_history.empty() ? Json::UInt64(0)
													 : m_mcp_chat_history.front()["id"];
	result["latest_id"] = m_mcp_chat_history.empty() ? Json::UInt64(0)
													 : m_mcp_chat_history.back()["id"];
	result["next_after_id"] = messages.empty() ? Json::UInt64(after_id)
											   : messages[messages.size() - 1]["id"];
	return result;
}

static bool validateMCPToolArguments(
		const std::string &tool, const Json::Value &args, std::string &error)
{
	auto require = [&](const char *name, Json::ValueType type) {
		if (!args.isMember(name)) {
			error = std::string(name) + " is required";
			return false;
		}
		if (args[name].type() != type) {
			error = std::string(name) + " has the wrong type";
			return false;
		}
		return true;
	};
	auto require_integer = [&](const char *name) {
		if (!args.isMember(name)) {
			error = std::string(name) + " is required";
			return false;
		}
		if (!(args[name].isInt64() || args[name].isUInt64())) {
			error = std::string(name) + " must be an integer";
			return false;
		}
		const bool out_of_range =
				args[name].isUInt64()
						? args[name].asUInt64() > static_cast<Json::UInt64>(
														  std::numeric_limits<s32>::max())
						: args[name].asInt64() < std::numeric_limits<s32>::min() ||
								  args[name].asInt64() > std::numeric_limits<s32>::max();
		if (out_of_range) {
			error = std::string(name) + " is out of range";
			return false;
		}
		return true;
	};
	auto optional_type = [&](const char *name, Json::ValueType type) {
		if (args.isMember(name) && args[name].type() != type) {
			error = std::string(name) + " has the wrong type";
			return false;
		}
		return true;
	};
	auto optional_integer = [&](const char *name) {
		if (args.isMember(name) && !(args[name].isInt64() || args[name].isUInt64())) {
			error = std::string(name) + " must be an integer";
			return false;
		}
		const bool out_of_range =
				args.isMember(name) &&
				(args[name].isUInt64()
								? args[name].asUInt64() >
										  static_cast<Json::UInt64>(
												  std::numeric_limits<s32>::max())
								: args[name].asInt64() <
												  std::numeric_limits<s32>::min() ||
										  args[name].asInt64() >
												  std::numeric_limits<s32>::max());
		if (out_of_range) {
			error = std::string(name) + " is out of range";
			return false;
		}
		return true;
	};
	auto require_position = [&]() {
		return require_integer("x") && require_integer("y") && require_integer("z");
	};

	if (tool == "get_player_state" || tool == "get_pointed_thing" ||
			tool == "get_nearby_objects" || tool == "stop_player_control")
		return true;
	if (tool == "get_inventory") {
		if (!optional_integer("node_x") || !optional_integer("node_y") ||
				!optional_integer("node_z"))
			return false;
		const int node_count = args.isMember("node_x") + args.isMember("node_y") +
							   args.isMember("node_z");
		if (node_count != 0 && node_count != 3) {
			error = "node_x, node_y and node_z must be provided together";
			return false;
		}
		for (const char *axis : {"x", "y", "z"}) {
			const std::string key = std::string("node_") + axis;
			if (args.isMember(key) &&
					(args[key].asInt64() < std::numeric_limits<s16>::min() ||
							args[key].asInt64() > std::numeric_limits<s16>::max())) {
				error = key + " is out of map coordinate range";
				return false;
			}
		}
		return true;
	}
	if (tool == "send_chat_message")
		return require("message", Json::stringValue);
	if (tool == "get_chat_messages")
		return optional_integer("count") &&
			   (!args.isMember("count") || args["count"].asInt64() >= 1) &&
			   optional_integer("after_id") &&
			   (!args.isMember("after_id") || args["after_id"].asInt64() >= 0) &&
			   optional_type("buffer", Json::stringValue);
	if (tool == "get_node" || tool == "dig_node" || tool == "move_player_to" ||
			tool == "teleport_player")
		return require_position();
	if (tool == "get_nodes_area")
		return require_integer("min_x") && require_integer("min_y") &&
			   require_integer("min_z") && require_integer("max_x") &&
			   require_integer("max_y") && require_integer("max_z");
	if (tool == "set_wielded_item")
		return optional_integer("slot") && optional_type("item", Json::stringValue);
	if (tool == "move_inventory_item") {
		if (!require_integer("from_index") || !require_integer("to_index") ||
				!optional_integer("count") ||
				!optional_type("from_list", Json::stringValue) ||
				!optional_type("to_list", Json::stringValue) ||
				!optional_integer("from_node_x") || !optional_integer("from_node_y") ||
				!optional_integer("from_node_z") || !optional_integer("to_node_x") ||
				!optional_integer("to_node_y") || !optional_integer("to_node_z"))
			return false;
		for (const char *prefix : {"from_node_", "to_node_"}) {
			const std::string p = prefix;
			const int node_count = args.isMember(p + "x") + args.isMember(p + "y") +
								   args.isMember(p + "z");
			if (node_count != 0 && node_count != 3) {
				error = p + "x, " + p + "y and " + p + "z must be provided together";
				return false;
			}
			for (const char *axis : {"x", "y", "z"}) {
				const std::string key = p + axis;
				if (args.isMember(key) &&
						(args[key].asInt64() < std::numeric_limits<s16>::min() ||
								args[key].asInt64() > std::numeric_limits<s16>::max())) {
					error = key + " is out of map coordinate range";
					return false;
				}
			}
		}
		if (args["from_index"].asInt64() < 0 || args["to_index"].asInt64() < 0 ||
				(args.isMember("count") && args["count"].asInt64() < 0)) {
			error = "inventory indices and count must be non-negative";
			return false;
		}
		return true;
	}
	if (tool == "craft" || tool == "get_world_content") {
		const char *field = tool == "craft" ? "count" : "radius";
		if (!optional_integer(field))
			return false;
		if (args.isMember(field) && args[field].asInt64() < 0) {
			error = std::string(field) + " must be non-negative";
			return false;
		}
		if (tool == "craft" && args.isMember(field) && args[field].asInt64() > 65535) {
			error = "count exceeds the supported craft count";
			return false;
		}
		return true;
	}
	if (tool == "use_item") {
		if (!optional_integer("slot") || !optional_integer("object_id") ||
				!optional_type("item", Json::stringValue) ||
				!optional_integer("node_x") || !optional_integer("node_y") ||
				!optional_integer("node_z"))
			return false;
		const int node_count = args.isMember("node_x") + args.isMember("node_y") +
							   args.isMember("node_z");
		if (node_count != 0 && node_count != 3) {
			error = "node_x, node_y and node_z must be provided together";
			return false;
		}
		for (const char *axis : {"x", "y", "z"}) {
			const std::string key = std::string("node_") + axis;
			if (args.isMember(key) &&
					(args[key].asInt64() < std::numeric_limits<s16>::min() ||
							args[key].asInt64() > std::numeric_limits<s16>::max())) {
				error = key + " is out of map coordinate range";
				return false;
			}
		}
		if (args.isMember("object_id") &&
				(args["object_id"].asInt64() <= 0 ||
						args["object_id"].asInt64() > std::numeric_limits<u16>::max())) {
			error = "object_id is out of range";
			return false;
		}
		return true;
	}
	if (tool == "interact_with_object" || tool == "punch_object") {
		if (!require_integer("object_id"))
			return false;
		if (args["object_id"].asInt64() <= 0 ||
				args["object_id"].asInt64() > std::numeric_limits<u16>::max()) {
			error = "object_id is out of range";
			return false;
		}
		return true;
	}
	if (tool == "place_node") {
		if (!require_position() || !optional_integer("slot") ||
				!optional_type("item", Json::stringValue) ||
				!optional_integer("under_x") || !optional_integer("under_y") ||
				!optional_integer("under_z"))
			return false;
		const int under_count = args.isMember("under_x") + args.isMember("under_y") +
								args.isMember("under_z");
		if (under_count != 0 && under_count != 3) {
			error = "under_x, under_y and under_z must be provided together";
			return false;
		}
		return true;
	}
	if (tool == "rotate_player") {
		if (!args.isMember("pitch") || !args["pitch"].isNumeric() ||
				!args.isMember("yaw") || !args["yaw"].isNumeric()) {
			error = "pitch and yaw must be numbers";
			return false;
		}
		if (!std::isfinite(args["pitch"].asDouble()) ||
				!std::isfinite(args["yaw"].asDouble())) {
			error = "pitch and yaw must be finite";
			return false;
		}
		return true;
	}
	if (tool == "look_at_position") {
		for (const char *axis : {"x", "y", "z"}) {
			const std::string key = axis;
			if (!args.isMember(key) || !args[key].isNumeric() ||
					!std::isfinite(args[key].asDouble())) {
				error = key + " must be a finite number";
				return false;
			}
		}
		return true;
	}
	if (tool == "look_at_object") {
		if (!require_integer("object_id"))
			return false;
		if (args["object_id"].asInt64() <= 0 ||
				args["object_id"].asInt64() > std::numeric_limits<u16>::max()) {
			error = "object_id is out of range";
			return false;
		}
		return true;
	}
	if (tool == "set_player_control") {
		for (const char *name : {"forward", "backward", "left", "right", "jump", "sneak",
					 "dig", "place", "aux1", "zoom"}) {
			if (!optional_type(name, Json::booleanValue))
				return false;
		}
		for (const char *name : {"pitch", "yaw"}) {
			if (args.isMember(name) &&
					(!args[name].isNumeric() || !std::isfinite(args[name].asDouble()))) {
				error = std::string(name) + " must be a finite number";
				return false;
			}
		}
		if (!optional_integer("duration_ms"))
			return false;
		if (args.isMember("duration_ms") && args["duration_ms"].asInt64() < 0) {
			error = "duration_ms must be non-negative";
			return false;
		}
		if (!args.isMember("forward") && !args.isMember("backward") &&
				!args.isMember("left") && !args.isMember("right") &&
				!args.isMember("jump") && !args.isMember("sneak") &&
				!args.isMember("dig") && !args.isMember("place") &&
				!args.isMember("aux1") && !args.isMember("zoom") &&
				!args.isMember("pitch") && !args.isMember("yaw")) {
			error = "at least one control field is required";
			return false;
		}
		return true;
	}
	if (tool == "press_keys") {
		if (!args.isMember("keys") || !args["keys"].isArray() || args["keys"].empty()) {
			error = "keys must be a non-empty array of game key names";
			return false;
		}
		if (!optional_integer("duration_ms") ||
				(args.isMember("duration_ms") &&
						(args["duration_ms"].asInt64() < 50 ||
								args["duration_ms"].asInt64() > 5000))) {
			error = "duration_ms must be between 50 and 5000";
			return false;
		}
		for (const auto &value : args["keys"]) {
			GameKeyType key;
			if (!value.isString() || !getMCPGameKey(value.asString(), key)) {
				error = "keys contains an unknown game key name";
				return false;
			}
		}
		return true;
	}

	// Unknown tools are handled by tools/call with an MCP invalid-params error.
	return true;
}

void Client::handleMCPMessage(mcp_ws_server_t::connection_ptr connection,
		const Json::Value &request, const std::string &session_id)
{
	Json::Value response;
	response["jsonrpc"] = "2.0";
	response["id"] = request.isMember("id") ? request["id"] : Json::Value();

	try {
		std::string method = request["method"].asString();

		if (method == "initialize") {
			Json::Value result;
			const std::string requested_version =
					request["params"].get("protocolVersion", "").asString();
			if (requested_version == "2024-11-05" || requested_version == "2025-03-26" ||
					requested_version == "2025-06-18")
				result["protocolVersion"] = requested_version;
			else
				result["protocolVersion"] = "2025-06-18";
			result["serverInfo"]["name"] = "freeminer";
			result["serverInfo"]["version"] = VERSION_STRING;
			result["capabilities"]["tools"] = Json::Value(Json::objectValue);
			response["result"] = result;
		} else if (method == "tools/list") {
			Json::Value tools(Json::arrayValue);

			tools.append(makeMCPTool("get_player_state",
					"Get the current player state including position, velocity, health and breath."));
			Json::Value inventory_schema = makeMCPObjectSchema();
			for (const char *axis : {"x", "y", "z"})
				addMCPSchemaProperty(inventory_schema,
						(std::string("node_") + axis).c_str(), "integer",
						"Optional nearby node inventory coordinate.");
			tools.append(makeMCPTool("get_inventory",
					"Get player inventory, or a nearby node inventory when node_x/y/z are provided.",
					inventory_schema));

			Json::Value send_chat_schema = makeMCPObjectSchema();
			addMCPSchemaProperty(send_chat_schema, "message", "string",
					"Public chat text to send as the local player. Commands may start with '/'.");
			addMCPRequired(send_chat_schema, "message");
			tools.append(makeMCPTool("send_chat_message",
					"Speak to other players in public chat as the local player, or issue a slash command.",
					send_chat_schema));

			Json::Value get_chat_schema = makeMCPObjectSchema();
			addMCPSchemaProperty(get_chat_schema, "count", "integer",
					"Maximum number of newest messages to return.");
			addMCPSchemaProperty(get_chat_schema, "buffer", "string",
					"Source to read: 'history' (structured MCP history), 'recent', or 'console'.");
			addMCPSchemaProperty(get_chat_schema, "after_id", "integer",
					"For history, return only messages newer than this message ID.");
			tools.append(makeMCPTool("get_chat_messages",
					"Read structured messages from other players and the server. Use next_after_id for polling.",
					get_chat_schema));

			Json::Value control_schema = makeMCPObjectSchema();
			addMCPSchemaProperty(control_schema, "forward", "boolean", "Hold forward.");
			addMCPSchemaProperty(control_schema, "backward", "boolean", "Hold backward.");
			addMCPSchemaProperty(control_schema, "left", "boolean", "Hold left.");
			addMCPSchemaProperty(control_schema, "right", "boolean", "Hold right.");
			addMCPSchemaProperty(control_schema, "jump", "boolean", "Hold jump.");
			addMCPSchemaProperty(control_schema, "sneak", "boolean", "Hold sneak.");
			addMCPSchemaProperty(control_schema, "dig", "boolean", "Hold dig.");
			addMCPSchemaProperty(control_schema, "place", "boolean", "Hold place.");
			addMCPSchemaProperty(control_schema, "aux1", "boolean", "Hold aux1.");
			addMCPSchemaProperty(control_schema, "zoom", "boolean", "Hold zoom.");
			addMCPSchemaProperty(control_schema, "pitch", "number", "Camera pitch.");
			addMCPSchemaProperty(control_schema, "yaw", "number", "Camera yaw.");
			addMCPSchemaProperty(control_schema, "duration_ms", "integer",
					"How long to hold the control override.");
			tools.append(makeMCPTool("set_player_control",
					"Temporarily set player movement and action controls.",
					control_schema));
			Json::Value keys_schema = makeMCPObjectSchema();
			Json::Value key_names(Json::arrayValue);
			for (const char *name : {"FORWARD", "BACKWARD", "LEFT", "RIGHT", "JUMP",
						 "AUX1", "SNEAK", "AUTOFORWARD", "DIG", "PLACE", "ESC",
						 "CAMERA_YAW_LEFT", "CAMERA_YAW_RIGHT", "CAMERA_PITCH_UP",
						 "CAMERA_PITCH_DOWN", "DROP", "INVENTORY", "CHAT", "CMD",
						 "CMD_LOCAL", "CONSOLE", "MINIMAP", "FREEMOVE", "PITCHMOVE",
						 "FASTMOVE", "NOCLIP", "HOTBAR_PREV", "HOTBAR_NEXT", "MUTE",
						 "INC_VOLUME", "DEC_VOLUME", "CINEMATIC", "SCREENSHOT",
						 "TOGGLE_BLOCK_BOUNDS", "TOGGLE_HUD", "TOGGLE_CHAT", "TOGGLE_FOG",
						 "TOGGLE_UPDATE_CAMERA", "TOGGLE_DEBUG", "TOGGLE_PROFILER",
						 "CAMERA_MODE", "INCREASE_VIEWING_RANGE",
						 "DECREASE_VIEWING_RANGE", "RANGESELECT", "ZOOM",
						 "QUICKTUNE_NEXT", "QUICKTUNE_PREV", "QUICKTUNE_INC",
						 "QUICKTUNE_DEC", "PLAYERLIST", "SLOT_1", "SLOT_2", "SLOT_3",
						 "SLOT_4", "SLOT_5", "SLOT_6", "SLOT_7", "SLOT_8", "SLOT_9",
						 "SLOT_10", "SLOT_11", "SLOT_12", "SLOT_13", "SLOT_14", "SLOT_15",
						 "SLOT_16", "SLOT_17", "SLOT_18", "SLOT_19", "SLOT_20", "SLOT_21",
						 "SLOT_22", "SLOT_23", "SLOT_24", "SLOT_25", "SLOT_26", "SLOT_27",
						 "SLOT_28", "SLOT_29", "SLOT_30", "SLOT_31", "SLOT_32"})
				key_names.append(name);
			Json::Value key_items;
			key_items["type"] = "string";
			key_items["enum"] = key_names;
			addMCPSchemaProperty(keys_schema, "keys", "array",
					"Game action keys to press through the normal input handler.");
			keys_schema["properties"]["keys"]["items"] = key_items;
			addMCPSchemaProperty(keys_schema, "duration_ms", "integer",
					"How long to hold each key (50 to 5000 ms).");
			addMCPRequired(keys_schema, "keys");
			tools.append(makeMCPTool("press_keys",
					"Press mapped game actions (movement, UI, hotbar, camera, and toggles) briefly.",
					keys_schema));
			tools.append(makeMCPTool("stop_player_control",
					"Immediately release the active MCP control override."));

			tools.append(makeMCPTool("get_node",
					"Get node data from the local client map at a world position. "
					"After edits, this cached view may lag behind the server.",
					makeMCPPositionSchema()));
			tools.append(makeMCPTool("get_nodes_area",
					"Get node data from the local client map for a bounded world area. "
					"After edits, this cached view may lag behind the server.",
					makeMCPAreaSchema()));

			Json::Value wield_schema = makeMCPObjectSchema();
			addMCPSchemaProperty(
					wield_schema, "slot", "integer", "Hotbar slot to wield.");
			addMCPSchemaProperty(wield_schema, "item", "string",
					"Item name to find and wield from the hotbar.");
			tools.append(makeMCPTool("set_wielded_item",
					"Wield a hotbar slot or item name.", wield_schema));

			Json::Value move_inv_schema = makeMCPObjectSchema();
			addMCPSchemaProperty(
					move_inv_schema, "from_list", "string", "Source list name.");
			addMCPSchemaProperty(
					move_inv_schema, "from_index", "integer", "Source stack index.");
			addMCPSchemaProperty(
					move_inv_schema, "to_list", "string", "Destination list name.");
			addMCPSchemaProperty(
					move_inv_schema, "to_index", "integer", "Destination stack index.");
			addMCPSchemaProperty(
					move_inv_schema, "count", "integer", "Count to move, or 0 for all.");
			for (const char *prefix : {"from_node_", "to_node_"}) {
				for (const char *axis : {"x", "y", "z"})
					addMCPSchemaProperty(move_inv_schema,
							(std::string(prefix) + axis).c_str(), "integer",
							"Optional node inventory coordinate.");
			}
			addMCPRequired(move_inv_schema, "from_index");
			addMCPRequired(move_inv_schema, "to_index");
			tools.append(makeMCPTool("move_inventory_item",
					"Move an item stack between player inventory and a nearby node inventory.",
					move_inv_schema));

			Json::Value craft_schema = makeMCPObjectSchema();
			addMCPSchemaProperty(
					craft_schema, "count", "integer", "Craft count, or 0 for all.");
			tools.append(makeMCPTool(
					"craft", "Craft from the current player craft grid.", craft_schema));

			Json::Value place_schema = makeMCPPositionSchema();
			addMCPSchemaProperty(
					place_schema, "slot", "integer", "Hotbar slot to place from.");
			addMCPSchemaProperty(place_schema, "item", "string",
					"Item name to find in the hotbar before placing.");
			addMCPSchemaProperty(place_schema, "under_x", "integer",
					"Optional support node X coordinate.");
			addMCPSchemaProperty(place_schema, "under_y", "integer",
					"Optional support node Y coordinate.");
			addMCPSchemaProperty(place_schema, "under_z", "integer",
					"Optional support node Z coordinate.");
			tools.append(makeMCPTool("place_node",
					"Place the wielded or requested hotbar item at a world position.",
					place_schema));
			tools.append(makeMCPTool("dig_node", "Dig a node at a world position.",
					makeMCPPositionSchema()));

			tools.append(makeMCPTool("move_player_to",
					"Move player to specific coordinates.", makeMCPPositionSchema()));

			Json::Value rotate_schema = makeMCPObjectSchema();
			addMCPSchemaProperty(rotate_schema, "pitch", "number", "Camera pitch.");
			addMCPSchemaProperty(rotate_schema, "yaw", "number", "Camera yaw.");
			addMCPRequired(rotate_schema, "pitch");
			addMCPRequired(rotate_schema, "yaw");
			tools.append(makeMCPTool("rotate_player",
					"Rotate player camera to specific angles.", rotate_schema));
			Json::Value look_at_schema = makeMCPObjectSchema();
			for (const char *axis : {"x", "y", "z"})
				addMCPSchemaProperty(look_at_schema, axis, "number",
						"Target world position in node coordinates.");
			for (const char *axis : {"x", "y", "z"})
				addMCPRequired(look_at_schema, axis);
			tools.append(makeMCPTool("look_at_position",
					"Aim the camera at a world position without moving the player.",
					look_at_schema));
			Json::Value look_at_object_schema = makeMCPObjectSchema();
			addMCPSchemaProperty(look_at_object_schema, "object_id", "integer",
					"Object ID from get_nearby_objects.");
			addMCPRequired(look_at_object_schema, "object_id");
			tools.append(makeMCPTool("look_at_object",
					"Aim the camera at a visible active object without moving the player.",
					look_at_object_schema));
			tools.append(makeMCPTool("teleport_player",
					"Instantly teleport player to coordinates.",
					makeMCPPositionSchema()));
			tools.append(makeMCPTool(
					"get_pointed_thing", "Get the current pointed thing under cursor."));
			tools.append(makeMCPTool("get_nearby_objects",
					"List nearby visible objects with IDs, positions, velocity, and display text."));
			Json::Value object_action_schema = makeMCPObjectSchema();
			addMCPSchemaProperty(object_action_schema, "object_id", "integer",
					"Object ID from get_nearby_objects.");
			addMCPRequired(object_action_schema, "object_id");
			tools.append(makeMCPTool("interact_with_object",
					"Right-click an object by ID. The server applies its normal interaction checks.",
					object_action_schema));
			tools.append(makeMCPTool("punch_object",
					"Punch an object by ID. The server applies its normal reach and combat rules.",
					object_action_schema));
			Json::Value use_schema = makeMCPObjectSchema();
			addMCPSchemaProperty(
					use_schema, "slot", "integer", "Optional hotbar slot to use.");
			addMCPSchemaProperty(
					use_schema, "item", "string", "Optional item name to find and use.");
			addMCPSchemaProperty(use_schema, "object_id", "integer",
					"Optional nearby object ID to target instead of the current pointed thing.");
			for (const char *axis : {"x", "y", "z"})
				addMCPSchemaProperty(use_schema, (std::string("node_") + axis).c_str(),
						"integer", "Optional nearby node coordinate to target directly.");
			tools.append(makeMCPTool("use_item",
					"Use the wielded item on the current target or a nearby object ID; an explicit node target receives the normal right-click interaction.",
					use_schema));

			Json::Value world_schema = makeMCPObjectSchema();
			addMCPSchemaProperty(world_schema, "radius", "integer",
					"Radius in map blocks around the player, capped internally.");
			tools.append(makeMCPTool("get_world_content",
					"Get sampled world content around player.", world_schema));

			Json::Value result;
			result["tools"] = tools;
			response["result"] = result;
		} else if (method == "tools/call" && request.isMember("params")) {
			Json::Value params = request["params"];
			Json::Value args = params.isMember("arguments")
									   ? params["arguments"]
									   : Json::Value(Json::objectValue);
			std::string tool_name = params["name"].asString();
			LocalPlayer *player = m_env.getLocalPlayer();

			if (tool_name == "get_player_state") {
				Json::Value player_state;
				if (!player) {
					player_state["success"] = false;
					player_state["error"] = "No local player";
				} else {
					player_state["position"]["x"] = player->getPosition().X / BS;
					player_state["position"]["y"] = player->getPosition().Y / BS;
					player_state["position"]["z"] = player->getPosition().Z / BS;
					player_state["rotation"]["pitch"] = player->getPitch();
					player_state["rotation"]["yaw"] = player->getYaw();
					player_state["velocity"]["x"] = player->getSpeed().X / BS;
					player_state["velocity"]["y"] = player->getSpeed().Y / BS;
					player_state["velocity"]["z"] = player->getSpeed().Z / BS;
					player_state["health"] = (int)player->hp;
					player_state["breath"] = (int)player->getBreath();
					player_state["wield_index"] =
							static_cast<int>(player->getWieldIndex());
					player_state["hotbar_size"] =
							static_cast<int>(player->getMaxHotbarItemcount());
					player_state["success"] = true;
				}
				setMCPStatusResult(response, player_state);
			} else if (tool_name == "get_inventory") {
				Json::Value inventory_obj;
				if (!player) {
					inventory_obj["success"] = false;
					inventory_obj["error"] = "No local player";
				} else {
					Inventory *inventory = &player->inventory;
					bool inventory_available = true;
					if (args.isMember("node_x")) {
						v3pos_t node_pos(args["node_x"].asInt(), args["node_y"].asInt(),
								args["node_z"].asInt());
						const v3opos_t node_center = intToFloat(node_pos, BS);
						if (player->getPosition().getDistanceFromSQ(node_center) >
								8.0f * 8.0f * BS * BS) {
							inventory_obj["success"] = false;
							inventory_obj["error"] =
									"Node inventory is farther than 8 nodes";
							inventory_available = false;
						} else {
							InventoryLocation location;
							location.setNodeMeta(node_pos);
							inventory = getInventory(location);
							if (!inventory) {
								inventory_obj["success"] = false;
								inventory_obj["error"] =
										"Node inventory is not loaded; open it in game first";
								inventory_available = false;
							}
						}
						if (inventory_available) {
							inventory_obj["node_position"]["x"] = node_pos.X;
							inventory_obj["node_position"]["y"] = node_pos.Y;
							inventory_obj["node_position"]["z"] = node_pos.Z;
						}
					}
					if (inventory_available) {
						Json::Value lists(Json::arrayValue);
						for (const InventoryList *list : inventory->getLists())
							lists.append(inventoryListToMCPJson(list));
						inventory_obj["success"] = true;
						inventory_obj["wield_index"] =
								static_cast<int>(player->getWieldIndex());
						inventory_obj["hotbar_size"] =
								static_cast<int>(player->getMaxHotbarItemcount());
						inventory_obj["lists"] = lists;
					}
				}
				setMCPStatusResult(response, inventory_obj);
			} else if (tool_name == "send_chat_message") {
				Json::Value status;
				std::string message = args.get("message", "").asString();
				if (message.empty()) {
					status["success"] = false;
					status["error"] = "message is required";
				} else if (getState() != LC_Ready) {
					status["success"] = false;
					status["error"] = "Client is not connected and ready";
				} else {
					const bool send_now = canSendChatMessage();
					const size_t queued_before = m_out_chat_queue.size();
					sendChatMessage(utf8_to_wide(message));
					if (send_now || m_out_chat_queue.size() > queued_before) {
						status["success"] = true;
						status["delivery"] = send_now ? "sent" : "queued";
						status["message"] = message;
						status["queued_messages"] =
								static_cast<Json::UInt64>(m_out_chat_queue.size());
					} else {
						status["success"] = false;
						status["error"] = "Outgoing chat queue is full";
					}
				}
				setMCPStatusResult(response, status);
			} else if (tool_name == "get_chat_messages") {
				Json::Value chat_obj;
				const std::string buffer_name = args.get("buffer", "history").asString();
				const int count = args.get("count", 50).asInt();
				if (buffer_name == "history") {
					chat_obj = getMCPChatMessages(
							args.get("after_id", Json::UInt64(0)).asUInt64(), count);
					chat_obj["buffer"] = "history";
				} else if (!chat_backend) {
					chat_obj["success"] = false;
					chat_obj["error"] = "Chat backend is not available";
				} else {
					if (buffer_name == "console") {
						chat_obj = chatBufferToMCPJson(
								chat_backend->getConsoleBuffer(), count);
						chat_obj["buffer"] = "console";
					} else if (buffer_name == "recent") {
						chat_obj = chatBufferToMCPJson(
								chat_backend->getRecentBuffer(), count);
						chat_obj["buffer"] = "recent";
					} else {
						chat_obj["success"] = false;
						chat_obj["error"] =
								"buffer must be 'history', 'recent', or 'console'";
					}
				}
				setMCPStatusResult(response, chat_obj);
			} else if (tool_name == "stop_player_control") {
				{
					std::lock_guard<std::mutex> lock(m_mcp_control_mutex);
					m_has_mcp_control_override = false;
				}
				Json::Value status;
				status["success"] = true;
				status["state"] = "released";
				setMCPStatusResult(response, status);
			} else if (tool_name == "set_player_control") {
				PlayerControl control = player ? player->control : PlayerControl();
				if (args.isMember("forward"))
					control.up = args["forward"].asBool() ? 1.0f : 0.0f;
				if (args.isMember("backward"))
					control.down = args["backward"].asBool() ? 1.0f : 0.0f;
				if (args.isMember("left"))
					control.left = args["left"].asBool() ? 1.0f : 0.0f;
				if (args.isMember("right"))
					control.right = args["right"].asBool() ? 1.0f : 0.0f;
				if (args.isMember("jump"))
					control.jump = args["jump"].asBool();
				if (args.isMember("sneak"))
					control.sneak = args["sneak"].asBool();
				if (args.isMember("dig"))
					control.dig = args["dig"].asBool();
				if (args.isMember("place"))
					control.place = args["place"].asBool();
				if (args.isMember("zoom"))
					control.zoom = args["zoom"].asBool();
				if (args.isMember("aux1"))
					control.aux1 = args["aux1"].asBool();
				if (args.isMember("pitch"))
					control.pitch = args["pitch"].asFloat();
				if (args.isMember("yaw"))
					control.yaw = args["yaw"].asFloat();
				if (args.isMember("pitch") || args.isMember("yaw"))
					setMCPRotationTarget(control.pitch, control.yaw);

				const u32 duration_ms =
						rangelim(args.get("duration_ms", 250).asUInt(), 50, 5000);
				if (player)
					setMCPPlayerControl(control, duration_ms);
				Json::Value status;
				status["success"] = player != nullptr;
				if (player)
					status["state"] = "active";
				else
					status["error"] = "No local player";
				status["duration_ms"] = duration_ms;
				status["controls"]["forward"] = control.up != 0;
				status["controls"]["backward"] = control.down != 0;
				status["controls"]["left"] = control.left != 0;
				status["controls"]["right"] = control.right != 0;
				status["controls"]["jump"] = control.jump;
				status["controls"]["sneak"] = control.sneak;
				status["controls"]["dig"] = control.dig;
				status["controls"]["place"] = control.place;
				status["controls"]["aux1"] = control.aux1;
				status["controls"]["zoom"] = control.zoom;
				setMCPStatusResult(response, status);
			} else if (tool_name == "press_keys") {
				Json::Value status;
				if (!m_mcp_key_injector) {
					status["success"] = false;
					status["error"] = "Game input handler is unavailable";
				} else {
					const auto duration = std::chrono::milliseconds(
							args.get("duration_ms", 250).asUInt());
					Json::Value pressed(Json::arrayValue);
					for (const auto &value : args["keys"]) {
						GameKeyType key;
						if (!getMCPGameKey(value.asString(), key))
							continue;
						const int key_id = static_cast<int>(key);
						m_mcp_key_injector(key_id, true);
						m_mcp_pressed_keys[key_id] =
								std::chrono::steady_clock::now() + duration;
						pressed.append(value.asString());
					}
					status["success"] = !pressed.empty();
					status["keys"] = pressed;
					status["duration_ms"] = duration.count();
				}
				setMCPStatusResult(response, status);
			} else if (tool_name == "get_node") {
				v3pos_t pos(args["x"].asInt(), args["y"].asInt(), args["z"].asInt());
				bool ok = false;
				MapNode node = m_env.getClientMap().getNode(pos, &ok);
				setMCPTextResult(
						response, nodeToMCPJson(pos, node, ok, getNodeDefManager()));
			} else if (tool_name == "get_nodes_area") {
				v3pos_t minp(args["min_x"].asInt(), args["min_y"].asInt(),
						args["min_z"].asInt());
				v3pos_t maxp(args["max_x"].asInt(), args["max_y"].asInt(),
						args["max_z"].asInt());
				if (maxp.X < minp.X)
					std::swap(maxp.X, minp.X);
				if (maxp.Y < minp.Y)
					std::swap(maxp.Y, minp.Y);
				if (maxp.Z < minp.Z)
					std::swap(maxp.Z, minp.Z);

				s64 dx = (s64)maxp.X - minp.X + 1;
				s64 dy = (s64)maxp.Y - minp.Y + 1;
				s64 dz = (s64)maxp.Z - minp.Z + 1;
				s64 volume = dx > 4096 || dy > 4096 || dz > 4096 ? 4097 : dx * dy * dz;
				Json::Value area;
				area["success"] = volume <= 4096;
				area["volume"] = (Json::Int64)volume;
				if (volume > 4096) {
					area["error"] = "Requested area is too large";
				} else {
					Json::Value nodes(Json::arrayValue);
					for (s64 x = minp.X; x <= maxp.X; x++) {
						for (s64 y = minp.Y; y <= maxp.Y; y++) {
							for (s64 z = minp.Z; z <= maxp.Z; z++) {
								v3pos_t pos((s32)x, (s32)y, (s32)z);
								bool ok = false;
								MapNode node = m_env.getClientMap().getNode(pos, &ok);
								nodes.append(nodeToMCPJson(
										pos, node, ok, getNodeDefManager()));
							}
						}
					}
					area["nodes"] = nodes;
				}
				setMCPStatusResult(response, area);
			} else if (tool_name == "set_wielded_item") {
				Json::Value status;
				selectMCPWieldedItem(this, player, args, status);
				setMCPStatusResult(response, status);
			} else if (tool_name == "move_inventory_item") {
				Json::Value status;
				status["success"] = false;
				if (!args.isMember("from_index") || !args.isMember("to_index")) {
					status["error"] = "from_index and to_index are required";
				} else {
					auto get_location = [&](const char *prefix,
												InventoryLocation &location,
												const std::string &list_name, int index) {
						const std::string key = std::string(prefix) + "x";
						if (args.isMember(key)) {
							v3pos_t node_pos(args[key].asInt(),
									args[std::string(prefix) + "y"].asInt(),
									args[std::string(prefix) + "z"].asInt());
							if (!approachMCPInteractionNode(this, node_pos, status))
								return false;
							location.setNodeMeta(node_pos);
						} else {
							location.setCurrentPlayer();
						}
						Inventory *inventory = getInventory(location);
						const InventoryList *list =
								inventory ? inventory->getList(list_name) : nullptr;
						if (!list || index >= list->getSize()) {
							status["error"] =
									"Inventory list or slot is unavailable; open node inventories in game first";
							return false;
						}
						return true;
					};
					IMoveAction action;
					action.count = args.get("count", 0).asUInt();
					action.from_list = args.get("from_list", "main").asString();
					action.from_i = args["from_index"].asInt();
					action.to_list = args.get("to_list", "main").asString();
					action.to_i = args["to_index"].asInt();
					if (get_location("from_node_", action.from_inv, action.from_list,
								action.from_i) &&
							get_location("to_node_", action.to_inv, action.to_list,
									action.to_i)) {
						inventoryAction(new IMoveAction(action));
						status["success"] = true;
						status["state"] = "submitted";
						status["note"] =
								"Inventory move was submitted; read both inventories to confirm.";
					}
				}
				setMCPStatusResult(response, status);
			} else if (tool_name == "craft") {
				ICraftAction *a = new ICraftAction();
				u16 count = args.get("count", 0).asUInt();
				a->count = count;
				a->craft_inv.setCurrentPlayer();
				inventoryAction(a);

				Json::Value status;
				status["success"] = true;
				status["count"] = count;
				status["state"] = "submitted";
				status["note"] =
						"Craft request was submitted; read the inventory to confirm.";
				setMCPStatusResult(response, status);
			} else if (tool_name == "place_node") {
				Json::Value status;
				v3pos_t target(args["x"].asInt(), args["y"].asInt(), args["z"].asInt());
				if (!selectMCPWieldedItem(this, player, args, status)) {
					setMCPStatusResult(response, status);
				} else {
					PointedThing pointed;
					if (makeMCPPlacePointedThing(this, target, args, pointed, status)) {
						if (approachMCPInteractionNode(this, target, status)) {
							setMCPCameraTarget(this, target);
							interact(INTERACT_PLACE, pointed);
							status["success"] = true;
							status["target"]["x"] = target.X;
							status["target"]["y"] = target.Y;
							status["target"]["z"] = target.Z;
							status["state"] = "submitted";
							status["note"] =
									"Placement was sent to the server and is not yet confirmed. "
									"The local map can remain stale until a server update arrives.";
						}
					}
					setMCPStatusResult(response, status);
				}
			} else if (tool_name == "dig_node") {
				Json::Value status;
				v3pos_t pos(args["x"].asInt(), args["y"].asInt(), args["z"].asInt());
				bool ok = false;
				MapNode node = m_env.getClientMap().getNode(pos, &ok);
				const ContentFeatures &features = getNodeDefManager()->get(node);
				if (!ok) {
					status["success"] = false;
					status["error"] = "Node position is not loaded";
				} else if (!features.diggable) {
					status["success"] = false;
					status["error"] = "Node is not diggable";
					status["node"] = nodeToMCPJson(pos, node, ok, getNodeDefManager());
				} else {
					if (approachMCPInteractionNode(this, pos, status)) {
						setMCPCameraTarget(this, pos);
						PointedThing pointed =
								makeMCPNodePointedThing(this, pos, features);
						interact(INTERACT_START_DIGGING, pointed);
						interact(INTERACT_DIGGING_COMPLETED, pointed);
						status["success"] = true;
						status["state"] = "submitted";
						status["note"] =
								"Dig request was sent to the server and is not yet confirmed. "
								"The local map can remain stale until a server update arrives.";
						status["node"] =
								nodeToMCPJson(pos, node, ok, getNodeDefManager());
					}
				}
				setMCPStatusResult(response, status);
			} else if (tool_name == "move_player_to" || tool_name == "teleport_player") {
				Json::Value status;
				if (!player) {
					status["success"] = false;
					status["error"] = "No local player";
				} else {
					player->setPosition(v3opos_t(args["x"].asFloat() * BS,
							args["y"].asFloat() * BS, args["z"].asFloat() * BS));
					player->setSpeed(v3f(0.0f));
					status["success"] = true;
					status["state"] = "submitted";
					status["note"] =
							"The local position was changed; the server may correct it. Read player state to confirm.";
				}
				setMCPStatusResult(response, status);
			} else if (tool_name == "rotate_player") {
				Json::Value status;
				if (!player) {
					status["success"] = false;
					status["error"] = "No local player";
				} else {
					PlayerControl control = player->control;
					control.pitch = args["pitch"].asFloat();
					control.yaw = args["yaw"].asFloat();
					setMCPRotationTarget(control.pitch, control.yaw);
					setMCPPlayerControl(control, 250);
					status["success"] = true;
					status["state"] = "submitted";
				}
				setMCPStatusResult(response, status);
			} else if (tool_name == "look_at_position" || tool_name == "look_at_object") {
				Json::Value status;
				if (!player) {
					status["success"] = false;
					status["error"] = "No local player";
				} else {
					v3opos_t target;
					bool target_available = true;
					if (tool_name == "look_at_position") {
						target = v3opos_t(args["x"].asFloat(), args["y"].asFloat(),
										 args["z"].asFloat()) *
								 BS;
					} else {
						ClientActiveObject *object = m_env.getActiveObject(
								static_cast<u16>(args["object_id"].asUInt()));
						if (!object) {
							status["success"] = false;
							status["error"] = "Object is not currently available";
							target_available = false;
						} else {
							target = object->getPosition();
							status["object_id"] = object->getId();
						}
					}
					if (target_available) {
						const auto eye = player->getEyePosition();
						const auto delta = target - eye;
						const f32 horizontal =
								std::sqrt(delta.X * delta.X + delta.Z * delta.Z);
						if (delta.getLengthSQ() < 0.0001f) {
							status["success"] = false;
							status["error"] =
									"Target position is too close to the camera";
						} else {
							constexpr f32 rad_to_deg = 180.0f / static_cast<f32>(M_PI);
							const f32 pitch =
									-std::atan2(delta.Y, horizontal) * rad_to_deg;
							const f32 yaw = -std::atan2(delta.X, delta.Z) * rad_to_deg;
							PlayerControl control = player->control;
							control.pitch = pitch;
							control.yaw = yaw;
							setMCPRotationTarget(pitch, yaw);
							setMCPPlayerControl(control, 250);
							status["success"] = true;
							status["state"] = "submitted";
							status["pitch"] = pitch;
							status["yaw"] = yaw;
							status["target"]["x"] = target.X / BS;
							status["target"]["y"] = target.Y / BS;
							status["target"]["z"] = target.Z / BS;
							status["note"] =
									"Camera rotation was submitted; read player state and get_pointed_thing to confirm aim.";
						}
					}
				}
				setMCPStatusResult(response, status);
			} else if (tool_name == "get_pointed_thing") {
				PointedThing pointed = getCurrentPointedThing();
				Json::Value pointed_obj;
				pointed_obj["success"] = true;
				pointed_obj["type"] = (int)pointed.type;
				pointed_obj["pointability"] = (int)pointed.pointability;

				if (pointed.type == POINTEDTHING_NODE) {
					pointed_obj["node"]["undersurface"]["x"] =
							pointed.node_undersurface.X;
					pointed_obj["node"]["undersurface"]["y"] =
							pointed.node_undersurface.Y;
					pointed_obj["node"]["undersurface"]["z"] =
							pointed.node_undersurface.Z;
					pointed_obj["node"]["abovesurface"]["x"] =
							pointed.node_abovesurface.X;
					pointed_obj["node"]["abovesurface"]["y"] =
							pointed.node_abovesurface.Y;
					pointed_obj["node"]["abovesurface"]["z"] =
							pointed.node_abovesurface.Z;
					pointed_obj["node"]["real_undersurface"]["x"] =
							pointed.node_real_undersurface.X;
					pointed_obj["node"]["real_undersurface"]["y"] =
							pointed.node_real_undersurface.Y;
					pointed_obj["node"]["real_undersurface"]["z"] =
							pointed.node_real_undersurface.Z;
				} else if (pointed.type == POINTEDTHING_OBJECT) {
					pointed_obj["object_id"] = (int)pointed.object_id;
				}

				pointed_obj["box_id"] = (int)pointed.box_id;
				pointed_obj["intersection_point"]["x"] = pointed.intersection_point.X;
				pointed_obj["intersection_point"]["y"] = pointed.intersection_point.Y;
				pointed_obj["intersection_point"]["z"] = pointed.intersection_point.Z;
				pointed_obj["intersection_normal"]["x"] = pointed.intersection_normal.X;
				pointed_obj["intersection_normal"]["y"] = pointed.intersection_normal.Y;
				pointed_obj["intersection_normal"]["z"] = pointed.intersection_normal.Z;
				pointed_obj["raw_intersection_normal"]["x"] =
						pointed.raw_intersection_normal.X;
				pointed_obj["raw_intersection_normal"]["y"] =
						pointed.raw_intersection_normal.Y;
				pointed_obj["raw_intersection_normal"]["z"] =
						pointed.raw_intersection_normal.Z;
				pointed_obj["distance_sq"] = pointed.distanceSq;
				setMCPTextResult(response, pointed_obj);
			} else if (tool_name == "get_nearby_objects") {
				Json::Value result;
				LocalPlayer *local_player = m_env.getLocalPlayer();
				if (!local_player) {
					result["success"] = false;
					result["error"] = "No local player";
				} else {
					std::vector<DistanceSortedActiveObject> objects;
					// Include the full active-object range so bots can plan where to
					// travel next. The previous hard-coded 32-node radius made the
					// tool unable to discover distant mobs and item drops.
					m_env.getActiveObjects(
							local_player->getPosition(), 128.0f * BS, objects);
					Json::Value list(Json::arrayValue);
					for (const auto &entry : objects) {
						ClientActiveObject *object = entry.obj.get();
						if (!object || object->isLocalPlayer())
							continue;
						const auto pos = object->getPosition() / BS;
						const v3f velocity = object->getVelocity() / BS;
						Json::Value item;
						item["id"] = object->getId();
						item["position"]["x"] = pos.X;
						item["position"]["y"] = pos.Y;
						item["position"]["z"] = pos.Z;
						item["velocity"]["x"] = velocity.X;
						item["velocity"]["y"] = velocity.Y;
						item["velocity"]["z"] = velocity.Z;
						item["info"] = object->infoText();
						if (const auto *cao = dynamic_cast<const GenericCAO *>(object))
							item["name"] = cao->getName();
						list.append(item);
					}
					result["success"] = true;
					result["radius_nodes"] = 128;
					result["objects"] = list;
				}
				setMCPStatusResult(response, result);
			} else if (tool_name == "use_item") {
				Json::Value status;
				if (selectMCPWieldedItem(this, player, args, status)) {
					PointedThing pointed = getCurrentPointedThing();
					bool target_available = true;
					if (args.isMember("node_x")) {
						v3pos_t node_pos(args["node_x"].asInt(), args["node_y"].asInt(),
								args["node_z"].asInt());
						const auto node_center = intToFloat(node_pos, BS);
						bool node_ok = false;
						const MapNode node =
								m_env.getClientMap().getNode(node_pos, &node_ok);
						const ContentFeatures &features = getNodeDefManager()->get(node);
						if (!player || player->getPosition().getDistanceFromSQ(
											   node_center) > 8.0f * 8.0f * BS * BS) {
							target_available = false;
							status["error"] = "Node is farther than 8 nodes";
						} else if (!node_ok || features.pointable ==
													   PointabilityType::POINTABLE_NOT) {
							target_available = false;
							status["error"] = "Node is not loaded or pointable";
						} else {
							if (!approachMCPInteractionNode(this, node_pos, status)) {
								target_available = false;
							} else {
								setMCPCameraTarget(this, node_pos);
								const v3pos_t above = node_pos + v3pos_t(0, 1, 0);
								const v3f normal(0.0f, 1.0f, 0.0f);
								pointed = PointedThing(
										v3pos_t(node_pos.X, node_pos.Y, node_pos.Z),
										v3pos_t(above.X, above.Y, above.Z),
										v3pos_t(node_pos.X, node_pos.Y, node_pos.Z),
										node_center, normal, 0,
										player->getPosition().getDistanceFromSQ(
												node_center),
										features.pointable);
							}
						}
					} else if (args.isMember("object_id")) {
						ClientActiveObject *object = m_env.getActiveObject(
								static_cast<u16>(args["object_id"].asUInt()));
						if (!object) {
							target_available = false;
							status["success"] = false;
							status["error"] = "Object is no longer available";
						} else {
							const auto position = object->getPosition();
							if (!approachMCPInteractionTarget(this, position, status)) {
								target_available = false;
							} else {
								setMCPCameraTarget(this, position);
								const v3f normal(0.0f, 1.0f, 0.0f);
								const f32 distance_sq =
										player->getPosition().getDistanceFromSQ(position);
								pointed = PointedThing(object->getId(), position, normal,
										normal, distance_sq, PointabilityType::POINTABLE);
							}
						}
					}
					if (target_available) {
						if (args.isMember("node_x"))
							interact(INTERACT_PLACE, pointed);
						else if (pointed.type == POINTEDTHING_NOTHING)
							interact(INTERACT_ACTIVATE, pointed);
						else
							interact(INTERACT_USE, pointed);
						status["success"] = true;
						status["state"] = "submitted";
						status["note"] =
								"Use request was sent to the server; read inventory to confirm.";
					}
				}
				setMCPStatusResult(response, status);
			} else if (tool_name == "interact_with_object" ||
					   tool_name == "punch_object") {
				Json::Value status;
				ClientActiveObject *object = m_env.getActiveObject(
						static_cast<u16>(args["object_id"].asUInt()));
				if (!object) {
					status["success"] = false;
					status["error"] = "Object is no longer available";
				} else {
					const auto position = object->getPosition();
					if (approachMCPInteractionTarget(this, position, status)) {
						setMCPCameraTarget(this, position);
						if (tool_name == "punch_object") {
							const auto direction = oposToV3f(
									(position - player->getPosition()).normalize());
							ItemStack selected_item;
							ItemStack hand_item;
							ItemStack *hand_item_ptr = nullptr;
							if (m_itemdef->isKnown(""))
								hand_item_ptr = &hand_item;
							ItemStack &tool_item =
									player->getWieldedItem(&selected_item, hand_item_ptr);
							object->directReportPunch(
									direction, &tool_item, hand_item_ptr, 1.0f);
						}
						const v3f normal(0.0f, 1.0f, 0.0f);
						const f32 distance_sq =
								player->getPosition().getDistanceFromSQ(position);
						PointedThing pointed(object->getId(), position, normal, normal,
								distance_sq, PointabilityType::POINTABLE);
						interact(tool_name == "punch_object" ? INTERACT_START_DIGGING
															 : INTERACT_PLACE,
								pointed);
						status["success"] = true;
						status["state"] = "submitted";
						status["object_id"] = object->getId();
						status["note"] =
								"Request was sent; the server decides whether the interaction succeeds.";
					}
				}
				setMCPStatusResult(response, status);
			} else if (tool_name == "get_world_content") {
				int radius = args.get("radius", 5).asInt();
				Json::Value world = getWorldContentAroundPlayer(radius);
				setMCPStatusResult(response, world);
			} else {
				setMCPError(response, -32602, "Tool not found: " + tool_name);
			}
		} else {
			setMCPError(response, -32601, "Method not found: " + method);
		}
	} catch (const std::exception &e) {
		setMCPError(response, -32603, "Internal error: " + std::string(e.what()));
	}

	sendMCPResponse(connection, std::move(response), session_id);
}

void Client::sendMCPResponse(mcp_ws_server_t::connection_ptr connection,
		Json::Value response, const std::string &session_id)
{
	auto payload = std::make_shared<std::string>(
			Json::writeString(Json::StreamWriterBuilder(), response));
	boost::asio::post(
			m_mcp_http_server.get_io_context(), [connection, payload, session_id]() {
				websocketpp::lib::error_code ec;
				connection->append_header("Content-Type", "application/json");
				if (!session_id.empty())
					connection->append_header("Mcp-Session-Id", session_id);
				connection->set_body(*payload);
				connection->set_status(websocketpp::http::status_code::ok);
				connection->send_http_response(ec);
				if (ec)
					verbosestream << "Failed to send MCP Streamable HTTP response: "
								  << ec.message() << std::endl;
			});
}

void Client::processMCPRequests()
{
	const auto now = std::chrono::steady_clock::now();
	for (auto it = m_mcp_pressed_keys.begin(); it != m_mcp_pressed_keys.end();) {
		if (it->second <= now) {
			if (m_mcp_key_injector)
				m_mcp_key_injector(it->first, false);
			it = m_mcp_pressed_keys.erase(it);
		} else {
			++it;
		}
	}

	std::deque<PendingMCPRequest> requests;
	{
		std::lock_guard<std::mutex> lock(m_mcp_request_mutex);
		requests.swap(m_mcp_requests);
	}

	for (const auto &pending : requests) {
		handleMCPMessage(pending.connection, pending.request, pending.session_id);
	}
}
#endif

#if USE_CLIENT_MCP
static std::string makeMCPHttpSessionId()
{
	std::random_device random;
	std::ostringstream id;
	id << std::hex << std::setfill('0');
	for (unsigned int i = 0; i < 4; ++i)
		id << std::setw(8) << random();
	return id.str();
}

static bool isLocalMCPOrigin(const std::string &origin)
{
	if (origin.empty())
		return true;
	for (const char *prefix :
			{"http://127.0.0.1", "https://127.0.0.1", "http://localhost",
					"https://localhost", "http://[::1]", "https://[::1]"}) {
		const size_t length = std::char_traits<char>::length(prefix);
		if (origin.compare(0, length, prefix) == 0 &&
				(origin.size() == length || origin[length] == ':'))
			return true;
	}
	return false;
}

void Client::onMCPStreamableHttp(websocketpp::connection_hdl hdl)
{
	auto connection = m_mcp_http_server.get_con_from_hdl(hdl);
	auto respond = [&](websocketpp::http::status_code::value status,
						   const std::string &body = "",
						   const std::string &content_type = "") {
		if (!content_type.empty())
			connection->append_header("Content-Type", content_type);
		connection->set_body(body);
		connection->set_status(status);
	};
	auto json_error = [&](websocketpp::http::status_code::value status, int code,
							  const std::string &message,
							  const Json::Value &id = Json::Value()) {
		Json::Value response;
		response["jsonrpc"] = "2.0";
		response["id"] = id;
		setMCPError(response, code, message);
		Json::StreamWriterBuilder writer;
		writer["indentation"] = "";
		respond(status, Json::writeString(writer, response), "application/json");
	};

	std::string uri = connection->get_request().get_uri();
	if (const auto query = uri.find('?'); query != std::string::npos)
		uri.resize(query);
	if (uri != "/mcp") {
		respond(websocketpp::http::status_code::not_found);
		return;
	}
	if (!isLocalMCPOrigin(connection->get_request_header("Origin"))) {
		respond(websocketpp::http::status_code::forbidden, "Untrusted Origin header\n",
				"text/plain; charset=utf-8");
		return;
	}

	const std::string method = connection->get_request().get_method();
	const std::string session_id = connection->get_request_header("Mcp-Session-Id");
	if (method == "GET") {
		// This server has no unsolicited server-to-client messages, so it does
		// not expose an SSE listening stream.
		respond(websocketpp::http::status_code::method_not_allowed);
		return;
	}
	if (method == "DELETE") {
		if (session_id.empty()) {
			respond(websocketpp::http::status_code::bad_request);
			return;
		}
		auto session = m_mcp_http_sessions.find(session_id);
		if (session == m_mcp_http_sessions.end()) {
			respond(websocketpp::http::status_code::not_found);
			return;
		}
		m_mcp_http_sessions.erase(session);
		respond(websocketpp::http::status_code::no_content);
		return;
	}
	if (method != "POST") {
		respond(websocketpp::http::status_code::method_not_allowed);
		return;
	}

	const std::string accept = connection->get_request_header("Accept");
	if (accept.find("application/json") == std::string::npos ||
			accept.find("text/event-stream") == std::string::npos) {
		respond(websocketpp::http::status_code::not_acceptable,
				"Accept must include application/json and text/event-stream\n",
				"text/plain; charset=utf-8");
		return;
	}
	const std::string content_type = connection->get_request_header("Content-Type");
	if (content_type.compare(
				0, std::string("application/json").size(), "application/json") != 0) {
		respond(websocketpp::http::status_code::unsupported_media_type);
		return;
	}

	Json::Value request;
	Json::CharReaderBuilder reader;
	std::string errors;
	const std::string &body = connection->get_request_body();
	std::unique_ptr<Json::CharReader> json_reader(reader.newCharReader());
	if (!json_reader->parse(body.data(), body.data() + body.size(), &request, &errors)) {
		json_error(websocketpp::http::status_code::bad_request, -32700,
				"Parse error: " + errors);
		return;
	}
	const bool has_id = request.isObject() && request.isMember("id");
	const Json::Value response_id = has_id ? request["id"] : Json::Value();
	if (!request.isObject() || request.get("jsonrpc", "").asString() != "2.0") {
		json_error(websocketpp::http::status_code::bad_request, -32600, "Invalid Request",
				response_id);
		return;
	}
	if (!request.isMember("method")) {
		// The server currently issues no JSON-RPC requests. Valid client
		// responses therefore have nothing to dispatch, but are accepted as
		// required by Streamable HTTP.
		respond(websocketpp::http::status_code::accepted);
		return;
	}
	if (!request["method"].isString()) {
		json_error(websocketpp::http::status_code::bad_request, -32600,
				"Invalid Request: method must be a string", response_id);
		return;
	}
	if (has_id && !(request["id"].isString() || request["id"].isInt64() ||
						  request["id"].isUInt64())) {
		json_error(websocketpp::http::status_code::bad_request, -32600,
				"Invalid Request: id must be a string or integer");
		return;
	}
	if (request.isMember("params") && !request["params"].isObject()) {
		json_error(websocketpp::http::status_code::bad_request, -32602,
				"Invalid params: params must be an object", response_id);
		return;
	}

	const std::string rpc_method = request["method"].asString();
	std::string request_session = session_id;
	if (rpc_method == "initialize") {
		if (!has_id || !session_id.empty()) {
			json_error(websocketpp::http::status_code::bad_request, -32600,
					"Initialize must be a request without a session ID", response_id);
			return;
		}
		const Json::Value &params = request["params"];
		if (!params.isObject() || !params["protocolVersion"].isString() ||
				!params["capabilities"].isObject() || !params["clientInfo"].isObject()) {
			json_error(websocketpp::http::status_code::bad_request, -32602,
					"Invalid initialize parameters", response_id);
			return;
		}
		request_session = makeMCPHttpSessionId();
		const std::string requested = params["protocolVersion"].asString();
		const std::string negotiated =
				requested == "2025-03-26" || requested == "2025-06-18" ? requested
																	   : "2025-06-18";
		request["params"]["protocolVersion"] = negotiated;
		m_mcp_http_sessions[request_session] = {
				MCPConnectionState::AwaitingInitialized, negotiated};
	} else {
		if (session_id.empty()) {
			respond(websocketpp::http::status_code::bad_request,
					"Missing Mcp-Session-Id header\n", "text/plain; charset=utf-8");
			return;
		}
		auto session = m_mcp_http_sessions.find(session_id);
		if (session == m_mcp_http_sessions.end()) {
			respond(websocketpp::http::status_code::not_found);
			return;
		}
		const std::string protocol =
				connection->get_request_header("MCP-Protocol-Version");
		if (!protocol.empty() && protocol != session->second.protocol_version) {
			respond(websocketpp::http::status_code::bad_request,
					"Unsupported MCP-Protocol-Version\n", "text/plain; charset=utf-8");
			return;
		}
		if (!has_id) {
			if (rpc_method == "notifications/initialized" &&
					session->second.state == MCPConnectionState::AwaitingInitialized)
				session->second.state = MCPConnectionState::Ready;
			respond(websocketpp::http::status_code::accepted);
			return;
		}
		if (session->second.state != MCPConnectionState::Ready) {
			json_error(websocketpp::http::status_code::bad_request, -32002,
					"Server is not initialized", response_id);
			return;
		}
	}

	if (rpc_method == "tools/call") {
		const Json::Value &params = request["params"];
		if (!params.isObject() || !params["name"].isString() ||
				(params.isMember("arguments") && !params["arguments"].isObject())) {
			json_error(websocketpp::http::status_code::bad_request, -32602,
					"Invalid tools/call parameters", response_id);
			return;
		}
		const Json::Value arguments = params.isMember("arguments")
											  ? params["arguments"]
											  : Json::Value(Json::objectValue);
		std::string argument_error;
		if (!validateMCPToolArguments(
					params["name"].asString(), arguments, argument_error)) {
			json_error(websocketpp::http::status_code::bad_request, -32602,
					"Invalid tool arguments: " + argument_error, response_id);
			return;
		}
	}

	{
		std::lock_guard<std::mutex> lock(m_mcp_request_mutex);
		if (m_mcp_requests.size() >= 128) {
			json_error(websocketpp::http::status_code::service_unavailable, -32000,
					"MCP request queue is full", response_id);
			return;
		}
		connection->defer_http_response();
		m_mcp_requests.push_back({connection, request, request_session});
	}
}
#endif

void Client::startMCPStreamableHttpServer(int port)
{
#if USE_CLIENT_MCP
	if (m_http_server_running)
		return;

	try {
		m_mcp_http_server.init_asio();
		m_mcp_http_server.set_reuse_addr(true);
		m_mcp_http_server.clear_access_channels(websocketpp::log::alevel::all);
		m_mcp_http_server.set_http_handler([this](websocketpp::connection_hdl hdl) {
			this->onMCPStreamableHttp(hdl);
		});

		websocketpp::lib::error_code ec;
		websocketpp::lib::asio::ip::tcp::endpoint endpoint(
				boost::asio::ip::make_address("127.0.0.1"), port);
		m_mcp_http_server.listen(endpoint, ec);
		if (ec) {
			errorstream << "Failed to bind MCP Streamable HTTP server to port " << port
						<< ": " << ec.message() << std::endl;
			return;
		}
		m_mcp_http_server.start_accept(ec);
		if (ec) {
			errorstream << "Failed to start MCP Streamable HTTP accept loop: "
						<< ec.message() << std::endl;
			return;
		}

		m_http_server_running = true;
		m_http_server_thread = std::thread([this]() {
			try {
				m_mcp_http_server.run();
			} catch (const std::exception &e) {
				errorstream << "MCP Streamable HTTP server error: " << e.what()
							<< std::endl;
			}
		});
		actionstream << "MCP Streamable HTTP transport started on http://127.0.0.1:"
					 << port << "/mcp" << std::endl;
	} catch (const std::exception &e) {
		errorstream << "Failed to start MCP Streamable HTTP server: " << e.what()
					<< std::endl;
	}
#endif
}

void Client::stopMCPServer()
{
#if USE_CLIENT_MCP
	if (m_http_server_running) {
		m_http_server_running = false;
		m_mcp_http_server.stop_listening();
		m_mcp_http_server.stop();
		if (m_http_server_thread.joinable())
			m_http_server_thread.join();
		m_mcp_http_sessions.clear();
		infostream << "MCP Streamable HTTP server stopped" << std::endl;
	}
	std::lock_guard<std::mutex> lock(m_mcp_request_mutex);
	m_mcp_requests.clear();
#endif
}

#if USE_CLIENT_MCP

void Client::setMCPPlayerControl(PlayerControl control, u32 duration_ms)
{
	control.setMovementFromKeys();

	std::lock_guard<std::mutex> lock(m_mcp_control_mutex);
	m_mcp_control_override = control;
	m_mcp_control_override_until =
			std::chrono::steady_clock::now() + std::chrono::milliseconds(duration_ms);
	m_has_mcp_control_override = true;
}

PointedThing Client::getCurrentPointedThing() const
{
	return m_mcp_pointed_thing;
}

void Client::setCurrentPointedThing(const PointedThing &pointed)
{
	m_mcp_pointed_thing = pointed;
}

Json::Value Client::getWorldContentAroundPlayer(int radius_blocks)
{
	Json::Value world_content;

	radius_blocks = rangelim(radius_blocks, 0, 3);

	LocalPlayer *player = m_env.getLocalPlayer();
	if (!player) {
		world_content["success"] = false;
		world_content["error"] = "No local player";
		return world_content;
	}
	world_content["success"] = true;

	auto player_pos = player->getPosition();
	auto player_block_pos = getNodeBlockPos(floatToInt(player_pos, BS));

	world_content["player_position"]["x"] = player_pos.X / BS;
	world_content["player_position"]["y"] = player_pos.Y / BS;
	world_content["player_position"]["z"] = player_pos.Z / BS;
	world_content["player_block_position"]["x"] = player_block_pos.X;
	world_content["player_block_position"]["y"] = player_block_pos.Y;
	world_content["player_block_position"]["z"] = player_block_pos.Z;
	world_content["radius_blocks"] = radius_blocks;

	ClientMap &map = m_env.getClientMap();
	Json::Value blocks_array(Json::arrayValue);

	const int max_blocks = 27;
	int block_count = 0;

	std::vector<v3pos_t> candidates;
	for (s32 x = -radius_blocks; x <= radius_blocks; ++x)
		for (s32 y = -radius_blocks; y <= radius_blocks; ++y)
			for (s32 z = -radius_blocks; z <= radius_blocks; ++z)
				candidates.emplace_back(player_block_pos.X + x, player_block_pos.Y + y,
						player_block_pos.Z + z);
	std::sort(candidates.begin(), candidates.end(),
			[&](const v3pos_t &a, const v3pos_t &b) {
				auto dist_sq = [&](const v3pos_t &p) {
					s64 x = (s64)p.X - player_block_pos.X;
					s64 y = (s64)p.Y - player_block_pos.Y;
					s64 z = (s64)p.Z - player_block_pos.Z;
					return x * x + y * y + z * z;
				};
				return dist_sq(a) < dist_sq(b);
			});

	for (const v3pos_t &block_pos : candidates) {
		if (block_count >= max_blocks)
			break;
		MapBlock *block = map.getBlockNoCreateNoEx(block_pos);

		if (!block)
			continue;

		Json::Value block_obj;
		block_obj["position"]["x"] = block_pos.X;
		block_obj["position"]["y"] = block_pos.Y;
		block_obj["position"]["z"] = block_pos.Z;
		block_obj["is_generated"] = block->isGenerated();
		block_obj["timestamp"] = static_cast<int>(block->getTimestamp());
		block_obj["is_air"] = block->isAir();

		Json::Value nodes_array(Json::arrayValue);
		int sample_count = 0;
		const int max_samples = 10;

		for (s16 nx = 0; nx < MAP_BLOCKSIZE && sample_count < max_samples; nx += 4) {
			for (s16 ny = 0; ny < MAP_BLOCKSIZE && sample_count < max_samples; ny += 4) {
				for (s16 nz = 0; nz < MAP_BLOCKSIZE && sample_count < max_samples;
						nz += 4) {
					v3pos_t node_pos(nx, ny, nz);
					MapNode node = block->getNodeNoCheck(node_pos);

					if (node.getContent() != CONTENT_AIR &&
							node.getContent() != CONTENT_IGNORE) {
						Json::Value node_obj;
						node_obj["pos"]["x"] = nx;
						node_obj["pos"]["y"] = ny;
						node_obj["pos"]["z"] = nz;
						node_obj["content"] = static_cast<int>(node.getContent());
						node_obj["name"] = getNodeDefManager()->get(node).name;
						node_obj["world_pos"]["x"] = block_pos.X * MAP_BLOCKSIZE + nx;
						node_obj["world_pos"]["y"] = block_pos.Y * MAP_BLOCKSIZE + ny;
						node_obj["world_pos"]["z"] = block_pos.Z * MAP_BLOCKSIZE + nz;
						nodes_array.append(node_obj);
						sample_count++;
					}
				}
			}
		}

		if (sample_count > 0)
			block_obj["sampled_nodes"] = nodes_array;
		block_obj["sample_count"] = sample_count;
		blocks_array.append(block_obj);
		block_count++;
	}

	world_content["blocks"] = blocks_array;
	world_content["block_count"] = (int)blocks_array.size();
	world_content["truncated"] = candidates.size() > static_cast<size_t>(max_blocks);
	world_content["sampling"] =
			"nearest loaded blocks; up to 10 sampled non-air nodes per block";

	return world_content;
}

#endif

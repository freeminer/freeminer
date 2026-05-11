// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "client.h"

#include "chat.h"
#include "client/localplayer.h"
#include "clientmap.h"
#include "constants.h"
#include "inventory.h"
#include "inventorymanager.h"
#include "irr_v3d.h"
#include "log.h"
#include "mapblock.h"
#include "mapnode.h"
#include "mcp_player_control.h"
#include "nodedef.h"
#include "settings.h"
#include "util/numeric.h"
#include "util/pointedthing.h"
#include "version.h"

#include <algorithm>
#include <cmath>
#include <sstream>

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

static void setMCPTextResult(Json::Value &response, const Json::Value &value)
{
	Json::Value content_array(Json::arrayValue);
	Json::Value content_item;
	content_item["type"] = "text";
	content_item["text"] = Json::writeString(Json::StreamWriterBuilder(), value);
	content_array.append(content_item);

	Json::Value result;
	result["content"] = content_array;
	response["result"] = result;
}

static void setMCPEmptyResult(Json::Value &response)
{
	Json::Value result;
	result["content"] = Json::Value(Json::arrayValue);
	response["result"] = result;
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

void Client::onWebSocketMessage(
		websocketpp::connection_hdl hdl, Client::mcp_ws_server_t::message_ptr msg)
{
	Json::Value request;
	Json::CharReaderBuilder reader;
	std::string errors;

	std::unique_ptr<Json::CharReader> json_reader(reader.newCharReader());
	if (!json_reader->parse(msg->get_payload().c_str(),
				msg->get_payload().c_str() + msg->get_payload().length(), &request,
				&errors)) {
		Json::Value error_response;
		error_response["jsonrpc"] = "2.0";
		error_response["id"] = request.isMember("id") ? request["id"] : Json::Value();
		setMCPError(error_response, -32700, "Parse error: " + errors);
		m_mcp_websocket_server.send(hdl,
				Json::writeString(Json::StreamWriterBuilder(), error_response),
				websocketpp::frame::opcode::text);
		return;
	}

	Json::Value response;
	response["jsonrpc"] = "2.0";
	response["id"] = request.isMember("id") ? request["id"] : Json::Value();

	try {
		std::string method = request["method"].asString();

		if (method == "initialize") {
			Json::Value result;
			result["protocolVersion"] = "2024-11-05";
			result["serverInfo"]["name"] = "freeminer";
			result["serverInfo"]["version"] = VERSION_STRING;
			result["capabilities"]["tools"] = Json::Value(Json::objectValue);
			response["result"] = result;
		} else if (method == "tools/list") {
			Json::Value tools(Json::arrayValue);

			tools.append(makeMCPTool("get_player_state",
					"Get the current player state including position, velocity, health and breath."));
			tools.append(makeMCPTool("get_inventory",
					"Get the local player inventory lists and item stacks."));

			Json::Value send_chat_schema = makeMCPObjectSchema();
			addMCPSchemaProperty(send_chat_schema, "message", "string",
					"Chat message text to send. Commands may start with '/'.");
			addMCPRequired(send_chat_schema, "message");
			tools.append(makeMCPTool("send_chat_message",
					"Send a chat message as the local player.", send_chat_schema));

			Json::Value get_chat_schema = makeMCPObjectSchema();
			addMCPSchemaProperty(get_chat_schema, "count", "integer",
					"Maximum number of newest messages to return.");
			addMCPSchemaProperty(get_chat_schema, "buffer", "string",
					"Chat buffer to read: 'recent' or 'console'.");
			tools.append(makeMCPTool("get_chat_messages",
					"Get messages from the local chat buffer.", get_chat_schema));

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

			tools.append(makeMCPTool("get_node", "Get node data at a world position.",
					makeMCPPositionSchema()));
			tools.append(makeMCPTool("get_nodes_area",
					"Get node data for a bounded world area.", makeMCPAreaSchema()));

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
			tools.append(makeMCPTool("move_inventory_item",
					"Move an item stack inside the current player inventory.",
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
			tools.append(makeMCPTool("teleport_player",
					"Instantly teleport player to coordinates.",
					makeMCPPositionSchema()));
			tools.append(makeMCPTool(
					"get_pointed_thing", "Get the current pointed thing under cursor."));

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
				setMCPTextResult(response, player_state);
			} else if (tool_name == "get_inventory") {
				Json::Value inventory_obj;
				if (!player) {
					inventory_obj["success"] = false;
					inventory_obj["error"] = "No local player";
				} else {
					Json::Value lists(Json::arrayValue);
					for (const InventoryList *list : player->inventory.getLists())
						lists.append(inventoryListToMCPJson(list));
					inventory_obj["success"] = true;
					inventory_obj["wield_index"] =
							static_cast<int>(player->getWieldIndex());
					inventory_obj["hotbar_size"] =
							static_cast<int>(player->getMaxHotbarItemcount());
					inventory_obj["lists"] = lists;
				}
				setMCPTextResult(response, inventory_obj);
			} else if (tool_name == "send_chat_message") {
				Json::Value status;
				std::string message = args.get("message", "").asString();
				if (message.empty()) {
					status["success"] = false;
					status["error"] = "message is required";
				} else {
					typeChatMessage(utf8_to_wide(message));
					status["success"] = true;
					status["message"] = message;
				}
				setMCPTextResult(response, status);
			} else if (tool_name == "get_chat_messages") {
				Json::Value chat_obj;
				if (!chat_backend) {
					chat_obj["success"] = false;
					chat_obj["error"] = "Chat backend is not available";
				} else {
					const std::string buffer_name =
							args.get("buffer", "recent").asString();
					const int count = args.get("count", 20).asInt();
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
						chat_obj["error"] = "buffer must be 'recent' or 'console'";
					}
				}
				setMCPTextResult(response, chat_obj);
			} else if (tool_name == "set_player_control") {
				PlayerControl control;
				if (args.isMember("forward") && args["forward"].asBool())
					control.direction_keys |= 1;
				if (args.isMember("backward") && args["backward"].asBool())
					control.direction_keys |= 2;
				if (args.isMember("left") && args["left"].asBool())
					control.direction_keys |= 4;
				if (args.isMember("right") && args["right"].asBool())
					control.direction_keys |= 8;
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

				const u32 duration_ms =
						rangelim(args.get("duration_ms", 250).asUInt(), 50, 5000);
				setMCPPlayerControl(control, duration_ms);
				setMCPEmptyResult(response);
			} else if (tool_name == "get_node") {
				v3pos_t pos(args["x"].asInt(), args["y"].asInt(), args["z"].asInt());
				bool ok = false;
				MapNode node = m_env.getClientMap().getNode(pos, &ok);
				setMCPTextResult(
						response, nodeToMCPJson(pos, node, ok, getNodeDefManager()));
			} else if (tool_name == "get_nodes_area") {
				v3s16 minp(args["min_x"].asInt(), args["min_y"].asInt(),
						args["min_z"].asInt());
				v3s16 maxp(args["max_x"].asInt(), args["max_y"].asInt(),
						args["max_z"].asInt());
				if (maxp.X < minp.X)
					std::swap(maxp.X, minp.X);
				if (maxp.Y < minp.Y)
					std::swap(maxp.Y, minp.Y);
				if (maxp.Z < minp.Z)
					std::swap(maxp.Z, minp.Z);

				s64 volume = (s64)(maxp.X - minp.X + 1) * (maxp.Y - minp.Y + 1) *
							 (maxp.Z - minp.Z + 1);
				Json::Value area;
				area["success"] = volume <= 4096;
				area["volume"] = (Json::Int64)volume;
				if (volume > 4096) {
					area["error"] = "Requested area is too large";
				} else {
					Json::Value nodes(Json::arrayValue);
					for (auto x = minp.X; x <= maxp.X; x++) {
						for (auto y = minp.Y; y <= maxp.Y; y++) {
							for (auto z = minp.Z; z <= maxp.Z; z++) {
								v3pos_t pos(x, y, z);
								bool ok = false;
								MapNode node = m_env.getClientMap().getNode(pos, &ok);
								nodes.append(nodeToMCPJson(
										pos, node, ok, getNodeDefManager()));
							}
						}
					}
					area["nodes"] = nodes;
				}
				setMCPTextResult(response, area);
			} else if (tool_name == "set_wielded_item") {
				Json::Value status;
				selectMCPWieldedItem(this, player, args, status);
				setMCPTextResult(response, status);
			} else if (tool_name == "move_inventory_item") {
				Json::Value status;
				status["success"] = false;
				if (!args.isMember("from_index") || !args.isMember("to_index")) {
					status["error"] = "from_index and to_index are required";
				} else {
					IMoveAction *a = new IMoveAction();
					a->count = args.get("count", 0).asUInt();
					a->from_inv.setCurrentPlayer();
					a->from_list = args.get("from_list", "main").asString();
					a->from_i = args["from_index"].asInt();
					a->to_inv.setCurrentPlayer();
					a->to_list = args.get("to_list", "main").asString();
					a->to_i = args["to_index"].asInt();
					inventoryAction(a);
					status["success"] = true;
				}
				setMCPTextResult(response, status);
			} else if (tool_name == "craft") {
				ICraftAction *a = new ICraftAction();
				u16 count = args.get("count", 0).asUInt();
				a->count = count;
				a->craft_inv.setCurrentPlayer();
				inventoryAction(a);

				Json::Value status;
				status["success"] = true;
				status["count"] = count;
				setMCPTextResult(response, status);
			} else if (tool_name == "place_node") {
				Json::Value status;
				v3pos_t target(args["x"].asInt(), args["y"].asInt(), args["z"].asInt());
				if (!selectMCPWieldedItem(this, player, args, status)) {
					setMCPTextResult(response, status);
				} else {
					PointedThing pointed;
					if (makeMCPPlacePointedThing(this, target, args, pointed, status)) {
						interact(INTERACT_PLACE, pointed);
						status["success"] = true;
						status["target"]["x"] = target.X;
						status["target"]["y"] = target.Y;
						status["target"]["z"] = target.Z;
					}
					setMCPTextResult(response, status);
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
					PointedThing pointed = makeMCPNodePointedThing(this, pos, features);
					interact(INTERACT_START_DIGGING, pointed);
					interact(INTERACT_DIGGING_COMPLETED, pointed);
					status["success"] = true;
					status["node"] = nodeToMCPJson(pos, node, ok, getNodeDefManager());
				}
				setMCPTextResult(response, status);
			} else if (tool_name == "move_player_to" && m_mcp_player_control) {
				m_mcp_player_control->movePlayerTo(
						args["x"].asFloat(), args["y"].asFloat(), args["z"].asFloat());
				setMCPEmptyResult(response);
			} else if (tool_name == "rotate_player" && m_mcp_player_control) {
				m_mcp_player_control->rotatePlayer(
						args["pitch"].asFloat(), args["yaw"].asFloat());
				setMCPEmptyResult(response);
			} else if (tool_name == "teleport_player" && m_mcp_player_control) {
				m_mcp_player_control->teleportPlayer(
						args["x"].asFloat(), args["y"].asFloat(), args["z"].asFloat());
				setMCPEmptyResult(response);
			} else if (tool_name == "get_pointed_thing") {
				PointedThing pointed = getCurrentPointedThing();
				Json::Value pointed_obj;
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
			} else if (tool_name == "get_world_content") {
				int radius = args.get("radius", 5).asInt();
				setMCPTextResult(response, getWorldContentAroundPlayer(radius));
			} else {
				setMCPError(response, -32601, "Tool not found: " + tool_name);
			}
		} else {
			setMCPError(response, -32601, "Method not found: " + method);
		}
	} catch (const std::exception &e) {
		setMCPError(response, -32603, "Internal error: " + std::string(e.what()));
	}

	m_mcp_websocket_server.send(hdl,
			Json::writeString(Json::StreamWriterBuilder(), response),
			websocketpp::frame::opcode::text);
}

void Client::startMCPWebSocketServer(int port)
{
	if (m_websocket_server_running)
		return;

	try {
		m_mcp_websocket_server.init_asio();
		m_mcp_websocket_server.set_reuse_addr(true);
		m_mcp_websocket_server.clear_access_channels(websocketpp::log::alevel::all);

		m_mcp_websocket_server.set_message_handler(
				[this](websocketpp::connection_hdl hdl,
						mcp_ws_server_t::message_ptr msg) {
					this->onWebSocketMessage(hdl, msg);
				});

		m_mcp_websocket_server.set_open_handler([](websocketpp::connection_hdl) {
			infostream << "MCP WebSocket client connected" << std::endl;
		});

		m_mcp_websocket_server.set_close_handler([](websocketpp::connection_hdl) {
			infostream << "MCP WebSocket client disconnected" << std::endl;
		});

		m_mcp_websocket_server.listen("127.0.0.1", std::to_string(port));
		m_mcp_websocket_server.start_accept();

		m_websocket_server_running = true;
		m_websocket_server_thread = std::thread([this]() {
			try {
				m_mcp_websocket_server.run();
			} catch (const std::exception &e) {
				errorstream << "MCP WebSocket server error: " << e.what() << std::endl;
			}
		});

		infostream << "MCP WebSocket server started on 127.0.0.1:" << port << std::endl;
	} catch (const std::exception &e) {
		errorstream << "Failed to start MCP WebSocket server: " << e.what() << std::endl;
	}
}

void Client::stopMCPWebSocketServer()
{
	if (m_websocket_server_running) {
		m_websocket_server_running = false;
		m_mcp_websocket_server.stop_listening();
		m_mcp_websocket_server.stop();
		if (m_websocket_server_thread.joinable())
			m_websocket_server_thread.join();
		infostream << "MCP WebSocket server stopped" << std::endl;
	}
}

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
	return PointedThing();
}

Json::Value Client::getWorldContentAroundPlayer(int radius_blocks)
{
	Json::Value world_content;

	radius_blocks = std::min(radius_blocks, 3);

	LocalPlayer *player = m_env.getLocalPlayer();
	if (!player)
		return world_content;

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

	auto min_block =
			player_block_pos - v3pos_t(radius_blocks, radius_blocks, radius_blocks);
	auto max_block =
			player_block_pos + v3pos_t(radius_blocks, radius_blocks, radius_blocks);

	for (auto x = min_block.X; x <= max_block.X && block_count < max_blocks; x++) {
		for (auto y = min_block.Y; y <= max_block.Y && block_count < max_blocks; y++) {
			for (auto z = min_block.Z; z <= max_block.Z && block_count < max_blocks;
					z++) {
				v3pos_t block_pos(x, y, z);
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

				for (s16 nx = 0; nx < MAP_BLOCKSIZE && sample_count < max_samples;
						nx += 4) {
					for (s16 ny = 0; ny < MAP_BLOCKSIZE && sample_count < max_samples;
							ny += 4) {
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
		}
	}

	world_content["blocks"] = blocks_array;
	world_content["block_count"] = (int)blocks_array.size();

	return world_content;
}

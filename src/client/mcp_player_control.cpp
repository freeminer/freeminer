#include "config.h"
#if USE_CLIENT_MCP

#include "mcp_player_control.h"

#include "client.h"
#include "constants.h"
#include "irr_v3d.h"
#include "irrlichttypes.h"
#include "localplayer.h"
#include "settings.h"
#include <json/json.h>
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>

MCPPlayerControl::MCPPlayerControl(Client *client) : m_client(client)
{
	m_mcp_process = 0;
	m_mcp_stdin = -1;
	m_mcp_stdout = -1;

	if (!g_settings->get("mcp_stdio_command").empty())
		initializeMCP();
}

MCPPlayerControl::~MCPPlayerControl()
{
	if (m_mcp_process > 0) {
		kill(m_mcp_process, SIGTERM);
		waitpid(m_mcp_process, nullptr, 0);
	}

	if (m_mcp_stdin >= 0) {
		close(m_mcp_stdin);
	}
	if (m_mcp_stdout >= 0) {
		close(m_mcp_stdout);
	}
}

bool MCPPlayerControl::initializeMCP()
{
	std::string command = g_settings->get("mcp_stdio_command");
	if (command.empty())
		return false;

	// Create pipes for communication
	int stdin_pipe[2];
	int stdout_pipe[2];

	if (pipe(stdin_pipe) == -1 || pipe(stdout_pipe) == -1) {
		std::cerr << "Failed to create pipes for MCP communication" << std::endl;
		return false;
	}

	// Start the MCP server process in stdio mode
	m_mcp_process = fork();
	if (m_mcp_process == 0) {
		// Child process - redirect stdin/stdout to pipes and start MCP server
		close(stdin_pipe[1]);  // Close write end of stdin pipe
		close(stdout_pipe[0]); // Close read end of stdout pipe

		dup2(stdin_pipe[0], STDIN_FILENO);	 // Redirect stdin to read from pipe
		dup2(stdout_pipe[1], STDOUT_FILENO); // Redirect stdout to write to pipe

		close(stdin_pipe[0]);
		close(stdout_pipe[1]);

		execl("/bin/sh", "sh", "-c", command.c_str(), (char *)nullptr);
		_exit(1);
	} else if (m_mcp_process > 0) {
		// Parent process - close unused ends of pipes
		close(stdin_pipe[0]);  // Close read end of stdin pipe
		close(stdout_pipe[1]); // Close write end of stdout pipe

		m_mcp_stdin = stdin_pipe[1];   // Write end for sending to MCP server
		m_mcp_stdout = stdout_pipe[0]; // Read end for receiving from MCP server

		// Give the server time to start
		usleep(1000000); // 1 second

		std::cout << "Successfully started MCP server process" << std::endl;
		return true;
	}

	return false;
}

Json::Value MCPPlayerControl::sendMCPRequest(
		const std::string &method, const Json::Value &params)
{
	static int request_id = 1;

	Json::Value request;
	request["jsonrpc"] = "2.0";
	request["id"] = request_id++;
	request["method"] = method;
	request["params"] = params;

	// Send request via stdio
	Json::StreamWriterBuilder writer;
	std::string request_str = Json::writeString(writer, request) + "\n";

	if (m_mcp_stdin >= 0) {
		// Send via stdio
		ssize_t bytes_written =
				write(m_mcp_stdin, request_str.c_str(), request_str.length());
		if (bytes_written == -1) {
			std::cerr << "Failed to send request to MCP server" << std::endl;
			return Json::Value();
		}

		// Read response line by line
		std::string response_str;
		char buffer[1024];
		fd_set read_fds;
		struct timeval timeout;

		// Set up timeout
		timeout.tv_sec = 5; // 5 second timeout
		timeout.tv_usec = 0;

		FD_ZERO(&read_fds);
		FD_SET(m_mcp_stdout, &read_fds);

		// Wait for data to be available
		int select_result = select(m_mcp_stdout + 1, &read_fds, NULL, NULL, &timeout);
		if (select_result > 0 && FD_ISSET(m_mcp_stdout, &read_fds)) {
			// Read response
			ssize_t bytes_read = read(m_mcp_stdout, buffer, sizeof(buffer) - 1);
			if (bytes_read > 0) {
				buffer[bytes_read] = '\0';
				response_str = buffer;

				Json::CharReaderBuilder reader;
				Json::Value response;
				std::string errors;

				std::unique_ptr<Json::CharReader> jsonReader(reader.newCharReader());
				if (jsonReader->parse(response_str.c_str(),
							response_str.c_str() + response_str.length(), &response,
							&errors)) {
					return response;
				} else {
					std::cerr << "Failed to parse JSON response: " << errors << std::endl;
					std::cerr << "Response was: " << response_str << std::endl;
				}
			}
		} else if (select_result == 0) {
			std::cerr << "Timeout waiting for MCP server response" << std::endl;
		} else {
			std::cerr << "Error waiting for MCP server response" << std::endl;
		}
	}

	return Json::Value();
}

void MCPPlayerControl::updatePlayerState()
{
	// Get current player state from MCP server
	Json::Value call_params;
	call_params["name"] = "get_player_state";
	call_params["arguments"] = Json::Value();

	Json::Value response = sendMCPRequest("tools/call", call_params);

	if (response.isMember("result") && response["result"].isMember("content")) {
		// Parse the player state JSON
		std::string state_json = response["result"]["content"][0]["text"].asString();

		Json::CharReaderBuilder reader;
		Json::Value player_state;
		std::string errors;

		std::unique_ptr<Json::CharReader> jsonReader(reader.newCharReader());
		if (jsonReader->parse(state_json.c_str(),
					state_json.c_str() + state_json.length(), &player_state, &errors)) {
			// Update local player state
			LocalPlayer *player = m_client->getEnv().getLocalPlayer();
			if (player) {
				v3opos_t new_pos(player_state["position"]["x"].asFloat() * BS,
						player_state["position"]["y"].asFloat() * BS,
						player_state["position"]["z"].asFloat() * BS);

				// Update player position
				player->setPosition(new_pos);

				// Update player rotation
				player->setPitch(player_state["rotation"]["pitch"].asFloat());
				player->setYaw(player_state["rotation"]["yaw"].asFloat());
			}
		}
	}
}

void MCPPlayerControl::setPlayerControl(const PlayerControl &control)
{
	// Convert PlayerControl to MCP format
	Json::Value args;
	args["forward"] = (control.direction_keys & (1 << 0)) != 0;	 // up
	args["backward"] = (control.direction_keys & (1 << 1)) != 0; // down
	args["left"] = (control.direction_keys & (1 << 2)) != 0;	 // left
	args["right"] = (control.direction_keys & (1 << 3)) != 0;	 // right
	args["jump"] = control.jump;
	args["sneak"] = control.sneak;
	args["dig"] = control.dig;
	args["place"] = control.place;
	args["zoom"] = control.zoom;
	args["aux1"] = control.aux1;

	// Send control update to MCP server
	Json::Value params;
	params["name"] = "set_player_control";
	params["arguments"] = args;

	sendMCPRequest("tools/call", params);
}

void MCPPlayerControl::movePlayerTo(opos_t x, opos_t y, opos_t z)
{
	LocalPlayer *player = m_client->getEnv().getLocalPlayer();
	if (player) {
		player->setPosition(v3opos_t(x * BS, y * BS, z * BS));
		player->setSpeed(v3f(0.0f));
	}

	Json::Value args;
	args["x"] = x;
	args["y"] = y;
	args["z"] = z;

	Json::Value params;
	params["name"] = "move_player_to";
	params["arguments"] = args;

	sendMCPRequest("tools/call", params);
}

void MCPPlayerControl::rotatePlayer(float pitch, float yaw)
{
	// Update local player rotation directly
	LocalPlayer *player = m_client->getEnv().getLocalPlayer();
	if (player) {
		player->setPitch(pitch);
		player->setYaw(yaw);
	}

	// Also send to MCP server for consistency
	Json::Value args;
	args["pitch"] = pitch;
	args["yaw"] = yaw;

	Json::Value params;
	params["name"] = "rotate_player";
	params["arguments"] = args;

	sendMCPRequest("tools/call", params);
}

void MCPPlayerControl::teleportPlayer(float x, float y, float z)
{
	LocalPlayer *player = m_client->getEnv().getLocalPlayer();
	if (player) {
		player->setPosition(v3opos_t(x * BS, y * BS, z * BS));
		player->setSpeed(v3f(0.0f));
	}

	Json::Value args;
	args["x"] = x;
	args["y"] = y;
	args["z"] = z;

	Json::Value params;
	params["name"] = "teleport_player";
	params["arguments"] = args;

	sendMCPRequest("tools/call", params);
}

#endif

#pragma once

#include "client.h"
#include "localplayer.h"
#include <json/json.h>
#include <signal.h>

class MCPPlayerControl {
public:
    MCPPlayerControl(Client* client);
    ~MCPPlayerControl();
    
    // Update player state from MCP server
    void updatePlayerState();
    
    // Control methods
    void setPlayerControl(const PlayerControl& control);
    void movePlayerTo(float x, float y, float z);
    void rotatePlayer(float pitch, float yaw);
    void teleportPlayer(float x, float y, float z);
    
private:
    bool initializeMCP();
    Json::Value sendMCPRequest(const std::string& method, const Json::Value& params);
    
    Client* m_client;
    pid_t m_mcp_process = 0;
    int m_mcp_stdin = -1;
    int m_mcp_stdout = -1;
};

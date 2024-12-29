// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2017 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#pragma once

#include "cpp_api/s_base.h"
#include "cpp_api/s_client.h"
#include "cpp_api/s_modchannels.h"
#include "cpp_api/s_security.h"

class Client;
class LocalPlayer;
class Camera;
class Minimap;

class ClientScripting:
	virtual public ScriptApiBase,
	public ScriptApiSecurity,
	public ScriptApiClient,
	public ScriptApiModChannels
{
public:
	ClientScripting(Client *client);
	void on_client_ready(LocalPlayer *localplayer);
	void on_camera_ready(Camera *camera);
	void on_minimap_ready(Minimap *minimap);

private:
	virtual void InitializeModApi(lua_State *L, int top);
};

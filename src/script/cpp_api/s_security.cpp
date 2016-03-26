/*
Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "cpp_api/s_security.h"

#include "filesys.h"
#include "porting.h"
#include "server.h"
#include "settings.h"

#include <cerrno>
#include <string>
#include <iostream>


#define SECURE_API(lib, name) \
	lua_pushcfunction(L, sl_##lib##_##name); \
	lua_setfield(L, -2, #name);


static inline void copy_safe(lua_State *L, const char *list[], unsigned len, int from=-2, int to=-1)
{
	if (from < 0) from = lua_gettop(L) + from + 1;
	if (to   < 0) to   = lua_gettop(L) + to   + 1;
	for (unsigned i = 0; i < (len / sizeof(list[0])); i++) {
		lua_getfield(L, from, list[i]);
		lua_setfield(L, to,   list[i]);
	}
}

// Pushes the original version of a library function on the stack, from the old version
static inline void push_original(lua_State *L, const char *lib, const char *func)
{
	lua_rawgeti(L, LUA_REGISTRYINDEX, CUSTOM_RIDX_GLOBALS_BACKUP);
	lua_getfield(L, -1, lib);
	lua_remove(L, -2);  // Remove globals_backup
	lua_getfield(L, -1, func);
	lua_remove(L, -2);  // Remove lib
}


void ScriptApiSecurity::initializeSecurity()
{
	static const char *whitelist[] = {
		"assert",
		"core",
		"collectgarbage",
		"DIR_DELIM",
		"error",
		"getfenv",
		"getmetatable",
		"ipairs",
		"next",
		"pairs",
		"pcall",
		"print",
		"rawequal",
		"rawget",
		"rawset",
		"select",
		"setfenv",
		"setmetatable",
		"tonumber",
		"tostring",
		"type",
		"unpack",
		"_VERSION",
		"xpcall",
		// Completely safe libraries
		"coroutine",
		"string",
		"table",
		"math",
	};
	static const char *io_whitelist[] = {
		"close",
		"flush",
		"read",
		"type",
		"write",
	};
	static const char *os_whitelist[] = {
		"clock",
		"date",
		"difftime",
		"exit",
		"getenv",
		"setlocale",
		"time",
		"tmpname",
	};
	static const char *debug_whitelist[] = {
		"gethook",
		"traceback",
		"getinfo",
		"getmetatable",
		"setupvalue",
		"setmetatable",
		"upvalueid",
		"upvaluejoin",
		"sethook",
		"debug",
		"setlocal",
	};
	static const char *package_whitelist[] = {
		"config",
		"cpath",
		"path",
		"searchpath",
	};
	static const char *jit_whitelist[] = {
		"arch",
		"flush",
		"off",
		"on",
		"opt",
		"os",
		"status",
		"version",
		"version_num",
	};

	m_secure = true;

	lua_State *L = getStack();

	// Backup globals to the registry
	lua_getglobal(L, "_G");
	lua_rawseti(L, LUA_REGISTRYINDEX, CUSTOM_RIDX_GLOBALS_BACKUP);

	// Replace the global environment with an empty one
#if LUA_VERSION_NUM <= 501
	int is_main = lua_pushthread(L);  // Push the main thread
	FATAL_ERROR_IF(!is_main, "Security: ScriptApi's Lua state "
			"isn't the main Lua thread!");
#endif
	lua_newtable(L);  // Create new environment
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "_G");  // Set _G of new environment
#if LUA_VERSION_NUM >= 502  // Lua >= 5.2
	// Set the global environment
	lua_rawseti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
#else  // Lua <= 5.1
	// Set the environment of the main thread
	FATAL_ERROR_IF(!lua_setfenv(L, -2), "Security: Unable to set "
			"environment of the main Lua thread!");
	lua_pop(L, 1);  // Pop thread
#endif

	// Get old globals
	lua_rawgeti(L, LUA_REGISTRYINDEX, CUSTOM_RIDX_GLOBALS_BACKUP);
	int old_globals = lua_gettop(L);


	// Copy safe base functions
	lua_getglobal(L, "_G");
	copy_safe(L, whitelist, sizeof(whitelist));

	// And replace unsafe ones
	SECURE_API(g, dofile);
	SECURE_API(g, load);
	SECURE_API(g, loadfile);
	SECURE_API(g, loadstring);
	SECURE_API(g, require);
	lua_pop(L, 1);


	// Copy safe IO functions
	lua_getfield(L, old_globals, "io");
	lua_newtable(L);
	copy_safe(L, io_whitelist, sizeof(io_whitelist));

	// And replace unsafe ones
	SECURE_API(io, open);
	SECURE_API(io, input);
	SECURE_API(io, output);
	SECURE_API(io, lines);

	lua_setglobal(L, "io");
	lua_pop(L, 1);  // Pop old IO


	// Copy safe OS functions
	lua_getfield(L, old_globals, "os");
	lua_newtable(L);
	copy_safe(L, os_whitelist, sizeof(os_whitelist));

	// And replace unsafe ones
	SECURE_API(os, remove);
	SECURE_API(os, rename);

	lua_setglobal(L, "os");
	lua_pop(L, 1);  // Pop old OS


	// Copy safe debug functions
	lua_getfield(L, old_globals, "debug");
	lua_newtable(L);
	copy_safe(L, debug_whitelist, sizeof(debug_whitelist));
	lua_setglobal(L, "debug");
	lua_pop(L, 1);  // Pop old debug


	// Copy safe package fields
	lua_getfield(L, old_globals, "package");
	lua_newtable(L);
	copy_safe(L, package_whitelist, sizeof(package_whitelist));
	lua_setglobal(L, "package");
	lua_pop(L, 1);  // Pop old package


	// Copy safe jit functions, if they exist
	lua_getfield(L, -1, "jit");
	if (!lua_isnil(L, -1)) {
		lua_newtable(L);
		copy_safe(L, jit_whitelist, sizeof(jit_whitelist));
		lua_setglobal(L, "jit");
	}
	lua_pop(L, 1);  // Pop old jit

	lua_pop(L, 1); // Pop globals_backup
}


bool ScriptApiSecurity::isSecure(lua_State *L)
{
	lua_rawgeti(L, LUA_REGISTRYINDEX, CUSTOM_RIDX_GLOBALS_BACKUP);
	bool secure = !lua_isnil(L, -1);
	lua_pop(L, 1);
	return secure;
}


#define CHECK_FILE_ERR(ret, fp) \
	if (ret) { \
		if (fp) std::fclose(fp); \
		lua_pushfstring(L, "%s: %s", path, strerror(errno)); \
		return false; \
	}


bool ScriptApiSecurity::safeLoadFile(lua_State *L, const char *path)
{
	FILE *fp;
	char *chunk_name;
	if (path == NULL) {
		fp = stdin;
		chunk_name = const_cast<char *>("=stdin");
	} else {
		fp = fopen(path, "rb");
		if (!fp) {
			lua_pushfstring(L, "%s: %s", path, strerror(errno));
			return false;
		}
		chunk_name = new char[strlen(path) + 2];
		chunk_name[0] = '@';
		chunk_name[1] = '\0';
		strcat(chunk_name, path);
	}

	size_t start = 0;
	int c = std::getc(fp);
	if (c == '#') {
		// Skip the first line
		while ((c = std::getc(fp)) != EOF && c != '\n');
		if (c == '\n') c = std::getc(fp);
		start = std::ftell(fp);
	}

	if (c == LUA_SIGNATURE[0]) {
		lua_pushliteral(L, "Bytecode prohibited when mod security is enabled.");
		return false;
	}

	// Read the file
	int ret = std::fseek(fp, 0, SEEK_END);
	CHECK_FILE_ERR(ret, fp);
	if (ret) {
		std::fclose(fp);
		lua_pushfstring(L, "%s: %s", path, strerror(errno));
		return false;
	}
	size_t size = std::ftell(fp) - start;
	char *code = new char[size];
	ret = std::fseek(fp, start, SEEK_SET);
	CHECK_FILE_ERR(ret, fp);
	if (ret) {
		std::fclose(fp);
		lua_pushfstring(L, "%s: %s", path, strerror(errno));
		return false;
	}
	size_t num_read = std::fread(code, 1, size, fp);
	if (path) {
		std::fclose(fp);
	}
	if (num_read != size) {
		lua_pushliteral(L, "Error reading file to load.");
		return false;
	}

	if (luaL_loadbuffer(L, code, size, chunk_name)) {
		return false;
	}

	if (path) {
		delete [] chunk_name;
	}
	return true;
}


bool ScriptApiSecurity::checkPath(lua_State *L, const char *path)
{
	std::string str;  // Transient

	std::string norel_path = fs::RemoveRelativePathComponents(path);
	std::string abs_path = fs::AbsolutePath(norel_path);

	if (!abs_path.empty()) {
		// Don't allow accessing the settings file
		str = fs::AbsolutePath(g_settings_path);
		if (str == abs_path) return false;
	}

	// If we couldn't find the absolute path (path doesn't exist) then
	// try removing the last components until it works (to allow
	// non-existent files/folders for mkdir).
	std::string cur_path = norel_path;
	std::string removed;
	while (abs_path.empty() && !cur_path.empty()) {
		std::string tmp_rmed;
		cur_path = fs::RemoveLastPathComponent(cur_path, &tmp_rmed);
		removed = tmp_rmed + (removed.empty() ? "" : DIR_DELIM + removed);
		abs_path = fs::AbsolutePath(cur_path);
	}
	if (abs_path.empty()) return false;
	// Add the removed parts back so that you can't, eg, create a
	// directory in worldmods if worldmods doesn't exist.
	if (!removed.empty()) abs_path += DIR_DELIM + removed;

	// Get server from registry
	lua_rawgeti(L, LUA_REGISTRYINDEX, CUSTOM_RIDX_SCRIPTAPI);
	ScriptApiBase *script = (ScriptApiBase *) lua_touserdata(L, -1);
	lua_pop(L, 1);
	const Server *server = script->getServer();

	if (!server) return false;

	// Get mod name
	lua_rawgeti(L, LUA_REGISTRYINDEX, CUSTOM_RIDX_CURRENT_MOD_NAME);
	if (lua_isstring(L, -1)) {
		std::string mod_name = lua_tostring(L, -1);

		// Builtin can access anything
		if (mod_name == BUILTIN_MOD_NAME) {
			return true;
		}

		// Allow paths in mod path
		const ModSpec *mod = server->getModSpec(mod_name);
		if (mod) {
			str = fs::AbsolutePath(mod->path);
			if (!str.empty() && fs::PathStartsWith(abs_path, str)) {
				return true;
			}
		}
	}
	lua_pop(L, 1);  // Pop mod name

	str = fs::AbsolutePath(server->getWorldPath());
	if (str.empty()) return false;
	// Don't allow access to world mods.  We add to the absolute path
	// of the world instead of getting the absolute paths directly
	// because that won't work if they don't exist.
	if (fs::PathStartsWith(abs_path, str + DIR_DELIM + "worldmods") ||
			fs::PathStartsWith(abs_path, str + DIR_DELIM + "game")) {
		return false;
	}
	// Allow all other paths in world path
	if (fs::PathStartsWith(abs_path, str)) {
		return true;
	}

	// Default to disallowing
	return false;
}


int ScriptApiSecurity::sl_g_dofile(lua_State *L)
{
	int nret = sl_g_loadfile(L);
	if (nret != 1) {
		lua_error(L);
		// code after this function isn't executed
	}
	int top_precall = lua_gettop(L);
	lua_call(L, 0, LUA_MULTRET);
	// Return number of arguments returned by the function,
	// adjusting for the function being poped.
	return lua_gettop(L) - (top_precall - 1);
}


int ScriptApiSecurity::sl_g_load(lua_State *L)
{
	size_t len;
	const char *buf;
	std::string code;
	const char *chunk_name = "=(load)";

	luaL_checktype(L, 1, LUA_TFUNCTION);
	if (!lua_isnone(L, 2)) {
		luaL_checktype(L, 2, LUA_TSTRING);
		chunk_name = lua_tostring(L, 2);
	}

	while (true) {
		lua_pushvalue(L, 1);
		lua_call(L, 0, 1);
		int t = lua_type(L, -1);
		if (t == LUA_TNIL) {
			break;
		} else if (t != LUA_TSTRING) {
			lua_pushnil(L);
			lua_pushliteral(L, "Loader didn't return a string");
			return 2;
		}
		buf = lua_tolstring(L, -1, &len);
		code += std::string(buf, len);
		lua_pop(L, 1); // Pop return value
	}
	if (code[0] == LUA_SIGNATURE[0]) {
		lua_pushnil(L);
		lua_pushliteral(L, "Bytecode prohibited when mod security is enabled.");
		return 2;
	}
	if (luaL_loadbuffer(L, code.data(), code.size(), chunk_name)) {
		lua_pushnil(L);
		lua_insert(L, lua_gettop(L) - 1);
		return 2;
	}
	return 1;
}


int ScriptApiSecurity::sl_g_loadfile(lua_State *L)
{
	const char *path = NULL;

	if (lua_isstring(L, 1)) {
		path = lua_tostring(L, 1);
		CHECK_SECURE_PATH(L, path);
	}

	if (!safeLoadFile(L, path)) {
		lua_pushnil(L);
		lua_insert(L, -2);
		return 2;
	}

	return 1;
}


int ScriptApiSecurity::sl_g_loadstring(lua_State *L)
{
	const char *chunk_name = "=(load)";

	luaL_checktype(L, 1, LUA_TSTRING);
	if (!lua_isnone(L, 2)) {
		luaL_checktype(L, 2, LUA_TSTRING);
		chunk_name = lua_tostring(L, 2);
	}

	size_t size;
	const char *code = lua_tolstring(L, 1, &size);

	if (size > 0 && code[0] == LUA_SIGNATURE[0]) {
		lua_pushnil(L);
		lua_pushliteral(L, "Bytecode prohibited when mod security is enabled.");
		return 2;
	}
	if (luaL_loadbuffer(L, code, size, chunk_name)) {
		lua_pushnil(L);
		lua_insert(L, lua_gettop(L) - 1);
		return 2;
	}
	return 1;
}


int ScriptApiSecurity::sl_g_require(lua_State *L)
{
	lua_pushliteral(L, "require() is disabled when mod security is on.");
	return lua_error(L);
}


int ScriptApiSecurity::sl_io_open(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TSTRING);
	const char *path = lua_tostring(L, 1);
	CHECK_SECURE_PATH(L, path);

	push_original(L, "io", "open");
	lua_pushvalue(L, 1);
	lua_pushvalue(L, 2);
	lua_call(L, 2, 2);
	return 2;
}


int ScriptApiSecurity::sl_io_input(lua_State *L)
{
	if (lua_isstring(L, 1)) {
		const char *path = lua_tostring(L, 1);
		CHECK_SECURE_PATH(L, path);
	}

	push_original(L, "io", "input");
	lua_pushvalue(L, 1);
	lua_call(L, 1, 1);
	return 1;
}


int ScriptApiSecurity::sl_io_output(lua_State *L)
{
	if (lua_isstring(L, 1)) {
		const char *path = lua_tostring(L, 1);
		CHECK_SECURE_PATH(L, path);
	}

	push_original(L, "io", "output");
	lua_pushvalue(L, 1);
	lua_call(L, 1, 1);
	return 1;
}


int ScriptApiSecurity::sl_io_lines(lua_State *L)
{
	if (lua_isstring(L, 1)) {
		const char *path = lua_tostring(L, 1);
		CHECK_SECURE_PATH(L, path);
	}

	push_original(L, "io", "lines");
	lua_pushvalue(L, 1);
	int top_precall = lua_gettop(L);
	lua_call(L, 1, LUA_MULTRET);
	// Return number of arguments returned by the function,
	// adjusting for the function being poped.
	return lua_gettop(L) - (top_precall - 1);
}


int ScriptApiSecurity::sl_os_rename(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TSTRING);
	const char *path1 = lua_tostring(L, 1);
	CHECK_SECURE_PATH(L, path1);

	luaL_checktype(L, 2, LUA_TSTRING);
	const char *path2 = lua_tostring(L, 2);
	CHECK_SECURE_PATH(L, path2);

	push_original(L, "os", "rename");
	lua_pushvalue(L, 1);
	lua_pushvalue(L, 2);
	lua_call(L, 2, 2);
	return 2;
}


int ScriptApiSecurity::sl_os_remove(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TSTRING);
	const char *path = lua_tostring(L, 1);
	CHECK_SECURE_PATH(L, path);

	push_original(L, "os", "remove");
	lua_pushvalue(L, 1);
	lua_call(L, 1, 2);
	return 2;
}


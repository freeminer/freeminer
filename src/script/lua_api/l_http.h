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

#ifndef L_HTTP_H_
#define L_HTTP_H_

#include "lua_api/l_base.h"
#include "config.h"

struct HTTPFetchRequest;
struct HTTPFetchResult;

class ModApiHttp : public ModApiBase {
private:
#if USE_CURL
	// Helpers for HTTP fetch functions
	static void read_http_fetch_request(lua_State *L, HTTPFetchRequest &req);
	static void push_http_fetch_result(lua_State *L, HTTPFetchResult &res, bool completed = true);

	// http_fetch_async({url=, timeout=, post_data=})
	static int l_http_fetch_async(lua_State *L);

	// http_fetch_async_get(handle)
	static int l_http_fetch_async_get(lua_State *L);

	// request_http_api()
	static int l_request_http_api(lua_State *L);
#endif

public:
	static void Initialize(lua_State *L, int top);
};

#endif /* L_HTTP_H_ */

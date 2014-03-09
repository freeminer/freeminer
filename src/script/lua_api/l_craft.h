/*
script/lua_api/l_craft.h
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

#ifndef L_CRAFT_H_
#define L_CRAFT_H_

#include <string>
#include <vector>

#include "lua_api/l_base.h"

struct CraftReplacements;

class ModApiCraft : public ModApiBase {
private:
	static int l_register_craft(lua_State *L);
	static int l_get_craft_recipe(lua_State *L);
	static int l_get_all_craft_recipes(lua_State *L);
	static int l_get_craft_result(lua_State *L);

	static bool readCraftReplacements(lua_State *L, int index,
			CraftReplacements &replacements);
	static bool readCraftRecipeShapeless(lua_State *L, int index,
			std::vector<std::string> &recipe);
	static bool readCraftRecipeShaped(lua_State *L, int index,
			int &width, std::vector<std::string> &recipe);

	static struct EnumString es_CraftMethod[];

public:
	static void Initialize(lua_State *L, int top);
};

#endif /* L_CRAFT_H_ */

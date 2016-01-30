/*
craftdef.cpp
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

#include "craftdef.h"

#include "irrlichttypes.h"
#include "log.h"
#include <sstream>
#include <set>
#include <algorithm>
#include "gamedef.h"
#include "inventory.h"
#include "util/serialize.h"
#include "util/string.h"
#include "util/numeric.h"
#include "strfnd.h"
#include "exceptions.h"

inline bool isGroupRecipeStr(const std::string &rec_name)
{
	return str_starts_with(rec_name, std::string("group:"));
}

inline u64 getHashForString(const std::string &recipe_str)
{
	/*errorstream << "Hashing craft string  \"" << recipe_str << '"';*/
	return murmur_hash_64_ua(recipe_str.data(), recipe_str.length(), 0xdeadbeef);
}

static u64 getHashForGrid(CraftHashType type, const std::vector<std::string> &grid_names)
{
	switch (type) {
		case CRAFT_HASH_TYPE_ITEM_NAMES: {
			std::ostringstream os;
			bool is_first = true;
			for (size_t i = 0; i < grid_names.size(); i++) {
				if (grid_names[i] != "") {
					os << (is_first ? "" : "\n") << grid_names[i];
					is_first = false;
				}
			}
			return getHashForString(os.str());
		} case CRAFT_HASH_TYPE_COUNT: {
			u64 cnt = 0;
			for (size_t i = 0; i < grid_names.size(); i++)
				if (grid_names[i] != "")
					cnt++;
			return cnt;
		} case CRAFT_HASH_TYPE_UNHASHED:
			return 0;
	}
	// invalid CraftHashType
	assert(false);
	return 0;
}

// Check if input matches recipe
// Takes recipe groups into account
static bool inputItemMatchesRecipe(const std::string &inp_name,
		const std::string &rec_name, IItemDefManager *idef)
{
	// Exact name
	if (inp_name == rec_name)
		return true;

	// Group
	if (isGroupRecipeStr(rec_name) && idef->isKnown(inp_name)) {
		const struct ItemDefinition &def = idef->get(inp_name);
		Strfnd f(rec_name.substr(6));
		bool all_groups_match = true;
		do {
			std::string check_group = f.next(",");
			if (itemgroup_get(def.groups, check_group) == 0) {
				all_groups_match = false;
				break;
			}
		} while (!f.atend());
		if (all_groups_match)
			return true;
	}

	// Didn't match
	return false;
}

// Deserialize an itemstring then return the name of the item
static std::string craftGetItemName(const std::string &itemstring, IGameDef *gamedef)
{
	ItemStack item;
	item.deSerialize(itemstring, gamedef->idef());
	return item.name;
}

// (mapcar craftGetItemName itemstrings)
static std::vector<std::string> craftGetItemNames(
		const std::vector<std::string> &itemstrings, IGameDef *gamedef)
{
	std::vector<std::string> result;
	for (std::vector<std::string>::size_type i = 0;
			i < itemstrings.size(); i++) {
		result.push_back(craftGetItemName(itemstrings[i], gamedef));
	}
	return result;
}

// Get name of each item, and return them as a new list.
static std::vector<std::string> craftGetItemNames(
		const std::vector<ItemStack> &items, IGameDef *gamedef)
{
	std::vector<std::string> result;
	for (std::vector<ItemStack>::size_type i = 0;
			i < items.size(); i++) {
		result.push_back(items[i].name);
	}
	return result;
}

// convert a list of item names, to ItemStacks.
static std::vector<ItemStack> craftGetItems(
		const std::vector<std::string> &items, IGameDef *gamedef)
{
	std::vector<ItemStack> result;
	for (std::vector<std::string>::size_type i = 0;
			i < items.size(); i++) {
		result.push_back(ItemStack(std::string(items[i]), (u16)1,
			(u16)0, "", gamedef->getItemDefManager()));
	}
	return result;
}

// Compute bounding rectangle given a matrix of items
// Returns false if every item is ""
static bool craftGetBounds(const std::vector<std::string> &items, unsigned int width,
		unsigned int &min_x, unsigned int &max_x,
		unsigned int &min_y, unsigned int &max_y)
{
	bool success = false;
	unsigned int x = 0;
	unsigned int y = 0;
	for (std::vector<std::string>::size_type i = 0;
			i < items.size(); i++) {
		// Is this an actual item?
		if (items[i] != "") {
			if (!success) {
				// This is the first nonempty item
				min_x = max_x = x;
				min_y = max_y = y;
				success = true;
			} else {
				if (x < min_x) min_x = x;
				if (x > max_x) max_x = x;
				if (y < min_y) min_y = y;
				if (y > max_y) max_y = y;
			}
		}

		// Step coordinate
		x++;
		if (x == width) {
			x = 0;
			y++;
		}
	}
	return success;
}

// Removes 1 from each item stack
static void craftDecrementInput(CraftInput &input, IGameDef *gamedef)
{
	for (std::vector<ItemStack>::size_type i = 0;
			i < input.items.size(); i++) {
		if (input.items[i].count != 0)
			input.items[i].remove(1);
	}
}

// Removes 1 from each item stack with replacement support
// Example: if replacements contains the pair ("bucket:bucket_water", "bucket:bucket_empty"),
//   a water bucket will not be removed but replaced by an empty bucket.
static void craftDecrementOrReplaceInput(CraftInput &input,
		std::vector<ItemStack> &output_replacements,
		const CraftReplacements &replacements,
		IGameDef *gamedef)
{
	if (replacements.pairs.empty()) {
		craftDecrementInput(input, gamedef);
		return;
	}

	// Make a copy of the replacements pair list
	std::vector<std::pair<std::string, std::string> > pairs = replacements.pairs;

	for (std::vector<ItemStack>::size_type i = 0;
			i < input.items.size(); i++) {
		ItemStack &item = input.items[i];
		// Find an appropriate replacement
		bool found_replacement = false;
		for (std::vector<std::pair<std::string, std::string> >::iterator
				j = pairs.begin();
				j != pairs.end(); ++j) {
			if (inputItemMatchesRecipe(item.name, j->first, gamedef->idef())) {
				if (item.count == 1) {
					item.deSerialize(j->second, gamedef->idef());
					found_replacement = true;
					pairs.erase(j);
					break;
				} else {
					ItemStack rep;
					rep.deSerialize(j->second, gamedef->idef());
					item.remove(1);
					found_replacement = true;
					output_replacements.push_back(rep);
					break;
				}
			}
		}
		// No replacement was found, simply decrement count by one
		if (!found_replacement && item.count > 0)
			item.remove(1);
	}
}

// Dump an itemstring matrix
static std::string craftDumpMatrix(const std::vector<std::string> &items,
		unsigned int width)
{
	std::ostringstream os(std::ios::binary);
	os << "{ ";
	unsigned int x = 0;
	for(std::vector<std::string>::size_type i = 0;
			i < items.size(); i++, x++) {
		if (x == width) {
			os << "; ";
			x = 0;
		} else if (x != 0) {
			os << ",";
		}
		os << '"' << items[i] << '"';
	}
	os << " }";
	return os.str();
}

// Dump an item matrix
std::string craftDumpMatrix(const std::vector<ItemStack> &items,
		unsigned int width)
{
	std::ostringstream os(std::ios::binary);
	os << "{ ";
	unsigned int x = 0;
	for (std::vector<ItemStack>::size_type i = 0;
			i < items.size(); i++, x++) {
		if (x == width) {
			os << "; ";
			x = 0;
		} else if (x != 0) {
			os << ",";
		}
		os << '"' << (items[i].getItemString()) << '"';
	}
	os << " }";
	return os.str();
}


/*
	CraftInput
*/

std::string CraftInput::dump() const
{
	std::ostringstream os(std::ios::binary);
	os << "(method=" << ((int)method) << ", items="
		<< craftDumpMatrix(items, width) << ")";
	return os.str();
}

/*
	CraftOutput
*/

std::string CraftOutput::dump() const
{
	std::ostringstream os(std::ios::binary);
	os << "(item=\"" << item << "\", time=" << time << ")";
	return os.str();
}

/*
	CraftReplacements
*/

std::string CraftReplacements::dump() const
{
	std::ostringstream os(std::ios::binary);
	os<<"{";
	const char *sep = "";
	for (std::vector<std::pair<std::string, std::string> >::size_type i = 0;
			i < pairs.size(); i++) {
		const std::pair<std::string, std::string> &repl_p = pairs[i];
		os << sep
			<< '"' << (repl_p.first)
			<< "\"=>\"" << (repl_p.second) << '"';
		sep = ",";
	}
	os << "}";
	return os.str();
}

/*
	CraftDefinitionShaped
*/

std::string CraftDefinitionShaped::getName() const
{
	return "shaped";
}

bool CraftDefinitionShaped::check(const CraftInput &input, IGameDef *gamedef) const
{
	if (input.method != CRAFT_METHOD_NORMAL)
		return false;

	// Get input item matrix
	std::vector<std::string> inp_names = craftGetItemNames(input.items, gamedef);
	unsigned int inp_width = input.width;
	if (inp_width == 0)
		return false;
	while (inp_names.size() % inp_width != 0)
		inp_names.push_back("");

	// Get input bounds
	unsigned int inp_min_x = 0, inp_max_x = 0, inp_min_y = 0, inp_max_y = 0;
	if (!craftGetBounds(inp_names, inp_width, inp_min_x, inp_max_x,
			inp_min_y, inp_max_y))
		return false;  // it was empty

	std::vector<std::string> rec_names;
	if (hash_inited)
		rec_names = recipe_names;
	else
		rec_names = craftGetItemNames(recipe, gamedef);

	// Get recipe item matrix
	unsigned int rec_width = width;
	if (rec_width == 0)
		return false;
	while (rec_names.size() % rec_width != 0)
		rec_names.push_back("");

	// Get recipe bounds
	unsigned int rec_min_x=0, rec_max_x=0, rec_min_y=0, rec_max_y=0;
	if (!craftGetBounds(rec_names, rec_width, rec_min_x, rec_max_x,
			rec_min_y, rec_max_y))
		return false;  // it was empty

	// Different sizes?
	if (inp_max_x - inp_min_x != rec_max_x - rec_min_x ||
			inp_max_y - inp_min_y != rec_max_y - rec_min_y)
		return false;

	// Verify that all item names in the bounding box are equal
	unsigned int w = inp_max_x - inp_min_x + 1;
	unsigned int h = inp_max_y - inp_min_y + 1;

	for (unsigned int y=0; y < h; y++) {
		unsigned int inp_y = (inp_min_y + y) * inp_width;
		unsigned int rec_y = (rec_min_y + y) * rec_width;

		for (unsigned int x=0; x < w; x++) {
			unsigned int inp_x = inp_min_x + x;
			unsigned int rec_x = rec_min_x + x;

			if (!inputItemMatchesRecipe(
					inp_names[inp_y + inp_x],
					rec_names[rec_y + rec_x], gamedef->idef())) {
				return false;
			}
		}
	}

	return true;
}

CraftOutput CraftDefinitionShaped::getOutput(const CraftInput &input, IGameDef *gamedef) const
{
	return CraftOutput(output, 0);
}

CraftInput CraftDefinitionShaped::getInput(const CraftOutput &output, IGameDef *gamedef) const
{
	return CraftInput(CRAFT_METHOD_NORMAL,width,craftGetItems(recipe,gamedef));
}

void CraftDefinitionShaped::decrementInput(CraftInput &input, std::vector<ItemStack> &output_replacements,
	 IGameDef *gamedef) const
{
	craftDecrementOrReplaceInput(input, output_replacements, replacements, gamedef);
}

CraftHashType CraftDefinitionShaped::getHashType() const
{
	assert(hash_inited); // Pre-condition
	bool has_group = false;
	for (size_t i = 0; i < recipe_names.size(); i++) {
		if (isGroupRecipeStr(recipe_names[i])) {
			has_group = true;
			break;
		}
	}
	if (has_group)
		return CRAFT_HASH_TYPE_COUNT;
	else
		return CRAFT_HASH_TYPE_ITEM_NAMES;
}

u64 CraftDefinitionShaped::getHash(CraftHashType type) const
{
	assert(hash_inited); // Pre-condition
	assert((type == CRAFT_HASH_TYPE_ITEM_NAMES)
		|| (type == CRAFT_HASH_TYPE_COUNT)); // Pre-condition

	std::vector<std::string> rec_names = recipe_names;
	std::sort(rec_names.begin(), rec_names.end());
	return getHashForGrid(type, rec_names);
}

void CraftDefinitionShaped::initHash(IGameDef *gamedef)
{
	if (hash_inited)
		return;
	hash_inited = true;
	recipe_names = craftGetItemNames(recipe, gamedef);
}

std::string CraftDefinitionShaped::dump() const
{
	std::ostringstream os(std::ios::binary);
	os << "(shaped, output=\"" << output
		<< "\", recipe=" << craftDumpMatrix(recipe, width)
		<< ", replacements=" << replacements.dump() << ")";
	return os.str();
}

/*
	CraftDefinitionShapeless
*/

std::string CraftDefinitionShapeless::getName() const
{
	return "shapeless";
}

bool CraftDefinitionShapeless::check(const CraftInput &input, IGameDef *gamedef) const
{
	if (input.method != CRAFT_METHOD_NORMAL)
		return false;

	// Filter empty items out of input
	std::vector<std::string> input_filtered;
	for (std::vector<ItemStack>::size_type i = 0;
			i < input.items.size(); i++) {
		const ItemStack &item = input.items[i];
		if (item.name != "")
			input_filtered.push_back(item.name);
	}

	// If there is a wrong number of items in input, no match
	if (input_filtered.size() != recipe.size()) {
		/*dstream<<"Number of input items ("<<input_filtered.size()
				<<") does not match recipe size ("<<recipe.size()<<") "
				<<"of recipe with output="<<output<<std::endl;*/
		return false;
	}

	std::vector<std::string> recipe_copy;
	if (hash_inited)
		recipe_copy = recipe_names;
	else {
		recipe_copy = craftGetItemNames(recipe, gamedef);
		std::sort(recipe_copy.begin(), recipe_copy.end());
	}

	// Try with all permutations of the recipe,
	// start from the lexicographically first permutation (=sorted),
	// recipe_names is pre-sorted
	do {
		// If all items match, the recipe matches
		bool all_match = true;
		//dstream<<"Testing recipe (output="<<output<<"):";
		for (size_t i=0; i<recipe.size(); i++) {
			//dstream<<" ("<<input_filtered[i]<<" == "<<recipe_copy[i]<<")";
			if (!inputItemMatchesRecipe(input_filtered[i], recipe_copy[i],
					gamedef->idef())) {
				all_match = false;
				break;
			}
		}
		//dstream<<" -> match="<<all_match<<std::endl;
		if (all_match)
			return true;
	} while (std::next_permutation(recipe_copy.begin(), recipe_copy.end()));

	return false;
}

CraftOutput CraftDefinitionShapeless::getOutput(const CraftInput &input, IGameDef *gamedef) const
{
	return CraftOutput(output, 0);
}

CraftInput CraftDefinitionShapeless::getInput(const CraftOutput &output, IGameDef *gamedef) const
{
	return CraftInput(CRAFT_METHOD_NORMAL, 0, craftGetItems(recipe, gamedef));
}

void CraftDefinitionShapeless::decrementInput(CraftInput &input, std::vector<ItemStack> &output_replacements,
	IGameDef *gamedef) const
{
	craftDecrementOrReplaceInput(input, output_replacements, replacements, gamedef);
}

CraftHashType CraftDefinitionShapeless::getHashType() const
{
	assert(hash_inited); // Pre-condition
	bool has_group = false;
	for (size_t i = 0; i < recipe_names.size(); i++) {
		if (isGroupRecipeStr(recipe_names[i])) {
			has_group = true;
			break;
		}
	}
	if (has_group)
		return CRAFT_HASH_TYPE_COUNT;
	else
		return CRAFT_HASH_TYPE_ITEM_NAMES;
}

u64 CraftDefinitionShapeless::getHash(CraftHashType type) const
{
	assert(hash_inited); // Pre-condition
	assert(type == CRAFT_HASH_TYPE_ITEM_NAMES
		|| type == CRAFT_HASH_TYPE_COUNT); // Pre-condition
	return getHashForGrid(type, recipe_names);
}

void CraftDefinitionShapeless::initHash(IGameDef *gamedef)
{
	if (hash_inited)
		return;
	hash_inited = true;
	recipe_names = craftGetItemNames(recipe, gamedef);
	std::sort(recipe_names.begin(), recipe_names.end());
}

std::string CraftDefinitionShapeless::dump() const
{
	std::ostringstream os(std::ios::binary);
	os << "(shapeless, output=\"" << output
		<< "\", recipe=" << craftDumpMatrix(recipe, recipe.size())
		<< ", replacements=" << replacements.dump() << ")";
	return os.str();
}

/*
	CraftDefinitionToolRepair
*/

static ItemStack craftToolRepair(
		const ItemStack &item1,
		const ItemStack &item2,
		float additional_wear,
		IGameDef *gamedef)
{
	IItemDefManager *idef = gamedef->idef();
	if (item1.count != 1 || item2.count != 1 || item1.name != item2.name
			|| idef->get(item1.name).type != ITEM_TOOL
			|| idef->get(item2.name).type != ITEM_TOOL) {
		// Failure
		return ItemStack();
	}

	s32 item1_uses = 65536 - (u32) item1.wear;
	s32 item2_uses = 65536 - (u32) item2.wear;
	s32 new_uses = item1_uses + item2_uses;
	s32 new_wear = 65536 - new_uses + floor(additional_wear * 65536 + 0.5);
	if (new_wear >= 65536)
		return ItemStack();
	if (new_wear < 0)
		new_wear = 0;

	ItemStack repaired = item1;
	repaired.wear = new_wear;
	return repaired;
}

std::string CraftDefinitionToolRepair::getName() const
{
	return "toolrepair";
}

bool CraftDefinitionToolRepair::check(const CraftInput &input, IGameDef *gamedef) const
{
	if (input.method != CRAFT_METHOD_NORMAL)
		return false;

	ItemStack item1;
	ItemStack item2;
	for (std::vector<ItemStack>::size_type i = 0;
			i < input.items.size(); i++) {
		const ItemStack &item = input.items[i];
		if (!item.empty()) {
			if (item1.empty())
				item1 = item;
			else if (item2.empty())
				item2 = item;
			else
				return false;
		}
	}
	ItemStack repaired = craftToolRepair(item1, item2, additional_wear, gamedef);
	return !repaired.empty();
}

CraftOutput CraftDefinitionToolRepair::getOutput(const CraftInput &input, IGameDef *gamedef) const
{
	ItemStack item1;
	ItemStack item2;
	for (std::vector<ItemStack>::size_type i = 0;
			i < input.items.size(); i++) {
		const ItemStack &item = input.items[i];
		if (!item.empty()) {
			if (item1.empty())
				item1 = item;
			else if (item2.empty())
				item2 = item;
		}
	}
	ItemStack repaired = craftToolRepair(item1, item2, additional_wear, gamedef);
	return CraftOutput(repaired.getItemString(), 0);
}

CraftInput CraftDefinitionToolRepair::getInput(const CraftOutput &output, IGameDef *gamedef) const
{
	std::vector<ItemStack> stack;
	stack.push_back(ItemStack());
	return CraftInput(CRAFT_METHOD_COOKING, additional_wear, stack);
}

void CraftDefinitionToolRepair::decrementInput(CraftInput &input, std::vector<ItemStack> &output_replacements,
	IGameDef *gamedef) const
{
	craftDecrementInput(input, gamedef);
}

std::string CraftDefinitionToolRepair::dump() const
{
	std::ostringstream os(std::ios::binary);
	os << "(toolrepair, additional_wear=" << additional_wear << ")";
	return os.str();
}

/*
	CraftDefinitionCooking
*/

std::string CraftDefinitionCooking::getName() const
{
	return "cooking";
}

bool CraftDefinitionCooking::check(const CraftInput &input, IGameDef *gamedef) const
{
	if (input.method != CRAFT_METHOD_COOKING)
		return false;

	// Filter empty items out of input
	std::vector<std::string> input_filtered;
	for (std::vector<ItemStack>::size_type i = 0;
			i < input.items.size(); i++) {
		const std::string &name = input.items[i].name;
		if (name != "")
			input_filtered.push_back(name);
	}

	// If there is a wrong number of items in input, no match
	if (input_filtered.size() != 1) {
		/*dstream<<"Number of input items ("<<input_filtered.size()
				<<") does not match recipe size (1) "
				<<"of cooking recipe with output="<<output<<std::endl;*/
		return false;
	}

	// Check the single input item
	return inputItemMatchesRecipe(input_filtered[0], recipe, gamedef->idef());
}

CraftOutput CraftDefinitionCooking::getOutput(const CraftInput &input, IGameDef *gamedef) const
{
	return CraftOutput(output, cooktime);
}

CraftInput CraftDefinitionCooking::getInput(const CraftOutput &output, IGameDef *gamedef) const
{
	std::vector<std::string> rec;
	rec.push_back(recipe);
	return CraftInput(CRAFT_METHOD_COOKING,cooktime,craftGetItems(rec,gamedef));
}

void CraftDefinitionCooking::decrementInput(CraftInput &input, std::vector<ItemStack> &output_replacements,
	IGameDef *gamedef) const
{
	craftDecrementOrReplaceInput(input, output_replacements, replacements, gamedef);
}

CraftHashType CraftDefinitionCooking::getHashType() const
{
	if (isGroupRecipeStr(recipe_name))
		return CRAFT_HASH_TYPE_COUNT;
	else
		return CRAFT_HASH_TYPE_ITEM_NAMES;
}

u64 CraftDefinitionCooking::getHash(CraftHashType type) const
{
	if (type == CRAFT_HASH_TYPE_ITEM_NAMES) {
		return getHashForString(recipe_name);
	} else if (type == CRAFT_HASH_TYPE_COUNT) {
		return 1;
	} else {
		//illegal hash type for this CraftDefinition (pre-condition)
		assert(false);
		return 0;
	}
}

void CraftDefinitionCooking::initHash(IGameDef *gamedef)
{
	if (hash_inited)
		return;
	hash_inited = true;
	recipe_name = craftGetItemName(recipe, gamedef);
}

std::string CraftDefinitionCooking::dump() const
{
	std::ostringstream os(std::ios::binary);
	os << "(cooking, output=\"" << output
		<< "\", recipe=\"" << recipe
		<< "\", cooktime=" << cooktime << ")"
		<< ", replacements=" << replacements.dump() << ")";
	return os.str();
}

/*
	CraftDefinitionFuel
*/

std::string CraftDefinitionFuel::getName() const
{
	return "fuel";
}

bool CraftDefinitionFuel::check(const CraftInput &input, IGameDef *gamedef) const
{
	if (input.method != CRAFT_METHOD_FUEL)
		return false;

	// Filter empty items out of input
	std::vector<std::string> input_filtered;
	for (std::vector<ItemStack>::size_type i = 0;
			i < input.items.size(); i++) {
		const std::string &name = input.items[i].name;
		if (name != "")
			input_filtered.push_back(name);
	}

	// If there is a wrong number of items in input, no match
	if (input_filtered.size() != 1) {
		/*dstream<<"Number of input items ("<<input_filtered.size()
				<<") does not match recipe size (1) "
				<<"of fuel recipe with burntime="<<burntime<<std::endl;*/
		return false;
	}

	// Check the single input item
	return inputItemMatchesRecipe(input_filtered[0], recipe, gamedef->idef());
}

CraftOutput CraftDefinitionFuel::getOutput(const CraftInput &input, IGameDef *gamedef) const
{
	return CraftOutput("", burntime);
}

CraftInput CraftDefinitionFuel::getInput(const CraftOutput &output, IGameDef *gamedef) const
{
	std::vector<std::string> rec;
	rec.push_back(recipe);
	return CraftInput(CRAFT_METHOD_COOKING,(int)burntime,craftGetItems(rec,gamedef));
}

void CraftDefinitionFuel::decrementInput(CraftInput &input, std::vector<ItemStack> &output_replacements,
	IGameDef *gamedef) const
{
	craftDecrementOrReplaceInput(input, output_replacements, replacements, gamedef);
}

CraftHashType CraftDefinitionFuel::getHashType() const
{
	if (isGroupRecipeStr(recipe_name))
		return CRAFT_HASH_TYPE_COUNT;
	else
		return CRAFT_HASH_TYPE_ITEM_NAMES;
}

u64 CraftDefinitionFuel::getHash(CraftHashType type) const
{
	if (type == CRAFT_HASH_TYPE_ITEM_NAMES) {
		return getHashForString(recipe_name);
	} else if (type == CRAFT_HASH_TYPE_COUNT) {
		return 1;
	} else {
		//illegal hash type for this CraftDefinition (pre-condition)
		assert(false);
		return 0;
	}
}

void CraftDefinitionFuel::initHash(IGameDef *gamedef)
{
	if (hash_inited)
		return;
	hash_inited = true;
	recipe_name = craftGetItemName(recipe, gamedef);
}
std::string CraftDefinitionFuel::dump() const
{
	std::ostringstream os(std::ios::binary);
	os << "(fuel, recipe=\"" << recipe
		<< "\", burntime=" << burntime << ")"
		<< ", replacements=" << replacements.dump() << ")";
	return os.str();
}

/*
	Craft definition manager
*/

class CCraftDefManager: public IWritableCraftDefManager
{
public:
	CCraftDefManager()
	{
		m_craft_defs.resize(craft_hash_type_max + 1);
	}

	virtual ~CCraftDefManager()
	{
		clear();
	}

	virtual bool getCraftResult(CraftInput &input, CraftOutput &output,
			std::vector<ItemStack> &output_replacement, bool decrementInput,
			IGameDef *gamedef) const
	{
		output.item = "";
		output.time = 0;

		// If all input items are empty, abort.
		bool all_empty = true;
		for (std::vector<ItemStack>::size_type i = 0;
			i < input.items.size(); i++) {
			if (!input.items[i].empty()) {
				all_empty = false;
				break;
			}
		}
		if (all_empty)
			return false;

		std::vector<std::string> input_names;
		input_names = craftGetItemNames(input.items, gamedef);
		std::sort(input_names.begin(), input_names.end());

		// Try hash types with increasing collision rate, and return if found.
		for (int type = 0; type <= craft_hash_type_max; type++) {
			u64 hash = getHashForGrid((CraftHashType) type, input_names);

			/*errorstream << "Checking type " << type << " with hash " << hash << std::endl;*/

			// We'd like to do "const [...] hash_collisions = m_craft_defs[type][hash];"
			// but that doesn't compile for some reason. This does.
			std::map<u64, std::vector<CraftDefinition*> >::const_iterator
				col_iter = (m_craft_defs[type]).find(hash);

			if (col_iter == (m_craft_defs[type]).end())
				continue;

			const std::vector<CraftDefinition*> &hash_collisions = col_iter->second;
			// Walk crafting definitions from back to front, so that later
			// definitions can override earlier ones.
			for (std::vector<CraftDefinition*>::size_type
					i = hash_collisions.size(); i > 0; i--) {
				CraftDefinition *def = hash_collisions[i - 1];

				/*errorstream << "Checking " << input.dump() << std::endl
					<< " against " << def->dump() << std::endl;*/

				if (def->check(input, gamedef)) {
					// Get output, then decrement input (if requested)
					output = def->getOutput(input, gamedef);
					if (decrementInput)
						def->decrementInput(input, output_replacement, gamedef);
					/*errorstream << "Check RETURNS TRUE" << std::endl;*/
					return true;
				}
			}
		}
		return false;
	}

	virtual std::vector<CraftDefinition*> getCraftRecipes(CraftOutput &output,
			IGameDef *gamedef, unsigned limit=0) const
	{
		std::vector<CraftDefinition*> recipes;

		std::map<std::string, std::vector<CraftDefinition*> >::const_iterator
			vec_iter = m_output_craft_definitions.find(output.item);

		if (vec_iter == m_output_craft_definitions.end())
			return recipes;

		const std::vector<CraftDefinition*> &vec = vec_iter->second;

		recipes.reserve(limit ? MYMIN(limit, vec.size()) : vec.size());

		for (std::vector<CraftDefinition*>::size_type i = vec.size();
				i > 0; i--) {
			CraftDefinition *def = vec[i - 1];
			if (limit && recipes.size() >= limit)
				break;
			recipes.push_back(def);
		}

		return recipes;
	}
	virtual std::string dump() const
	{
		std::ostringstream os(std::ios::binary);
		os << "Crafting definitions:\n";
		for (int type = 0; type <= craft_hash_type_max; ++type) {
			for (std::map<u64, std::vector<CraftDefinition*> >::const_iterator
					it = (m_craft_defs[type]).begin();
					it != (m_craft_defs[type]).end(); ++it) {
				for (std::vector<CraftDefinition*>::size_type i = 0;
						i < it->second.size(); i++) {
					os << "type " << type
						<< " hash " << it->first
						<< " def " << it->second[i]->dump()
						<< "\n";
				}
			}
		}
		return os.str();
	}
	virtual void registerCraft(CraftDefinition *def, IGameDef *gamedef)
	{
		verbosestream << "registerCraft: registering craft definition: "
				<< def->dump() << std::endl;
		m_craft_defs[(int) CRAFT_HASH_TYPE_UNHASHED][0].push_back(def);

		CraftInput input;
		std::string output_name = craftGetItemName(
				def->getOutput(input, gamedef).item, gamedef);
		m_output_craft_definitions[output_name].push_back(def);
	}
	virtual void clear()
	{
		for (int type = 0; type <= craft_hash_type_max; ++type) {
			for (std::map<u64, std::vector<CraftDefinition*> >::iterator
					it = m_craft_defs[type].begin();
					it != m_craft_defs[type].end(); ++it) {
				for (std::vector<CraftDefinition*>::iterator
						iit = it->second.begin();
						iit != it->second.end(); ++iit) {
					delete *iit;
				}
				it->second.clear();
			}
			m_craft_defs[type].clear();
		}
		m_output_craft_definitions.clear();
	}
	virtual void initHashes(IGameDef *gamedef)
	{
		// Move the CraftDefs from the unhashed layer into layers higher up.
		std::vector<CraftDefinition *> &unhashed =
			m_craft_defs[(int) CRAFT_HASH_TYPE_UNHASHED][0];
		for (std::vector<CraftDefinition*>::size_type i = 0;
			i < unhashed.size(); i++) {
			CraftDefinition *def = unhashed[i];

			// Initialize and get the definition's hash
			def->initHash(gamedef);
			CraftHashType type = def->getHashType();
			u64 hash = def->getHash(type);

			// Enter the definition
			m_craft_defs[type][hash].push_back(def);
		}
		unhashed.clear();
	}
private:
	//TODO: change both maps to unordered_map when c++11 can be used
	std::vector<std::map<u64, std::vector<CraftDefinition*> > > m_craft_defs;
	std::map<std::string, std::vector<CraftDefinition*> > m_output_craft_definitions;
};

IWritableCraftDefManager* createCraftDefManager()
{
	return new CCraftDefManager();
}


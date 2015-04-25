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
	/*errorstream << "Hashing craft string  \"" << recipe_str << "\"";*/
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
}

// Check if input matches recipe
// Takes recipe groups into account
static bool inputItemMatchesRecipe(const std::string &inp_name,
		const std::string &rec_name, IItemDefManager *idef)
{
	// Exact name
	if(inp_name == rec_name)
		return true;

	// Group
	if (isGroupRecipeStr(rec_name) && idef->isKnown(inp_name)) {
		const struct ItemDefinition &def = idef->get(inp_name);
		Strfnd f(rec_name.substr(6));
		bool all_groups_match = true;
		do{
			std::string check_group = f.next(",");
			if(itemgroup_get(def.groups, check_group) == 0){
				all_groups_match = false;
				break;
			}
		}while(!f.atend());
		if(all_groups_match)
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
	for(std::vector<std::string>::const_iterator
			i = itemstrings.begin();
			i != itemstrings.end(); i++)
	{
		result.push_back(craftGetItemName(*i, gamedef));
	}
	return result;
}

// Get name of each item, and return them as a new list.
static std::vector<std::string> craftGetItemNames(
		const std::vector<ItemStack> &items, IGameDef *gamedef)
{
	std::vector<std::string> result;
	for(std::vector<ItemStack>::const_iterator
			i = items.begin();
			i != items.end(); i++)
	{
		result.push_back(i->name);
	}
	return result;
}

// convert a list of item names, to ItemStacks.
static std::vector<ItemStack> craftGetItems(
		const std::vector<std::string> &items, IGameDef *gamedef)
{
	std::vector<ItemStack> result;
	for(std::vector<std::string>::const_iterator
			i = items.begin();
			i != items.end(); i++)
	{
		result.push_back(ItemStack(std::string(*i),(u16)1,(u16)0,"",gamedef->getItemDefManager()));
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
	for(std::vector<std::string>::const_iterator
			i = items.begin();
			i != items.end(); i++)
	{
		if(*i != "")  // Is this an actual item?
		{
			if(!success)
			{
				// This is the first nonempty item
				min_x = max_x = x;
				min_y = max_y = y;
				success = true;
			}
			else
			{
				if(x < min_x) min_x = x;
				if(x > max_x) max_x = x;
				if(y < min_y) min_y = y;
				if(y > max_y) max_y = y;
			}
		}

		// Step coordinate
		x++;
		if(x == width)
		{
			x = 0;
			y++;
		}
	}
	return success;
}

// Removes 1 from each item stack
static void craftDecrementInput(CraftInput &input, IGameDef *gamedef)
{
	for(std::vector<ItemStack>::iterator
			i = input.items.begin();
			i != input.items.end(); i++)
	{
		if(i->count != 0)
			i->remove(1);
	}
}

// Removes 1 from each item stack with replacement support
// Example: if replacements contains the pair ("bucket:bucket_water", "bucket:bucket_empty"),
//   a water bucket will not be removed but replaced by an empty bucket.
static void craftDecrementOrReplaceInput(CraftInput &input,
		const CraftReplacements &replacements,
		IGameDef *gamedef)
{
	if(replacements.pairs.empty())
	{
		craftDecrementInput(input, gamedef);
		return;
	}

	// Make a copy of the replacements pair list
	std::vector<std::pair<std::string, std::string> > pairs = replacements.pairs;

	for(std::vector<ItemStack>::iterator
			i = input.items.begin();
			i != input.items.end(); i++)
	{
		if(i->count == 1)
		{
			// Find an appropriate replacement
			bool found_replacement = false;
			for(std::vector<std::pair<std::string, std::string> >::iterator
					j = pairs.begin();
					j != pairs.end(); j++)
			{
				ItemStack from_item;
				from_item.deSerialize(j->first, gamedef->idef());
				if(i->name == from_item.name)
				{
					i->deSerialize(j->second, gamedef->idef());
					found_replacement = true;
					pairs.erase(j);
					break;
				}
			}
			// No replacement was found, simply decrement count to zero
			if(!found_replacement)
				i->remove(1);
		}
		else if(i->count >= 2)
		{
			// Ignore replacements for items with count >= 2
			i->remove(1);
		}
	}
}

// Dump an itemstring matrix
static std::string craftDumpMatrix(const std::vector<std::string> &items,
		unsigned int width)
{
	std::ostringstream os(std::ios::binary);
	os<<"{ ";
	unsigned int x = 0;
	for(std::vector<std::string>::const_iterator
			i = items.begin();
			i != items.end(); i++, x++)
	{
		if(x == width)
		{
			os<<"; ";
			x = 0;
		}
		else if(x != 0)
		{
			os<<",";
		}
		os<<"\""<<(*i)<<"\"";
	}
	os<<" }";
	return os.str();
}

// Dump an item matrix
std::string craftDumpMatrix(const std::vector<ItemStack> &items,
		unsigned int width)
{
	std::ostringstream os(std::ios::binary);
	os<<"{ ";
	unsigned int x = 0;
	for(std::vector<ItemStack>::const_iterator
			i = items.begin();
			i != items.end(); i++, x++)
	{
		if(x == width)
		{
			os<<"; ";
			x = 0;
		}
		else if(x != 0)
		{
			os<<",";
		}
		os<<"\""<<(i->getItemString())<<"\"";
	}
	os<<" }";
	return os.str();
}


/*
	CraftInput
*/

std::string CraftInput::dump() const
{
	std::ostringstream os(std::ios::binary);
	os<<"(method="<<((int)method)<<", items="<<craftDumpMatrix(items, width)<<")";
	return os.str();
}

/*
	CraftOutput
*/

std::string CraftOutput::dump() const
{
	std::ostringstream os(std::ios::binary);
	os<<"(item=\""<<item<<"\", time="<<time<<")";
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
	for(std::vector<std::pair<std::string, std::string> >::const_iterator
			i = pairs.begin();
			i != pairs.end(); i++)
	{
		os<<sep<<"\""<<(i->first)<<"\"=>\""<<(i->second)<<"\"";
		sep = ",";
	}
	os<<"}";
	return os.str();
}

void CraftReplacements::serialize(std::ostream &os) const
{
	writeU16(os, pairs.size());
	for(u32 i=0; i<pairs.size(); i++)
	{
		os<<serializeString(pairs[i].first);
		os<<serializeString(pairs[i].second);
	}
}

void CraftReplacements::deSerialize(std::istream &is)
{
	pairs.clear();
	u32 count = readU16(is);
	for(u32 i=0; i<count; i++)
	{
		std::string first = deSerializeString(is);
		std::string second = deSerializeString(is);
		pairs.push_back(std::make_pair(first, second));
	}
}

/*
	CraftDefinition
*/

void CraftDefinition::serialize(std::ostream &os) const
{
	writeU8(os, 1); // version
	os<<serializeString(getName());
	serializeBody(os);
}

CraftDefinition* CraftDefinition::deSerialize(std::istream &is)
{
	int version = readU8(is);
	if(version != 1) throw SerializationError(
			"unsupported CraftDefinition version");
	std::string name = deSerializeString(is);
	CraftDefinition *def = NULL;
	if(name == "shaped")
	{
		def = new CraftDefinitionShaped;
	}
	else if(name == "shapeless")
	{
		def = new CraftDefinitionShapeless;
	}
	else if(name == "toolrepair")
	{
		def = new CraftDefinitionToolRepair;
	}
	else if(name == "cooking")
	{
		def = new CraftDefinitionCooking;
	}
	else if(name == "fuel")
	{
		def = new CraftDefinitionFuel;
	}
	else
	{
		infostream<<"Unknown CraftDefinition name=\""<<name<<"\""<<std::endl;
                throw SerializationError("Unknown CraftDefinition name");
	}
	def->deSerializeBody(is, version);
	return def;
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
	if(input.method != CRAFT_METHOD_NORMAL)
		return false;

	// Get input item matrix
	std::vector<std::string> inp_names = craftGetItemNames(input.items, gamedef);
	unsigned int inp_width = input.width;
	if(inp_width == 0)
		return false;
	while(inp_names.size() % inp_width != 0)
		inp_names.push_back("");

	// Get input bounds
	unsigned int inp_min_x=0, inp_max_x=0, inp_min_y=0, inp_max_y=0;
	if(!craftGetBounds(inp_names, inp_width, inp_min_x, inp_max_x, inp_min_y, inp_max_y))
		return false;  // it was empty

	std::vector<std::string> rec_names;
	if (hash_inited)
		rec_names = recipe_names;
	else
		rec_names = craftGetItemNames(recipe, gamedef);

	// Get recipe item matrix
	unsigned int rec_width = width;
	if(rec_width == 0)
		return false;
	while(rec_names.size() % rec_width != 0)
		rec_names.push_back("");

	// Get recipe bounds
	unsigned int rec_min_x=0, rec_max_x=0, rec_min_y=0, rec_max_y=0;
	if(!craftGetBounds(rec_names, rec_width, rec_min_x, rec_max_x, rec_min_y, rec_max_y))
		return false;  // it was empty

	// Different sizes?
	if(inp_max_x - inp_min_x != rec_max_x - rec_min_x ||
			inp_max_y - inp_min_y != rec_max_y - rec_min_y)
		return false;

	// Verify that all item names in the bounding box are equal
	unsigned int w = inp_max_x - inp_min_x + 1;
	unsigned int h = inp_max_y - inp_min_y + 1;

	for(unsigned int y=0; y < h; y++) {
		unsigned int inp_y = (inp_min_y + y) * inp_width;
		unsigned int rec_y = (rec_min_y + y) * rec_width;

		for(unsigned int x=0; x < w; x++) {
			unsigned int inp_x = inp_min_x + x;
			unsigned int rec_x = rec_min_x + x;

			if(!inputItemMatchesRecipe(
					inp_names[inp_y + inp_x],
					rec_names[rec_y + rec_x], gamedef->idef())
			) {
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

void CraftDefinitionShaped::decrementInput(CraftInput &input, IGameDef *gamedef) const
{
	craftDecrementOrReplaceInput(input, replacements, gamedef);
}

CraftHashType CraftDefinitionShaped::getHashType() const
{
	assert(hash_inited); //pre-condition
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
	assert(hash_inited); //pre-condition
	if ((type == CRAFT_HASH_TYPE_ITEM_NAMES) || (type == CRAFT_HASH_TYPE_COUNT)) {
		std::vector<std::string> rec_names = recipe_names;
		std::sort(rec_names.begin(), rec_names.end());
		return getHashForGrid(type, rec_names);
	} else {
		//illegal hash type for this CraftDefinition (pre-condition)
		assert(false);
		return 0;
	}
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
	os<<"(shaped, output=\""<<output
		<<"\", recipe="<<craftDumpMatrix(recipe, width)
		<<", replacements="<<replacements.dump()<<")";
	return os.str();
}

void CraftDefinitionShaped::serializeBody(std::ostream &os) const
{
	os<<serializeString(output);
	writeU16(os, width);
	writeU16(os, recipe.size());
	for(u32 i=0; i<recipe.size(); i++)
		os<<serializeString(recipe[i]);
	replacements.serialize(os);
}

void CraftDefinitionShaped::deSerializeBody(std::istream &is, int version)
{
	if(version != 1) throw SerializationError(
			"unsupported CraftDefinitionShaped version");
	output = deSerializeString(is);
	width = readU16(is);
	recipe.clear();
	u32 count = readU16(is);
	for(u32 i=0; i<count; i++)
		recipe.push_back(deSerializeString(is));
	replacements.deSerialize(is);
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
	if(input.method != CRAFT_METHOD_NORMAL)
		return false;
	
	// Filter empty items out of input
	std::vector<std::string> input_filtered;
	for(std::vector<ItemStack>::const_iterator
			i = input.items.begin();
			i != input.items.end(); i++)
	{
		if(i->name != "")
			input_filtered.push_back(i->name);
	}

	// If there is a wrong number of items in input, no match
	if(input_filtered.size() != recipe.size()){
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
		for(size_t i=0; i<recipe.size(); i++){
			//dstream<<" ("<<input_filtered[i]<<" == "<<recipe_copy[i]<<")";
			if(!inputItemMatchesRecipe(input_filtered[i], recipe_copy[i],
					gamedef->idef())){
				all_match = false;
				break;
			}
		}
		//dstream<<" -> match="<<all_match<<std::endl;
		if(all_match)
			return true;
	}while(std::next_permutation(recipe_copy.begin(), recipe_copy.end()));

	return false;
}

CraftOutput CraftDefinitionShapeless::getOutput(const CraftInput &input, IGameDef *gamedef) const
{
	return CraftOutput(output, 0);
}

CraftInput CraftDefinitionShapeless::getInput(const CraftOutput &output, IGameDef *gamedef) const
{
	return CraftInput(CRAFT_METHOD_NORMAL,0,craftGetItems(recipe,gamedef));
}

void CraftDefinitionShapeless::decrementInput(CraftInput &input, IGameDef *gamedef) const
{
	craftDecrementOrReplaceInput(input, replacements, gamedef);
}

CraftHashType CraftDefinitionShapeless::getHashType() const
{
	assert(hash_inited); //pre-condition
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
	assert(hash_inited); //pre-condition
	if (type == CRAFT_HASH_TYPE_ITEM_NAMES || type == CRAFT_HASH_TYPE_COUNT) {
		return getHashForGrid(type, recipe_names);
	} else {
		//illegal hash type for this CraftDefinition (pre-condition)
		assert(false);
		return 0;
	}
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
	os<<"(shapeless, output=\""<<output
		<<"\", recipe="<<craftDumpMatrix(recipe, recipe.size())
		<<", replacements="<<replacements.dump()<<")";
	return os.str();
}

void CraftDefinitionShapeless::serializeBody(std::ostream &os) const
{
	os<<serializeString(output);
	writeU16(os, recipe.size());
	for(u32 i=0; i<recipe.size(); i++)
		os<<serializeString(recipe[i]);
	replacements.serialize(os);
}

void CraftDefinitionShapeless::deSerializeBody(std::istream &is, int version)
{
	if(version != 1) throw SerializationError(
			"unsupported CraftDefinitionShapeless version");
	output = deSerializeString(is);
	recipe.clear();
	u32 count = readU16(is);
	for(u32 i=0; i<count; i++)
		recipe.push_back(deSerializeString(is));
	replacements.deSerialize(is);
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
	if(item1.count != 1 || item2.count != 1 || item1.name != item2.name
			|| idef->get(item1.name).type != ITEM_TOOL
			|| idef->get(item2.name).type != ITEM_TOOL)
	{
		// Failure
		return ItemStack();
	}

	s32 item1_uses = 65536 - (u32) item1.wear;
	s32 item2_uses = 65536 - (u32) item2.wear;
	s32 new_uses = item1_uses + item2_uses;
	s32 new_wear = 65536 - new_uses + floor(additional_wear * 65536 + 0.5);
	if(new_wear >= 65536)
		return ItemStack();
	if(new_wear < 0)
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
	if(input.method != CRAFT_METHOD_NORMAL)
		return false;

	ItemStack item1;
	ItemStack item2;
	for(std::vector<ItemStack>::const_iterator
			i = input.items.begin();
			i != input.items.end(); i++)
	{
		if(!i->empty())
		{
			if(item1.empty())
				item1 = *i;
			else if(item2.empty())
				item2 = *i;
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
	for(std::vector<ItemStack>::const_iterator
			i = input.items.begin();
			i != input.items.end(); i++)
	{
		if(!i->empty())
		{
			if(item1.empty())
				item1 = *i;
			else if(item2.empty())
				item2 = *i;
		}
	}
	ItemStack repaired = craftToolRepair(item1, item2, additional_wear, gamedef);
	return CraftOutput(repaired.getItemString(), 0);
}

CraftInput CraftDefinitionToolRepair::getInput(const CraftOutput &output, IGameDef *gamedef) const
{
	std::vector<ItemStack> stack;
	stack.push_back(ItemStack());
	return CraftInput(CRAFT_METHOD_COOKING,additional_wear,stack);
}

void CraftDefinitionToolRepair::decrementInput(CraftInput &input, IGameDef *gamedef) const
{
	craftDecrementInput(input, gamedef);
}

std::string CraftDefinitionToolRepair::dump() const
{
	std::ostringstream os(std::ios::binary);
	os<<"(toolrepair, additional_wear="<<additional_wear<<")";
	return os.str();
}

void CraftDefinitionToolRepair::serializeBody(std::ostream &os) const
{
	writeF1000(os, additional_wear);
}

void CraftDefinitionToolRepair::deSerializeBody(std::istream &is, int version)
{
	if(version != 1) throw SerializationError(
			"unsupported CraftDefinitionToolRepair version");
	additional_wear = readF1000(is);
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
	if(input.method != CRAFT_METHOD_COOKING)
		return false;

	// Filter empty items out of input
	std::vector<std::string> input_filtered;
	for(std::vector<ItemStack>::const_iterator
			i = input.items.begin();
			i != input.items.end(); i++)
	{
		if(i->name != "")
			input_filtered.push_back(i->name);
	}

	// If there is a wrong number of items in input, no match
	if(input_filtered.size() != 1){
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

void CraftDefinitionCooking::decrementInput(CraftInput &input, IGameDef *gamedef) const
{
	craftDecrementOrReplaceInput(input, replacements, gamedef);
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
	os<<"(cooking, output=\""<<output
		<<"\", recipe=\""<<recipe
		<<"\", cooktime="<<cooktime<<")"
		<<", replacements="<<replacements.dump()<<")";
	return os.str();
}

void CraftDefinitionCooking::serializeBody(std::ostream &os) const
{
	os<<serializeString(output);
	os<<serializeString(recipe);
	writeF1000(os, cooktime);
	replacements.serialize(os);
}

void CraftDefinitionCooking::deSerializeBody(std::istream &is, int version)
{
	if(version != 1) throw SerializationError(
			"unsupported CraftDefinitionCooking version");
	output = deSerializeString(is);
	recipe = deSerializeString(is);
	cooktime = readF1000(is);
	replacements.deSerialize(is);
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
	if(input.method != CRAFT_METHOD_FUEL)
		return false;

	// Filter empty items out of input
	std::vector<std::string> input_filtered;
	for(std::vector<ItemStack>::const_iterator
			i = input.items.begin();
			i != input.items.end(); i++)
	{
		if(i->name != "")
			input_filtered.push_back(i->name);
	}

	// If there is a wrong number of items in input, no match
	if(input_filtered.size() != 1){
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

void CraftDefinitionFuel::decrementInput(CraftInput &input, IGameDef *gamedef) const
{
	craftDecrementOrReplaceInput(input, replacements, gamedef);
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
	os<<"(fuel, recipe=\""<<recipe
		<<"\", burntime="<<burntime<<")"
		<<", replacements="<<replacements.dump()<<")";
	return os.str();
}

void CraftDefinitionFuel::serializeBody(std::ostream &os) const
{
	os<<serializeString(recipe);
	writeF1000(os, burntime);
	replacements.serialize(os);
}

void CraftDefinitionFuel::deSerializeBody(std::istream &is, int version)
{
	if(version != 1) throw SerializationError(
			"unsupported CraftDefinitionFuel version");
	recipe = deSerializeString(is);
	burntime = readF1000(is);
	replacements.deSerialize(is);
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
			bool decrementInput, IGameDef *gamedef) const
	{
		output.item = "";
		output.time = 0;

		// If all input items are empty, abort.
		bool all_empty = true;
		for (std::vector<ItemStack>::const_iterator
				i = input.items.begin();
				i != input.items.end(); i++) {
			if (!i->empty()) {
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
			for (std::vector<CraftDefinition*>::const_reverse_iterator
					i = hash_collisions.rbegin();
					i != hash_collisions.rend(); i++) {
				CraftDefinition *def = *i;

				/*errorstream << "Checking " << input.dump() << std::endl
					<< " against " << def->dump() << std::endl;*/

				if (def->check(input, gamedef)) {
					// Get output, then decrement input (if requested)
					output = def->getOutput(input, gamedef);
					if (decrementInput)
						def->decrementInput(input, gamedef);
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

		for (std::vector<CraftDefinition*>::const_reverse_iterator
				it = vec.rbegin(); it != vec.rend(); ++it) {
			if (limit && recipes.size() >= limit)
				break;
			recipes.push_back(*it);
		}

		return recipes;
	}
	virtual std::string dump() const
	{
		std::ostringstream os(std::ios::binary);
		os << "Crafting definitions:\n";
		for (int type = 0; type <= craft_hash_type_max; type++) {
			for (std::map<u64, std::vector<CraftDefinition*> >::const_iterator
					i = (m_craft_defs[type]).begin();
					i != (m_craft_defs[type]).end(); i++) {
				for (std::vector<CraftDefinition*>::const_iterator
						ii = i->second.begin(); ii != i->second.end(); ii++) {
					os << "type " << type << " hash " << i->first << (*ii)->dump() << "\n";
				}
			}
		}
		return os.str();
	}
	virtual void registerCraft(CraftDefinition *def, IGameDef *gamedef)
	{
		verbosestream<<"registerCraft: registering craft definition: "
				<<def->dump()<<std::endl;
		m_craft_defs[(int) CRAFT_HASH_TYPE_UNHASHED][0].push_back(def);

		CraftInput input;
		std::string output_name = craftGetItemName(
				def->getOutput(input, gamedef).item, gamedef);
		m_output_craft_definitions[output_name].push_back(def);
	}
	virtual void clear()
	{
		for (int type = 0; type <= craft_hash_type_max; type++) {
			for (std::map<u64, std::vector<CraftDefinition*> >::iterator
					i = m_craft_defs[type].begin();
					i != m_craft_defs[type].end(); i++) {
				for (std::vector<CraftDefinition*>::iterator
						ii = i->second.begin(); ii != i->second.end(); ii++) {
					delete *ii;
				}
				i->second.clear();
			}
			m_craft_defs[type].clear();
		}
		m_output_craft_definitions.clear();
	}
	virtual void initHashes(IGameDef *gamedef)
	{
		// Move the CraftDefs from the unhashed layer into layers higher up.
		for (std::vector<CraftDefinition*>::iterator
			i = (m_craft_defs[(int) CRAFT_HASH_TYPE_UNHASHED][0]).begin();
			i != (m_craft_defs[(int) CRAFT_HASH_TYPE_UNHASHED][0]).end(); i++) {
			CraftDefinition *def = *i;

			// Initialize and get the definition's hash
			def->initHash(gamedef);
			CraftHashType type = def->getHashType();
			u64 hash = def->getHash(type);

			// Enter the definition
			m_craft_defs[type][hash].push_back(def);
		}
		m_craft_defs[(int) CRAFT_HASH_TYPE_UNHASHED][0].clear();
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


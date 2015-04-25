/*
craftdef.h
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

#ifndef CRAFTDEF_HEADER
#define CRAFTDEF_HEADER

#include <string>
#include <iostream>
#include <vector>
#include <utility>
#include "gamedef.h"
#include "inventory.h"

/*
	Crafting methods.

	The crafting method depends on the inventory list
	that the crafting input comes from.
*/
enum CraftMethod
{
	// Crafting grid
	CRAFT_METHOD_NORMAL,
	// Cooking something in a furnace
	CRAFT_METHOD_COOKING,
	// Using something as fuel for a furnace
	CRAFT_METHOD_FUEL,
};

/*
	The type a hash can be. The earlier a type is mentioned in this enum,
	the earlier it is tried at crafting, and the less likely is a collision.
	Changing order causes changes in behaviour, so know what you do.
 */
enum CraftHashType
{
	// Hashes the normalized names of the recipe's elements.
	// Only recipes without group usage can be found here,
	// because groups can't be guessed efficiently.
	CRAFT_HASH_TYPE_ITEM_NAMES,

	// Counts the non-empty slots.
	CRAFT_HASH_TYPE_COUNT,

	// This layer both spares an extra variable, and helps to retain (albeit rarely used) functionality. Maps to 0.
	// Before hashes are "initialized", all hashes reside here, after initialisation, none are.
	CRAFT_HASH_TYPE_UNHASHED

};
const int craft_hash_type_max = (int) CRAFT_HASH_TYPE_UNHASHED;

/*
	Input: The contents of the crafting slots, arranged in matrix form
*/
struct CraftInput
{
	CraftMethod method;
	unsigned int width;
	std::vector<ItemStack> items;

	CraftInput():
		method(CRAFT_METHOD_NORMAL), width(0), items()
	{}
	CraftInput(CraftMethod method_, unsigned int width_,
			const std::vector<ItemStack> &items_):
		method(method_), width(width_), items(items_)
	{}
	std::string dump() const;
};

/*
	Output: Result of crafting operation
*/
struct CraftOutput
{
	// Used for normal crafting and cooking, itemstring
	std::string item;
	// Used for cooking (cook time) and fuel (burn time), seconds
	float time;

	CraftOutput():
		item(""), time(0)
	{}
	CraftOutput(std::string item_, float time_):
		item(item_), time(time_)
	{}
	std::string dump() const;
};

/*
	A list of replacements. A replacement indicates that a specific
	input item should not be deleted (when crafting) but replaced with
	a different item. Each replacements is a pair (itemstring to remove,
	itemstring to replace with)

	Example: If ("bucket:bucket_water", "bucket:bucket_empty") is a
	replacement pair, the crafting input slot that contained a water
	bucket will contain an empty bucket after crafting.

	Note: replacements only work correctly when stack_max of the item
	to be replaced is 1. It is up to the mod writer to ensure this.
*/
struct CraftReplacements
{
	// List of replacements
	std::vector<std::pair<std::string, std::string> > pairs;

	CraftReplacements():
		pairs()
	{}
	CraftReplacements(std::vector<std::pair<std::string, std::string> > pairs_):
		pairs(pairs_)
	{}
	std::string dump() const;
	void serialize(std::ostream &os) const;
	void deSerialize(std::istream &is);
};

/*
	Crafting definition base class
*/
class CraftDefinition
{
public:
	CraftDefinition(){}
	virtual ~CraftDefinition(){}

	void serialize(std::ostream &os) const;
	static CraftDefinition* deSerialize(std::istream &is);

	// Returns type of crafting definition
	virtual std::string getName() const=0;

	// Checks whether the recipe is applicable
	virtual bool check(const CraftInput &input, IGameDef *gamedef) const=0;
	// Returns the output structure, meaning depends on crafting method
	// The implementation can assume that check(input) returns true
	virtual CraftOutput getOutput(const CraftInput &input, IGameDef *gamedef) const=0;
	// the inverse of the above
	virtual CraftInput getInput(const CraftOutput &output, IGameDef *gamedef) const=0;
	// Decreases count of every input item
	virtual void decrementInput(CraftInput &input, IGameDef *gamedef) const=0;

	virtual CraftHashType getHashType() const = 0;
	virtual u64 getHash(CraftHashType type) const = 0;

	// to be called after all mods are loaded, so that we catch all aliases
	virtual void initHash(IGameDef *gamedef) = 0;

	virtual std::string dump() const=0;
protected:
	virtual void serializeBody(std::ostream &os) const=0;
	virtual void deSerializeBody(std::istream &is, int version)=0;
};

/*
	A plain-jane (shaped) crafting definition

	Supported crafting method: CRAFT_METHOD_NORMAL.
	Requires the input items to be arranged exactly like in the recipe.
*/
class CraftDefinitionShaped: public CraftDefinition
{
public:
	CraftDefinitionShaped():
		output(""), width(1), recipe(), hash_inited(false), replacements()
	{}
	CraftDefinitionShaped(
			const std::string &output_,
			unsigned int width_,
			const std::vector<std::string> &recipe_,
			const CraftReplacements &replacements_):
		output(output_), width(width_), recipe(recipe_),
		hash_inited(false), replacements(replacements_)
	{}
	virtual ~CraftDefinitionShaped(){}

	virtual std::string getName() const;
	virtual bool check(const CraftInput &input, IGameDef *gamedef) const;
	virtual CraftOutput getOutput(const CraftInput &input, IGameDef *gamedef) const;
	virtual CraftInput getInput(const CraftOutput &output, IGameDef *gamedef) const;
	virtual void decrementInput(CraftInput &input, IGameDef *gamedef) const;

	virtual CraftHashType getHashType() const;
	virtual u64 getHash(CraftHashType type) const;

	virtual void initHash(IGameDef *gamedef);

	virtual std::string dump() const;

protected:
	virtual void serializeBody(std::ostream &os) const;
	virtual void deSerializeBody(std::istream &is, int version);

private:
	// Output itemstring
	std::string output;
	// Width of recipe
	unsigned int width;
	// Recipe matrix (itemstrings)
	std::vector<std::string> recipe;
	// Recipe matrix (item names)
	std::vector<std::string> recipe_names;
	// bool indicating if initHash has been called already
	bool hash_inited;
	// Replacement items for decrementInput()
	CraftReplacements replacements;
};

/*
	A shapeless crafting definition
	Supported crafting method: CRAFT_METHOD_NORMAL.
	Input items can arranged in any way.
*/
class CraftDefinitionShapeless: public CraftDefinition
{
public:
	CraftDefinitionShapeless():
		output(""), recipe(), hash_inited(false), replacements()
	{}
	CraftDefinitionShapeless(
			const std::string &output_,
			const std::vector<std::string> &recipe_,
			const CraftReplacements &replacements_):
		output(output_), recipe(recipe_),
		hash_inited(false), replacements(replacements_)
	{}
	virtual ~CraftDefinitionShapeless(){}

	virtual std::string getName() const;
	virtual bool check(const CraftInput &input, IGameDef *gamedef) const;
	virtual CraftOutput getOutput(const CraftInput &input, IGameDef *gamedef) const;
	virtual CraftInput getInput(const CraftOutput &output, IGameDef *gamedef) const;
	virtual void decrementInput(CraftInput &input, IGameDef *gamedef) const;

	virtual CraftHashType getHashType() const;
	virtual u64 getHash(CraftHashType type) const;

	virtual void initHash(IGameDef *gamedef);

	virtual std::string dump() const;

protected:
	virtual void serializeBody(std::ostream &os) const;
	virtual void deSerializeBody(std::istream &is, int version);

private:
	// Output itemstring
	std::string output;
	// Recipe list (itemstrings)
	std::vector<std::string> recipe;
	// Recipe list (item names)
	std::vector<std::string> recipe_names;
	// bool indicating if initHash has been called already
	bool hash_inited;
	// Replacement items for decrementInput()
	CraftReplacements replacements;
};

/*
	Tool repair crafting definition
	Supported crafting method: CRAFT_METHOD_NORMAL.
	Put two damaged tools into the crafting grid, get one tool back.
	There should only be one crafting definition of this type.
*/
class CraftDefinitionToolRepair: public CraftDefinition
{
public:
	CraftDefinitionToolRepair():
		additional_wear(0)
	{}
	CraftDefinitionToolRepair(float additional_wear_):
		additional_wear(additional_wear_)
	{}
	virtual ~CraftDefinitionToolRepair(){}

	virtual std::string getName() const;
	virtual bool check(const CraftInput &input, IGameDef *gamedef) const;
	virtual CraftOutput getOutput(const CraftInput &input, IGameDef *gamedef) const;
	virtual CraftInput getInput(const CraftOutput &output, IGameDef *gamedef) const;
	virtual void decrementInput(CraftInput &input, IGameDef *gamedef) const;

	virtual CraftHashType getHashType() const { return CRAFT_HASH_TYPE_COUNT; }
	virtual u64 getHash(CraftHashType type) const { return 2; }

	virtual void initHash(IGameDef *gamedef) {}

	virtual std::string dump() const;

protected:
	virtual void serializeBody(std::ostream &os) const;
	virtual void deSerializeBody(std::istream &is, int version);

private:
	// This is a constant that is added to the wear of the result.
	// May be positive or negative, allowed range [-1,1].
	// 1 = new tool is completely broken
	// 0 = simply add remaining uses of both input tools
	// -1 = new tool is completely pristine
	float additional_wear;
};

/*
	A cooking (in furnace) definition
	Supported crafting method: CRAFT_METHOD_COOKING.
*/
class CraftDefinitionCooking: public CraftDefinition
{
public:
	CraftDefinitionCooking():
		output(""), recipe(""), hash_inited(false), cooktime()
	{}
	CraftDefinitionCooking(
			const std::string &output_,
			const std::string &recipe_,
			float cooktime_,
			const CraftReplacements &replacements_):
		output(output_), recipe(recipe_), hash_inited(false),
		cooktime(cooktime_), replacements(replacements_)
	{}
	virtual ~CraftDefinitionCooking(){}

	virtual std::string getName() const;
	virtual bool check(const CraftInput &input, IGameDef *gamedef) const;
	virtual CraftOutput getOutput(const CraftInput &input, IGameDef *gamedef) const;
	virtual CraftInput getInput(const CraftOutput &output, IGameDef *gamedef) const;
	virtual void decrementInput(CraftInput &input, IGameDef *gamedef) const;

	virtual CraftHashType getHashType() const;
	virtual u64 getHash(CraftHashType type) const;

	virtual void initHash(IGameDef *gamedef);

	virtual std::string dump() const;

protected:
	virtual void serializeBody(std::ostream &os) const;
	virtual void deSerializeBody(std::istream &is, int version);

private:
	// Output itemstring
	std::string output;
	// Recipe itemstring
	std::string recipe;
	// Recipe item name
	std::string recipe_name;
	// bool indicating if initHash has been called already
	bool hash_inited;
	// Time in seconds
	float cooktime;
	// Replacement items for decrementInput()
	CraftReplacements replacements;
};

/*
	A fuel (for furnace) definition
	Supported crafting method: CRAFT_METHOD_FUEL.
*/
class CraftDefinitionFuel: public CraftDefinition
{
public:
	CraftDefinitionFuel():
		recipe(""), hash_inited(false), burntime()
	{}
	CraftDefinitionFuel(std::string recipe_,
			float burntime_,
			const CraftReplacements &replacements_):
		recipe(recipe_), hash_inited(false), burntime(burntime_), replacements(replacements_)
	{}
	virtual ~CraftDefinitionFuel(){}

	virtual std::string getName() const;
	virtual bool check(const CraftInput &input, IGameDef *gamedef) const;
	virtual CraftOutput getOutput(const CraftInput &input, IGameDef *gamedef) const;
	virtual CraftInput getInput(const CraftOutput &output, IGameDef *gamedef) const;
	virtual void decrementInput(CraftInput &input, IGameDef *gamedef) const;

	virtual CraftHashType getHashType() const;
	virtual u64 getHash(CraftHashType type) const;

	virtual void initHash(IGameDef *gamedef);

	virtual std::string dump() const;

protected:
	virtual void serializeBody(std::ostream &os) const;
	virtual void deSerializeBody(std::istream &is, int version);

private:
	// Recipe itemstring
	std::string recipe;
	// Recipe item name
	std::string recipe_name;
	// bool indicating if initHash has been called already
	bool hash_inited;
	// Time in seconds
	float burntime;
	// Replacement items for decrementInput()
	CraftReplacements replacements;
};

/*
	Crafting definition manager
*/
class ICraftDefManager
{
public:
	ICraftDefManager(){}
	virtual ~ICraftDefManager(){}

	// The main crafting function
	virtual bool getCraftResult(CraftInput &input, CraftOutput &output,
			bool decrementInput, IGameDef *gamedef) const=0;
	virtual std::vector<CraftDefinition*> getCraftRecipes(CraftOutput &output,
			IGameDef *gamedef, unsigned limit=0) const=0;
	
	// Print crafting recipes for debugging
	virtual std::string dump() const=0;
};

class IWritableCraftDefManager : public ICraftDefManager
{
public:
	IWritableCraftDefManager(){}
	virtual ~IWritableCraftDefManager(){}

	// The main crafting function
	virtual bool getCraftResult(CraftInput &input, CraftOutput &output,
			bool decrementInput, IGameDef *gamedef) const=0;
	virtual std::vector<CraftDefinition*> getCraftRecipes(CraftOutput &output, 
			IGameDef *gamedef, unsigned limit=0) const=0;

	// Print crafting recipes for debugging
	virtual std::string dump() const=0;

	// Add a crafting definition.
	// After calling this, the pointer belongs to the manager.
	virtual void registerCraft(CraftDefinition *def, IGameDef *gamedef) = 0;

	// Delete all crafting definitions
	virtual void clear()=0;

	// To be called after all mods are loaded, so that we catch all aliases
	virtual void initHashes(IGameDef *gamedef) = 0;
};

IWritableCraftDefManager* createCraftDefManager();

#endif


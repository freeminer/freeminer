/*
settings.h
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
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

#ifndef SETTINGS_HEADER
#define SETTINGS_HEADER

#include "irrlichttypes_bloated.h"
#include "util/string.h"
#include "jthread/jmutex.h"
#include <string>
#include <map>
#include <list>
#include <set>

#include "porting.h"
#include "json/json.h" // for json config values
#include "msgpack.h"
#include <stdint.h>

class Settings;
struct NoiseParams;

/** function type to register a changed callback */
typedef void (*setting_changed_callback)(const std::string, void*);

enum ValueType {
	VALUETYPE_STRING,
	VALUETYPE_FLAG // Doesn't take any arguments
};

enum SettingsParseEvent {
	SPE_NONE,
	SPE_INVALID,
	SPE_COMMENT,
	SPE_KVPAIR,
	SPE_END,
	SPE_GROUP,
	SPE_MULTILINE,
};

struct ValueSpec {
	ValueSpec(ValueType a_type, const char *a_help=NULL)
	{
		type = a_type;
		help = a_help;
	}

	ValueType type;
	const char *help;
};

struct SettingsEntry {
	SettingsEntry()
	{
		group    = NULL;
		is_group = false;
	}

	SettingsEntry(const std::string &value_)
	{
		value    = value_;
		group    = NULL;
		is_group = false;
	}

	SettingsEntry(Settings *group_)
	{
		group    = group_;
		is_group = true;
	}

	std::string value;
	Settings *group;
	bool is_group;
};

class Settings {
public:
	Settings() {}
	~Settings();

	//Settings & operator += (const Settings &other);
	//Settings & operator = (const Settings &other);

	/***********************
	 * Reading and writing *
	 ***********************/

	// Read configuration file.  Returns success.
	bool readConfigFile(const std::string &filename);
	//Updates configuration file.  Returns success.
	bool updateConfigFile(const std::string &filename);
	// NOTE: Types of allowed_options are ignored.  Returns success.
	bool parseCommandLine(int argc, char *argv[],
			std::map<std::string, ValueSpec> &allowed_options);
	bool parseConfigLines(std::istream &is, const std::string &end = "");
	void writeLines(std::ostream &os, u32 tab_depth=0) const;

	SettingsParseEvent parseConfigObject(const std::string &line,
		const std::string &end, std::string &name, std::string &value);
	bool updateConfigObject(std::istream &is, std::ostream &os,
		const std::string &end, u32 tab_depth=0);

	static bool checkNameValid(const std::string &name);
	static bool checkValueValid(const std::string &value);
	static std::string sanitizeName(const std::string &name);
	static std::string sanitizeValue(const std::string &value);
	static std::string getMultiline(std::istream &is, size_t *num_lines=NULL);
	static void printEntry(std::ostream &os, const std::string &name,
		const SettingsEntry &entry, u32 tab_depth=0);

	/***********
	 * Getters *
	 ***********/

	const SettingsEntry &getEntry(const std::string &name) const;
	Settings *getGroup(const std::string &name) const;
	std::string get(const std::string &name) const;
	bool getBool(const std::string &name) const;
	u16 getU16(const std::string &name) const;
	s16 getS16(const std::string &name) const;
	s32 getS32(const std::string &name) const;
	u64 getU64(const std::string &name) const;
	float getFloat(const std::string &name) const;
	v2f getV2F(const std::string &name) const;
	v3f getV3F(const std::string &name) const;
	u32 getFlagStr(const std::string &name, const FlagDesc *flagdesc,
			u32 *flagmask) const;
	// N.B. if getStruct() is used to read a non-POD aggregate type,
	// the behavior is undefined.
	bool getStruct(const std::string &name, const std::string &format,
			void *out, size_t olen) const;
	bool getNoiseParams(const std::string &name, NoiseParams &np) const;
	bool getNoiseParamsFromValue(const std::string &name, NoiseParams &np) const;
	bool getNoiseParamsFromGroup(const std::string &name, NoiseParams &np) const;

	// return all keys used
	std::vector<std::string> getNames() const;
	bool exists(const std::string &name) const;


	/***************************************
	 * Getters that don't throw exceptions *
	 ***************************************/

	bool getEntryNoEx(const std::string &name, SettingsEntry &val) const;
	bool getGroupNoEx(const std::string &name, Settings *&val) const;
	bool getNoEx(const std::string &name, std::string &val) const;
	bool getFlag(const std::string &name) const;
	bool getU16NoEx(const std::string &name, u16 &val) const;
	bool getS16NoEx(const std::string &name, s16 &val) const;
	bool getS32NoEx(const std::string &name, s32 &val) const;
	bool getU64NoEx(const std::string &name, u64 &val) const;
	bool getFloatNoEx(const std::string &name, float &val) const;
	bool getV2FNoEx(const std::string &name, v2f &val) const;
	bool getV3FNoEx(const std::string &name, v3f &val) const;
	// N.B. getFlagStrNoEx() does not set val, but merely modifies it.  Thus,
	// val must be initialized before using getFlagStrNoEx().  The intention of
	// this is to simplify modifying a flags field from a default value.
	bool getFlagStrNoEx(const std::string &name, u32 &val, FlagDesc *flagdesc) const;


	/***********
	 * Setters *
	 ***********/

	// N.B. Groups not allocated with new must be set to NULL in the settings
	// tree before object destruction.
	bool setEntry(const std::string &name, const void *entry,
		bool set_group, bool set_default);
	bool set(const std::string &name, const std::string &value);
	bool setDefault(const std::string &name, const std::string &value);
	bool setGroup(const std::string &name, Settings *group);
	bool setGroupDefault(const std::string &name, Settings *group);
	bool setBool(const std::string &name, bool value);
	bool setS16(const std::string &name, s16 value);
	bool setU16(const std::string &name, u16 value);
	bool setS32(const std::string &name, s32 value);
	bool setU64(const std::string &name, uint64_t value);
	bool setFloat(const std::string &name, float value);
	bool setV2F(const std::string &name, v2f value);
	bool setV3F(const std::string &name, v3f value);
	bool setFlagStr(const std::string &name, u32 flags,
		const FlagDesc *flagdesc, u32 flagmask);
	bool setNoiseParams(const std::string &name, const NoiseParams &np,
		bool set_default=false);
	// N.B. if setStruct() is used to write a non-POD aggregate type,
	// the behavior is undefined.
	bool setStruct(const std::string &name, const std::string &format, void *value);
	// remove a setting
	bool remove(const std::string &name);
	void clear();
	void clearDefaults();
	void updateValue(const Settings &other, const std::string &name);
	void update(const Settings &other);

	Json::Value getJson(const std::string & name, const Json::Value & def = Json::Value());
	void setJson(const std::string & name, const Json::Value & value);

	void registerChangedCallback(std::string name, setting_changed_callback cbf, void *userdata = NULL);
	void deregisterChangedCallback(std::string name, setting_changed_callback cbf, void *userdata = NULL);

	Json::Value m_json;
	bool toJson(Json::Value &json) const;
	bool fromJson(const Json::Value &json);
	bool writeJsonFile(const std::string &filename);
	bool readJsonFile(const std::string &filename);
	void msgpack_pack(msgpack::packer<msgpack::sbuffer> &pk) const;
	void msgpack_unpack(msgpack::object o);

private:

	void updateNoLock(const Settings &other);
	void clearNoLock();
	void clearDefaultsNoLock();

	void doCallbacks(std::string name);

	std::map<std::string, SettingsEntry> m_settings;
	std::map<std::string, SettingsEntry> m_defaults;

	Json::Reader json_reader;
	Json::StyledWriter json_writer;

	std::map<std::string, std::vector<std::pair<setting_changed_callback,void*> > > m_callbacks;

	mutable JMutex m_callbackMutex;
	mutable JMutex m_mutex; // All methods that access m_settings/m_defaults directly should lock this.

};

extern Settings *g_settings;
extern std::string g_settings_path;

#endif

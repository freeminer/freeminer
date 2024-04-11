#pragma once

#include <unordered_map>
#include "key_value_storage.h"

class KeyValueCached
{
public:
	KeyValueStorage database;
	std::unordered_map<std::string, std::string> stats;

	KeyValueCached(const std::string &savedir, const std::string &name);
	~KeyValueCached();

	void save();
	void unload();
	void open();
	void close();

	const std::string &get(const std::string &key);
	const std::string &put(const std::string &key, const std::string &value);

private:
	std::mutex mutex;
};

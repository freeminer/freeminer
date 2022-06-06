#pragma once

#include <array>
#include <vector>
#include "mapnode.h"

class ServerEnvironment;
class ABMWithState;
class MapBlock;
class ServerMap;

struct ActiveABM
{
	ActiveABM()
	{}
	ABMWithState *abmws;
	int chance;
};

class ABMHandler
{
private:
	ServerEnvironment *m_env;
	std::array<std::vector<ActiveABM> *, CONTENT_ID_CAPACITY> m_aabms;
	std::list<std::vector<ActiveABM>*> m_aabms_list;
	bool m_aabms_empty;
public:
	ABMHandler(ServerEnvironment *env);
	void init(std::vector<ABMWithState> &abms);
	~ABMHandler();
	u32 countObjects(MapBlock *block, ServerMap * map, u32 &wider);
	void apply(MapBlock *block, bool activate = false);
};

#pragma once

#include <array>
#include <vector>
#include "mapnode.h"

class ServerEnvironment;
struct ABMWithState;
class MapBlock;
class ServerMap;

struct ActiveABM
{
	ABMWithState *abmws = nullptr;

	//ActiveBlockModifier *abm;
	int chance{};
	std::vector<content_t> required_neighbors;
	bool check_required_neighbors{}; // false if required_neighbors is known to be empty
	s16 min_y{};
	s16 max_y{};
};

class ABMHandler
{
private:
	ServerEnvironment *m_env{};

	//std::vector<std::vector<ActiveABM> *> m_aabms;

	std::array<std::vector<ActiveABM> *, CONTENT_ID_CAPACITY> m_aabms;
	std::list<std::vector<ActiveABM> *> m_aabms_list;
	bool m_aabms_empty{true};

public:
	ABMHandler(ServerEnvironment *env);
	void init(std::vector<ABMWithState> &abms);
	~ABMHandler();
	u32 countObjects(MapBlock *block, ServerMap *map, u32 &wider);
	void apply(MapBlock *block, uint8_t activate = 0);
};

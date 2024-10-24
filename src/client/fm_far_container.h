#pragma once

#include "fm_nodecontainer.h"
#include "mapblock.h"
#include "threading/concurrent_unordered_map.h"

class Mapgen;
class Client;
class FarContainer : public NodeContainer
{
	Client *m_client{};

public:
	Mapgen *m_mg{};
	bool use_weather {true};
	bool have_params {};
	FarContainer(Client *client);
	const MapNode &getNodeRefUnsafe(const v3pos_t &p) override;
};

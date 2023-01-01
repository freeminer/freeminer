#ifndef UNORDERED_MAP_HASH_HEADER
#define UNORDERED_MAP_HASH_HEADER

#include <unordered_map>
#include <unordered_set>
#include "../irr_v2d.h"
#include "../irr_v3d.h"

/*
struct v3s16Hash {
	std::size_t operator()(const v3s16& k) const {
		return (  (std::hash<int>()(k.X)
		        ^ (std::hash<int>()(k.Y) << 1)) >> 1)
		        ^ (std::hash<int>()(k.X) << 1);
	}
};

struct v3s16Equal {
	bool operator()(const v3s16& lhs, const v3s16& rhs) const {
		return lhs.X == rhs.X && lhs.Y == rhs.Y && lhs.Z == rhs.Z;
	}
};
*/

struct v2POSHash {
	std::size_t operator()(const v2pos_t& k) const {
		return ( (std::hash<int>()(k.X) ^ (std::hash<int>()(k.Y) << 1)) >> 1);
	}
};

struct v3POSHash {
	std::size_t operator()(const v3pos_t& k) const {
		return ( (std::hash<int>()(k.X) ^ (std::hash<int>()(k.Y) << 1)) >> 1) ^ (std::hash<int>()(k.Z) << 1);
	}
};


struct v2POSEqual {
	bool operator()(const v2pos_t& lhs, const v2pos_t& rhs) const {
		return lhs.X == rhs.X && lhs.Y == rhs.Y;
	}
};

struct v3POSEqual {
	bool operator()(const v3pos_t& lhs, const v3pos_t& rhs) const {
		return lhs.X == rhs.X && lhs.Y == rhs.Y && lhs.Z == rhs.Z;
	}
};


template <typename T>
using unordered_map_v2pos = std::unordered_map<v2pos_t, T, v2POSHash, v2POSEqual>;
using unordered_set_v2pos = std::unordered_set<v2pos_t, v2POSHash, v2POSEqual>;

template <typename T>
using unordered_map_v3pos = std::unordered_map<v3pos_t, T, v3POSHash, v3POSEqual>;
using unordered_set_v3pos = std::unordered_set<v3pos_t, v3POSHash, v3POSEqual>;


#endif

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
	std::size_t operator()(const v2POS& k) const {
		return ( (std::hash<int>()(k.X) ^ (std::hash<int>()(k.Y) << 1)) >> 1);
	}
};

struct v3POSHash {
	std::size_t operator()(const v3POS& k) const {
		return ( (std::hash<int>()(k.X) ^ (std::hash<int>()(k.Y) << 1)) >> 1) ^ (std::hash<int>()(k.Z) << 1);
	}
};


struct v2POSEqual {
	bool operator()(const v2POS& lhs, const v2POS& rhs) const {
		return lhs.X == rhs.X && lhs.Y == rhs.Y;
	}
};

struct v3POSEqual {
	bool operator()(const v3POS& lhs, const v3POS& rhs) const {
		return lhs.X == rhs.X && lhs.Y == rhs.Y && lhs.Z == rhs.Z;
	}
};


template <typename T>
using unordered_map_v2POS = std::unordered_map<v2POS, T, v2POSHash, v2POSEqual>;
using unordered_set_v2POS = std::unordered_set<v2POS, v2POSHash, v2POSEqual>;

template <typename T>
using unordered_map_v3POS = std::unordered_map<v3POS, T, v3POSHash, v3POSEqual>;
using unordered_set_v3POS = std::unordered_set<v3POS, v3POSHash, v3POSEqual>;


#endif

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

struct v2posHash {
	std::size_t operator()(const v2pos_t& k) const {
		return ( (std::hash<int>()(k.X) ^ (std::hash<int>()(k.Y) << 1)) >> 1);
	}
};

struct v3posHash {
	std::size_t operator()(const v3pos_t& k) const {
		return ( (std::hash<int>()(k.X) ^ (std::hash<int>()(k.Y) << 1)) >> 1) ^ (std::hash<int>()(k.Z) << 1);
	}
};


struct v2posEqual {
	bool operator()(const v2pos_t& lhs, const v2pos_t& rhs) const {
		return lhs.X == rhs.X && lhs.Y == rhs.Y;
	}
};

struct v3posEqual {
	bool operator()(const v3pos_t& lhs, const v3pos_t& rhs) const {
		return lhs.X == rhs.X && lhs.Y == rhs.Y && lhs.Z == rhs.Z;
	}
};

struct v2bposHash {
	std::size_t operator()(const v2bpos_t& k) const {
		return ( (std::hash<int>()(k.X) ^ (std::hash<int>()(k.Y) << 1)) >> 1);
	}
};

struct v3bposHash {
	std::size_t operator()(const v3bpos_t& k) const {
		return ( (std::hash<int>()(k.X) ^ (std::hash<int>()(k.Y) << 1)) >> 1) ^ (std::hash<int>()(k.Z) << 1);
	}
};


struct v2bposEqual {
	bool operator()(const v2bpos_t& lhs, const v2bpos_t& rhs) const {
		return lhs.X == rhs.X && lhs.Y == rhs.Y;
	}
};

struct v3bposEqual {
	bool operator()(const v3bpos_t& lhs, const v3bpos_t& rhs) const {
		return lhs.X == rhs.X && lhs.Y == rhs.Y && lhs.Z == rhs.Z;
	}
};


template <typename T>
using unordered_map_v2pos = std::unordered_map<v2pos_t, T, v2posHash, v2posEqual>;
using unordered_set_v2pos = std::unordered_set<v2pos_t, v2posHash, v2posEqual>;

template <typename T>
using unordered_map_v3pos = std::unordered_map<v3pos_t, T, v3posHash, v3posEqual>;
using unordered_set_v3pos = std::unordered_set<v3pos_t, v3posHash, v3posEqual>;

template <typename T>
using unordered_map_v3bpos = std::unordered_map<v3bpos_t, T, v3bposHash, v3bposEqual>;
using unordered_set_v3bpos = std::unordered_set<v3bpos_t, v3bposHash, v3bposEqual>;

#endif

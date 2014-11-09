#ifndef UNORDERED_MAP_HASH_HEADER
#define UNORDERED_MAP_HASH_HEADER

#include <unordered_map>
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

struct v3POSHash {
	std::size_t operator()(const v3POS& k) const {
		return (  (std::hash<int>()(k.X)
		        ^ (std::hash<int>()(k.Y) << 1)) >> 1)
		        ^ (std::hash<int>()(k.X) << 1);
	}
};

struct v3POSEqual {
	bool operator()(const v3POS& lhs, const v3POS& rhs) const {
		return lhs.X == rhs.X && lhs.Y == rhs.Y && lhs.Z == rhs.Z;
	}
};

#endif

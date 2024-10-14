/*
Copyright (C) 2024 proller <proler@gmail.com>
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

#pragma once

#include <unordered_set>

#include "lock.h"

template <class LOCKER, class Value, class Hash = std::hash<Value>,
		class Pred = std::equal_to<Value>, class Alloc = std::allocator<Value>>
class concurrent_unordered_set_ : public std::unordered_set<Value, Hash, Pred, Alloc>,
								  public LOCKER
{
public:
	using full_type = std::unordered_set<Value, Hash, Pred, Alloc>;

	~concurrent_unordered_set_() { clear(); }

	LOCK_UNIQUE_PROXY(full_type, operator=);
	LOCK_SHARED_PROXY(full_type, empty);
	LOCK_SHARED_PROXY(full_type, size);
	LOCK_SHARED_PROXY(full_type, max_size);

	LOCK_SHARED_PROXY(full_type, begin);
	LOCK_SHARED_PROXY(full_type, end);
	LOCK_SHARED_PROXY(full_type, cbegin);
	LOCK_SHARED_PROXY(full_type, cend);

	LOCK_UNIQUE_PROXY(full_type, emplace);
	LOCK_UNIQUE_PROXY(full_type, emplace_hint);
	LOCK_UNIQUE_PROXY(full_type, insert);
	LOCK_UNIQUE_PROXY(full_type, extract);
	LOCK_UNIQUE_PROXY(full_type, erase);
	LOCK_UNIQUE_PROXY(full_type, clear);
	LOCK_UNIQUE_PROXY(full_type, merge);
	LOCK_UNIQUE_PROXY(full_type, swap);
	//c++23 LOCK_UNIQUE_PROXY(full_type, insert_range);

	LOCK_SHARED_PROXY(full_type, find);
	LOCK_SHARED_PROXY(full_type, count);
	LOCK_SHARED_PROXY(full_type, contains);
	LOCK_SHARED_PROXY(full_type, equal_range);

	LOCK_SHARED_PROXY(full_type, at);
	LOCK_UNIQUE_PROXY(full_type, assign);

	LOCK_UNIQUE_PROXY(full_type, rehash);
	LOCK_UNIQUE_PROXY(full_type, reserve);
	LOCK_SHARED_PROXY(full_type, load_factor);
	LOCK_SHARED_PROXY(full_type, max_load_factor);
};

template <class Value, class Hash = std::hash<Value>, class Pred = std::equal_to<Value>,
		class Alloc = std::allocator<Value>>
using concurrent_unordered_set =
		concurrent_unordered_set_<locker<>, Value, Hash, Pred, Alloc>;

template <class Value, class Hash = std::hash<Value>, class Pred = std::equal_to<Value>,
		class Alloc = std::allocator<Value>>
using concurrent_shared_unordered_set =
		concurrent_unordered_set_<shared_locker, Value, Hash, Pred, Alloc>;

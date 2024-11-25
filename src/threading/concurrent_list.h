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

#include <list>

#include "lock.h"

template <class LOCKER, class T, class Allocator = std::allocator<T>>
class concurrent_list_ : public std::list<T, Allocator>, public LOCKER
{
public:
	typedef typename std::list<T, Allocator> full_type;
	typedef T mapped_type;

	~concurrent_list_() { clear(); }

	template <typename... Args>
	decltype(auto) assign(Args &&...args)
	{
		const auto lock = LOCKER::lock_unique_rec();
		return full_type::assign(std::forward<Args>(args)...);
	}

	template <typename... Args>
	decltype(auto) insert(Args &&...args)
	{
		const auto lock = LOCKER::lock_unique_rec();
		return full_type::insert(std::forward<Args>(args)...);
	}

	template <typename... Args>
	decltype(auto) emplace(Args &&...args)
	{
		const auto lock = LOCKER::lock_unique_rec();
		return full_type::emplace(std::forward<Args>(args)...);
	}

	template <typename... Args>
	decltype(auto) emplace_try(Args &&...args)
	{
		const auto lock = LOCKER::try_lock_unique_rec();
		if (!lock->owns_lock())
			return false;
		return full_type::emplace(std::forward<Args>(args)...).second;
	}

	template <typename... Args>
	decltype(auto) empty(Args &&...args)
	{
		const auto lock = LOCKER::lock_shared_rec();
		return full_type::empty(std::forward<Args>(args)...);
	}

	template <typename... Args>
	decltype(auto) size(Args &&...args)
	{
		const auto lock = LOCKER::lock_shared_rec();
		return full_type::size(std::forward<Args>(args)...);
	}

	template <typename... Args>
	decltype(auto) begin(Args &&...args)
	{
		const auto lock = LOCKER::lock_shared_rec();
		return full_type::begin(std::forward<Args>(args)...);
	}

	template <typename... Args>
	decltype(auto) rbegin(Args &&...args)
	{
		const auto lock = LOCKER::lock_shared_rec();
		return full_type::rbegin(std::forward<Args>(args)...);
	}

	template <typename... Args>
	decltype(auto) end(Args &&...args)
	{
		const auto lock = LOCKER::lock_shared_rec();
		return full_type::end(std::forward<Args>(args)...);
	}

	template <typename... Args>
	decltype(auto) rend(Args &&...args)
	{
		const auto lock = LOCKER::lock_shared_rec();
		return full_type::rend(std::forward<Args>(args)...);
	}

	template <typename... Args>
	decltype(auto) erase(Args &&...args)
	{
		const auto lock = LOCKER::lock_unique_rec();
		return full_type::erase(std::forward<Args>(args)...);
	}

	template <typename... Args>
	decltype(auto) clear(Args &&...args)
	{
		const auto lock = LOCKER::lock_unique_rec();
		return full_type::clear(std::forward<Args>(args)...);
	}
};

template <class T, class Allocator = std::allocator<T>>
using concurrent_list = concurrent_list_<locker<>, T, Allocator>;

#if ENABLE_THREADS

template <class T, class Allocator = std::allocator<T>>
using maybe_concurrent_list = concurrent_list<T, Allocator>;

#else

template <class T, class Allocator = std::allocator<T>>
class not_concurrent_list : public std::list<T, Allocator>, public dummy_locker
{
public:
	typedef typename std::list<T, Allocator> full_type;
	typedef T mapped_type;
};

template <class T, class Allocator = std::allocator<T>>
using maybe_concurrent_list = not_concurrent_list<T, Allocator>;

#endif

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

#include <unordered_map>

#include "lock.h"

template <class LOCKER, class Key, class T, class Hash = std::hash<Key>,
		class Pred = std::equal_to<Key>,
		class Alloc = std::allocator<std::pair<const Key, T>>>
class concurrent_unordered_map_ : public std::unordered_map<Key, T, Hash, Pred, Alloc>,
								  public LOCKER
{
public:
	using full_type = std::unordered_map<Key, T, Hash, Pred, Alloc>;
	typedef Key key_type;
	typedef T mapped_type;
	typedef Hash hasher;
	typedef Pred key_equal;
	typedef Alloc allocator_type;
	typedef std::pair<const key_type, mapped_type> value_type;
	typedef value_type &reference;
	typedef const value_type &const_reference;
	typedef typename full_type::pointer pointer;
	typedef typename full_type::const_pointer const_pointer;
	typedef typename full_type::size_type size_type;
	typedef typename full_type::difference_type difference_type;

	typedef typename full_type::const_iterator const_iterator;
	typedef typename full_type::iterator iterator;

	~concurrent_unordered_map_() { clear(); }

	template <typename... Args>
	decltype(auto) operator=(Args &&...args)
	{
		const auto lock = LOCKER::lock_unique_rec();
		return full_type::operator=(std::forward<Args>(args)...);
	}

	const mapped_type nothing{};

	template <typename... Args>
	const mapped_type &get(Args &&...args) const
	{
		const auto lock = LOCKER::lock_shared_rec();
		if (const auto &it = full_type::find(std::forward<Args>(args)...);
				it != full_type::end()) {
			return it->second;
		}
		return nothing;
	}

	template <typename... Args>
	const mapped_type &at_or(Args &&...args, const mapped_type &def) const
	{
		const auto lock = LOCKER::lock_shared_rec();
		if (const auto &it = full_type::find(std::forward<Args>(args)...);
				it != full_type::end()) {
			return it->second;
		}
		return def;
	}

	template <typename... Args>
	decltype(auto) assign(Args &&...args)
	{
		const auto lock = LOCKER::lock_unique_rec();
		return full_type::assign(std::forward<Args>(args)...);
	}

	template <typename... Args>
	decltype(auto) emplace(Args &&...args)
	{
		const auto lock = LOCKER::lock_unique_rec();
		return full_type::emplace(std::forward<Args>(args)...);
	}

	template <typename... Args>
	decltype(auto) insert_or_assign(Args &&...args)
	{
		const auto lock = LOCKER::lock_unique_rec();
		return full_type::insert_or_assign(std::forward<Args>(args)...);
	}

	bool empty() const
	{
		const auto lock = LOCKER::lock_shared_rec();
		return full_type::empty();
	}

	size_type size() const
	{
		const auto lock = LOCKER::lock_shared_rec();
		return full_type::size();
	}

	size_type count(const key_type &k)
	{
		const auto lock = LOCKER::lock_shared_rec();
		return full_type::count(k);
	}

	template <typename... Args>
	decltype(auto) contains(Args &&...args) const
	{
		const auto lock = LOCKER::lock_shared_rec();
		return full_type::contains(std::forward<Args>(args)...);
	}

	iterator find(const key_type &k)
	{
		const auto lock = LOCKER::lock_shared_rec();
		return full_type::find(k);
	};

	const_iterator find(const key_type &k) const
	{
		const auto lock = LOCKER::lock_shared_rec();
		return full_type::find(k);
	};

	iterator begin()
	{
		const auto lock = LOCKER::lock_shared_rec();
		return full_type::begin();
	};

	const_iterator begin() const
	{
		const auto lock = LOCKER::lock_shared_rec();
		return full_type::begin();
	};

	iterator end()
	{
		const auto lock = LOCKER::lock_shared_rec();
		return full_type::end();
	};

	const_iterator end() const
	{
		const auto lock = LOCKER::lock_shared_rec();
		return full_type::end();
	};

	template <typename... Args>
	decltype(auto) at(Args &&...args)
	{
		const auto lock = LOCKER::lock_shared_rec();
		return full_type::at(std::forward<Args>(args)...);
	}

	mapped_type &operator[](const key_type &k) = delete;

	mapped_type &operator[](key_type &&k) = delete;

	typename full_type::iterator erase(const_iterator position)
	{
		const auto lock = LOCKER::lock_unique_rec();
		return full_type::erase(position);
	}

	typename full_type::iterator erase(iterator position)
	{
		const auto lock = LOCKER::lock_unique_rec();
		return full_type::erase(position);
	}

	size_type erase(const key_type &k)
	{
		const auto lock = LOCKER::lock_unique_rec();
		return full_type::erase(k);
	}

	typename full_type::iterator erase(const_iterator first, const_iterator last)
	{
		const auto lock = LOCKER::lock_unique_rec();
		return full_type::erase(first, last);
	}

	void clear()
	{
		const auto lock = LOCKER::lock_unique_rec();
		full_type::clear();
	}
};

template <class Key, class T, class Hash = std::hash<Key>,
		class Pred = std::equal_to<Key>,
		class Alloc = std::allocator<std::pair<const Key, T>>>
using concurrent_unordered_map =
		concurrent_unordered_map_<locker<>, Key, T, Hash, Pred, Alloc>;

template <class Key, class T, class Hash = std::hash<Key>,
		class Pred = std::equal_to<Key>,
		class Alloc = std::allocator<std::pair<const Key, T>>>
using concurrent_shared_unordered_map =
		concurrent_unordered_map_<shared_locker, Key, T, Hash, Pred, Alloc>;

#if ENABLE_THREADS

template <class Key, class T, class Hash = std::hash<Key>,
		class Pred = std::equal_to<Key>,
		class Alloc = std::allocator<std::pair<const Key, T>>>
using maybe_concurrent_unordered_map =
		concurrent_unordered_map<Key, T, Hash, Pred, Alloc>;

#else

template <class Key, class T, class Hash = std::hash<Key>,
		class Pred = std::equal_to<Key>,
		class Alloc = std::allocator<std::pair<const Key, T>>>
class not_concurrent_unordered_map : public std::unordered_map<Key, T, Hash, Pred, Alloc>,
									 public dummy_locker
{
public:
	typedef typename std::unordered_map<Key, T, Hash, Pred, Alloc> full_type;
	typedef Key key_type;
	typedef T mapped_type;

	mapped_type &get(const key_type &k) { return full_type::operator[](k); }

	void set(const key_type &k, const mapped_type &v) { full_type::operator[](k) = v; }
};

template <class Key, class T, class Hash = std::hash<Key>,
		class Pred = std::equal_to<Key>,
		class Alloc = std::allocator<std::pair<const Key, T>>>
using maybe_concurrent_unordered_map =
		not_concurrent_unordered_map<Key, T, Hash, Pred, Alloc>;

#endif

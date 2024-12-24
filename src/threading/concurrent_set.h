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

#include <set>

#include "lock.h"

template <class LOCKER, class Key, class Compare = std::less<Key>,
		class Allocator = std::allocator<Key>>
class concurrent_set_ : public std::set<Key, Compare, Allocator>, public LOCKER
{
public:
	typedef typename std::set<Key, Compare, Allocator> full_type;
	typedef bool mapped_type;

	typedef Key key_type;
	typedef key_type value_type;
	typedef Compare key_compare;
	typedef key_compare value_compare;
	typedef Allocator allocator_type;
	//typedef typename allocator_type::reference reference;
	//typedef typename allocator_type::const_reference const_reference;
	typedef typename allocator_type::size_type size_type;
	typedef typename allocator_type::difference_type difference_type;
	//typedef typename allocator_type::pointer pointer;
	//typedef typename allocator_type::const_pointer const_pointer;

	typedef typename full_type::iterator iterator;
	typedef typename full_type::const_iterator const_iterator;
	typedef typename full_type::reverse_iterator reverse_iterator;
	typedef typename full_type::const_reverse_iterator const_reverse_iterator;
	//typedef typename full_type::node_type node_type;				   // C++17
	//typedef typename full_type::insert_return_type insert_return_type; // C++17

	typedef typename std::pair<iterator, bool> insert_return_type_old;

	mapped_type nothing = {};

	~concurrent_set_() { clear(); }

	template <typename... Args>
	mapped_type &get(Args &&...args)
	{
		const auto lock = LOCKER::lock_shared_rec();

		//if (!full_type::contains(std::forward<Args>(args)...))
		if (full_type::find(std::forward<Args>(args)...) == full_type::end())
			return nothing;

		return full_type::operator[](std::forward<Args>(args)...);
	}

	template <typename... Args>
	decltype(auto) assign(Args &&...args)
	{
		const auto lock = LOCKER::lock_unique_rec();
		return full_type::assign(std::forward<Args>(args)...);
	}

	insert_return_type_old insert(const key_type &k)
	{
		const auto lock = LOCKER::lock_unique_rec();
		return full_type::insert(k);
	}
	insert_return_type_old insert_try(const key_type &k)
	{
		const auto lock = LOCKER::try_lock_unique_rec();
		if (!lock->owns_lock())
			return {};
		return full_type::insert(k);
	}

	bool set_try(const key_type &k, const mapped_type &v)
	{
		const auto lock = LOCKER::try_lock_unique_rec();
		if (!lock->owns_lock())
			return false;
		full_type::operator[](k) = v;
		return true;
	}

	bool empty()
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
	decltype(auto) contains(Args &&...args)
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

	reverse_iterator rbegin()
	{
		const auto lock = LOCKER::lock_shared_rec();
		return full_type::rbegin();
	};

	const_reverse_iterator rbegin() const
	{
		const auto lock = LOCKER::lock_shared_rec();
		return full_type::rbegin();
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

	reverse_iterator rend()
	{
		const auto lock = LOCKER::lock_shared_rec();
		return full_type::rend();
	};

	const_reverse_iterator rend() const
	{
		const auto lock = LOCKER::lock_shared_rec();
		return full_type::rend();
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

	/*
	typename full_type::iterator erase(iterator position)
	{
		const auto lock = LOCKER::lock_unique_rec();
		return full_type::erase(position);
	}
*/

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

	template <typename... Args>
	decltype(auto) operator=(Args &&...args)
	{
		const auto lock = LOCKER::lock_unique_rec();
		return full_type::operator=(std::forward<Args>(args)...);
	}
};

template <class Key, class Compare = std::less<Key>,
		class Allocator = std::allocator<Key>>
using concurrent_set = concurrent_set_<locker<>, Key, Compare, Allocator>;

template <class Key, class Compare = std::less<Key>,
		class Allocator = std::allocator<Key>>
using concurrent_shared_set = concurrent_set_<shared_locker, Key, Compare, Allocator>;

#if ENABLE_THREADS

template <class Key, class Compare = std::less<Key>,
		class Allocator = std::allocator<Key>>
using maybe_concurrent_set = concurrent_set<Key, Compare, Allocator>;

#else

template <class Key, class Compare = std::less<Key>,
		class Allocator = std::allocator<Key>>
class not_concurrent_set : public std::set<Key, Compare, Allocator>, public dummy_locker
{
public:
	typedef typename std::set<Key, Compare, Allocator> full_type;
	typedef Key key_type;
	typedef bool mapped_type;

	mapped_type &get(const key_type &k) { return full_type::operator[](k); }

	void set(const key_type &k, const mapped_type &v) { full_type::operator[](k) = v; }

	bool set_try(const key_type &k, const mapped_type &v)
	{
		full_type::operator[](k) = v;
		return true;
	}
};

template <class Key, class Compare = std::less<Key>,
		class Allocator = std::allocator<Key>>
using maybe_concurrent_set = not_concurrent_set<Key, Compare, Allocator>;

#endif

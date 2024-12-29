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

#include "lock.h"

#include <vector>

template <class LOCKER, class T, class Allocator = std::allocator<T>>
class concurrent_vector_ : public std::vector<T, Allocator>, public LOCKER
{
public:
	typedef typename std::vector<T, Allocator> full_type;
	typedef T value_type;
	typedef Allocator allocator_type;
	typedef typename full_type::reference reference;
	typedef typename full_type::const_reference const_reference;
	typedef typename full_type::size_type size_type;
	typedef typename full_type::pointer pointer;
	typedef typename full_type::const_pointer const_pointer;

	typedef typename full_type::const_iterator const_iterator;
	typedef typename full_type::iterator iterator;

	~concurrent_vector_() { clear(); }

	template <typename... Args>
	decltype(auto) operator=(Args &&...args)
	{
		const auto lock = LOCKER::lock_unique_rec();
		// TODO: other.shared_lock
		return full_type::operator=(std::forward<Args>(args)...);
	}

	template <typename... Args>
	decltype(auto) assign(Args &&...args)
	{
		const auto lock = LOCKER::lock_unique_rec();
		return full_type::assign(std::forward<Args>(args)...);
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

	template <typename... Args>
	decltype(auto) at(Args &&...args)
	{
		const auto lock = LOCKER::lock_shared_rec();
		return full_type::at(std::forward<Args>(args)...);
	}

	reference operator[](size_type n)
	{
		const auto lock = LOCKER::lock_unique_rec();
		return full_type::operator[](n);
	};

	const_reference operator[](size_type n) const
	{
		const auto lock = LOCKER::lock_shared_rec();
		return full_type::operator[](n);
	};

	void resize(size_type sz)
	{
		const auto lock = LOCKER::lock_unique_rec();
		return full_type::resize(sz);
	};

	void clear()
	{
		const auto lock = LOCKER::lock_unique_rec();
		return full_type::clear();
	};

	template <typename... Args>
	decltype(auto) push_back(Args &&...args)
	{
		const auto lock = LOCKER::lock_unique_rec();
		return full_type::push_back(std::forward<Args>(args)...);
	}

	template <typename... Args>
	decltype(auto) emplace_back(Args &&...args)
	{
		const auto lock = LOCKER::lock_unique_rec();
		return full_type::emplace_back(std::forward<Args>(args)...);
	}
};

template <class T, class Allocator = std::allocator<T>>
using concurrent_vector = concurrent_vector_<locker<>, T, Allocator>;

template <class T, class Allocator = std::allocator<T>>
using concurrent_shared_vector = concurrent_vector_<shared_locker, T, Allocator>;

#if ENABLE_THREADS

template <class T, class Allocator = std::allocator<T>>
using maybe_concurrent_vector = concurrent_vector<T, Allocator>;

#else

template <class T, class Allocator = std::allocator<T>>
class not_concurrent_vector : public std::vector<T, Allocator>, public dummy_locker
{
public:
	typedef typename std::vector<T, Allocator> full_type;
	typedef T key_type;
	typedef T mapped_type;
	typedef T value_type;
	typedef typename full_type::size_type size_type;

	mapped_type &get(size_type n) { return full_type::operator[](n); }

	void set(size_type n, const mapped_type &v) { full_type::operator[](n) = v; }
};

template <class T, class Allocator = std::allocator<T>>
using maybe_concurrent_vector = not_concurrent_vector<T, Allocator>;

#endif

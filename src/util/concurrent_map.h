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

#ifndef UTIL_CONCURENT_MAP_HEADER
#define UTIL_CONCURENT_MAP_HEADER

#include <map>

#include "lock.h"


template < class LOCKER, class Key, class T, class Compare = std::less<Key>,
           class Allocator = std::allocator<std::pair<const Key, T> > >
class concurrent_map_: public std::map<Key, T, Compare, Allocator>,
	public LOCKER {
public:
	typedef typename std::map<Key, T, Compare, Allocator> full_type;
	typedef Key                                           key_type;
	typedef T                                             mapped_type;
	typedef Allocator                                     allocator_type;
	typedef typename allocator_type::size_type            size_type;
	typedef typename full_type::const_iterator            const_iterator;
	typedef typename full_type::iterator                  iterator;
	typedef typename full_type::reverse_iterator          reverse_iterator;
	typedef typename full_type::const_reverse_iterator    const_reverse_iterator;

	mapped_type& get(const key_type& k) {
		auto lock = LOCKER::lock_shared_rec();
		return full_type::operator[](k);
	}

	void set(const key_type& k, const mapped_type& v) {
		auto lock = LOCKER::lock_unique_rec();
		full_type::operator[](k) = v;
	}

	bool set_try(const key_type& k, const mapped_type& v) {
		auto lock = LOCKER::try_lock_unique_rec();
		if (!lock->owns_lock())
			return false;
		full_type::operator[](k) = v;
		return true;
	}

	bool      empty() {
		auto lock = LOCKER::lock_shared_rec();
		return full_type::empty();
	}

	size_type size() {
		auto lock = LOCKER::lock_shared_rec();
		return full_type::size();
	}

	size_type count(const key_type& k) {
		auto lock = LOCKER::lock_shared_rec();
		return full_type::count(k);
	}

	iterator find(const key_type& k) {
		auto lock = LOCKER::lock_shared_rec();
		return full_type::find(k);
	};

	const_iterator find(const key_type& k) const {
		auto lock = LOCKER::lock_shared_rec();
		return full_type::find(k);
	};

	iterator begin() noexcept {
		auto lock = LOCKER::lock_shared_rec();
		return full_type::begin();
	};

	const_iterator begin()   const noexcept {
		auto lock = LOCKER::lock_shared_rec();
		return full_type::begin();
	};

	reverse_iterator rbegin() noexcept {
		auto lock = LOCKER::lock_shared_rec();
		return full_type::rbegin();
	};

	const_reverse_iterator rbegin()   const noexcept {
		auto lock = LOCKER::lock_shared_rec();
		return full_type::rbegin();
	};

	iterator end() noexcept {
		auto lock = LOCKER::lock_shared_rec();
		return full_type::end();
	};

	const_iterator end()   const noexcept {
		auto lock = LOCKER::lock_shared_rec();
		return full_type::end();
	};

	reverse_iterator rend() noexcept {
		auto lock = LOCKER::lock_shared_rec();
		return full_type::rend();
	};

	const_reverse_iterator rend()   const noexcept {
		auto lock = LOCKER::lock_shared_rec();
		return full_type::rend();
	};

	mapped_type& operator[](const key_type& k) = delete;

	mapped_type& operator[](key_type&& k) = delete;

	typename full_type::iterator  erase(const_iterator position) {
		auto lock = LOCKER::lock_unique_rec();
		return full_type::erase(position);
	}

	typename full_type::iterator  erase(iterator position) {
		auto lock = LOCKER::lock_unique_rec();
		return full_type::erase(position);
	}

	size_type erase(const key_type& k) {
		auto lock = LOCKER::lock_unique_rec();
		return full_type::erase(k);
	}

	typename full_type::iterator  erase(const_iterator first, const_iterator last) {
		auto lock = LOCKER::lock_unique_rec();
		return full_type::erase(first, last);
	}

	void clear() {
		auto lock = LOCKER::lock_unique_rec();
		full_type::clear();
	}
};

template <class Key, class T, class Compare = std::less<Key>,
          class Allocator = std::allocator<std::pair<const Key, T> >>
class concurrent_map: public concurrent_map_<locker<>, Key, T, Compare, Allocator>
{ };


#if ENABLE_THREADS

template < class Key, class T, class Compare = std::less<Key>,
           class Allocator = std::allocator<std::pair<const Key, T> >>
class maybe_concurrent_map: public concurrent_map<Key, T, Compare, Allocator>
{ };

#else

template < class Key, class T, class Compare = std::less<Key>,
           class Allocator = std::allocator<std::pair<const Key, T> >>
class not_concurrent_map: public std::map<Key, T, Compare, Allocator>,
	public dummy_locker {
public:
	typedef typename std::map<Key, T, Compare, Allocator> full_type;
	typedef Key                                           key_type;
	typedef T                                             mapped_type;

	mapped_type& get(const key_type& k) {
		return full_type::operator[](k);
	}

	void set(const key_type& k, const mapped_type& v) {
		full_type::operator[](k) = v;
	}

	bool set_try(const key_type& k, const mapped_type& v) {
		full_type::operator[](k) = v;
		return true;
	}
};

template < class Key, class T, class Compare = std::less<Key>,
           class Allocator = std::allocator<std::pair<const Key, T> >>
class maybe_concurrent_map: public not_concurrent_map<Key, T, Compare, Allocator>
{ };

#endif

#endif

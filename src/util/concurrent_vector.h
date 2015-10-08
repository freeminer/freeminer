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

#ifndef UTIL_CONCURENT_VECTOR_HEADER
#define UTIL_CONCURENT_VECTOR_HEADER

#include "util/lock.h"

#include <vector>

template <class T, class Allocator = std::allocator<T> >
class concurrent_vector :
	public std::vector<T, Allocator>,
	public locker<> {
public:
	typedef typename std::vector<T, Allocator>           full_type;
	typedef T                                        value_type;
	typedef Allocator                                allocator_type;
	typedef typename full_type::reference       reference;
	typedef typename full_type::const_reference const_reference;
	typedef typename full_type::size_type       size_type;
	typedef typename full_type::pointer         pointer;
	typedef typename full_type::const_pointer   const_pointer;

	typedef typename full_type::const_iterator                         const_iterator;
	typedef typename full_type::iterator                               iterator;


	bool      empty() {
		auto lock = lock_shared_rec();
		return full_type::empty();
	}

	size_type size() {
		auto lock = lock_shared_rec();
		return full_type::size();
	}

	reference       operator[](size_type n) {
		auto lock = lock_unique_rec();
		return full_type::operator[](n);
	};

	const_reference operator[](size_type n) const {
		auto lock = lock_shared_rec();
		return full_type::operator[](n);
	};

	void resize(size_type sz) {
		auto lock = lock_unique_rec();
		return full_type::resize(sz);
	};

	void clear() {
		auto lock = lock_unique_rec();
		return full_type::clear();
	};

	void push_back(const value_type& x) {
		auto lock = lock_unique_rec();
		return full_type::push_back(x);
	};

	void push_back(value_type&& x) {
		auto lock = lock_unique_rec();
		return full_type::push_back(x);
	};

};


#if ENABLE_THREADS

template <class T, class Allocator = std::allocator<T> >
class maybe_concurrent_vector : public concurrent_vector<T, Allocator>
{};

#else

template <class T, class Allocator = std::allocator<T> >
class not_concurrent_vector :
	public std::vector<T, Allocator>,
	public dummy_locker {
public:
	typedef typename std::vector<T, Allocator>       full_type;
	typedef T                                        key_type;
	typedef T                                        mapped_type;
	typedef T                                        value_type;
	typedef typename full_type::size_type       size_type;

	mapped_type& get(size_type n) {
		return full_type::operator[](n);
	}

	void set(size_type n, const mapped_type& v) {
		full_type::operator[](n) = v;
	}
};

template <class T, class Allocator = std::allocator<T> >
class maybe_concurrent_vector: public not_concurrent_vector<T, Allocator>
{};

#endif


#endif

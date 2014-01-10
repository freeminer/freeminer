/*
Copyright (C) 2013 xyz, Ilya Zhuravlev <whatever@xyz.is>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef FMBITSET_HEADER
#define FMBITSET_HEADER

#include <cstdlib>
#include <vector>

class FMBitset {
	public:
		FMBitset(size_t capacity);
		size_t count();
		void set(size_t index, bool value);
		bool get(size_t index);

	private:
		std::vector<char> m_bits;
		size_t m_count;
};


#endif

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

#include <fmbitset.h>

FMBitset::FMBitset(size_t capacity):
	m_bits(capacity, 0),
	m_count(0)
{}

size_t FMBitset::count() {
	return m_count;
}

void FMBitset::set(size_t index, bool value) {
	if (m_bits[index] != value) {
		m_bits[index] = value;
		m_count += value ? 1 : -1;
	}
}

bool FMBitset::get(size_t index) {
	return m_bits[index];
}

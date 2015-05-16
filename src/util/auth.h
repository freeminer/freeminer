/*
Minetest
Copyright (C) 2015 est31 <MTest31@outlook.com>

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

#ifndef AUTH_H
#define AUTH_H

std::string translatePassword(const std::string &name,
	const std::string &password);
void getSRPVerifier(const std::string &name,
	const std::string &password, char **salt, size_t *salt_len,
	char **bytes_v, size_t *len_v);
std::string getSRPVerifier(const std::string &name,
	const std::string &password);
std::string getSRPVerifier(const std::string &name,
	const std::string &password, const std::string &salt);
std::string encodeSRPVerifier(const std::string &verifier,
	const std::string &salt);
bool decodeSRPVerifier(const std::string &enc_pwd,
	std::string *salt, std::string *bytes_v);

#endif
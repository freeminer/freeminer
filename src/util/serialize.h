/*
util/serialize.h
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
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

#ifndef UTIL_SERIALIZE_HEADER
#define UTIL_SERIALIZE_HEADER

#include "../irrlichttypes_bloated.h"
#include "config.h"
#if HAVE_ENDIAN_H
#include <endian.h>
#include <string.h> // for memcpy
#endif
#include <iostream>
#include <string>

#include "../msgpack_fix.h"
#include "msgpack_define_external.h"

#define FIXEDPOINT_FACTOR 1000.0f
#define FIXEDPOINT_INVFACTOR (1.0f/FIXEDPOINT_FACTOR)

#if HAVE_ENDIAN_H
// use machine native byte swapping routines
// Note: memcpy below is optimized out by modern compilers

inline void writeU64(u8* data, u64 i)
{
	u64 val = htobe64(i);
	memcpy(data, &val, 8);
}

inline void writeU32(u8* data, u32 i)
{
	u32 val = htobe32(i);
	memcpy(data, &val, 4);
}

inline void writeU16(u8* data, u16 i)
{
	u16 val = htobe16(i);
	memcpy(data, &val, 2);
}

inline u64 readU64(const u8* data)
{
	u64 val;
	memcpy(&val, data, 8);
	return be64toh(val);
}

inline u32 readU32(const u8* data)
{
	u32 val;
	memcpy(&val, data, 4);
	return be32toh(val);
}

inline u16 readU16(const u8* data)
{
	u16 val;
	memcpy(&val, data, 2);
	return be16toh(val);
}

#else
// generic byte-swapping implementation

inline void writeU64(u8 *data, u64 i)
{
	data[0] = ((i>>56)&0xff);
	data[1] = ((i>>48)&0xff);
	data[2] = ((i>>40)&0xff);
	data[3] = ((i>>32)&0xff);
	data[4] = ((i>>24)&0xff);
	data[5] = ((i>>16)&0xff);
	data[6] = ((i>> 8)&0xff);
	data[7] = ((i>> 0)&0xff);
}

inline void writeU32(u8 *data, u32 i)
{
	data[0] = ((i>>24)&0xff);
	data[1] = ((i>>16)&0xff);
	data[2] = ((i>> 8)&0xff);
	data[3] = ((i>> 0)&0xff);
}

inline void writeU16(u8 *data, u16 i)
{
	data[0] = ((i>> 8)&0xff);
	data[1] = ((i>> 0)&0xff);
}

inline u64 readU64(const u8 *data)
{
	return ((u64)data[0]<<56) | ((u64)data[1]<<48)
		| ((u64)data[2]<<40) | ((u64)data[3]<<32)
		| ((u64)data[4]<<24) | ((u64)data[5]<<16)
		| ((u64)data[6]<<8) | ((u64)data[7]<<0);
}

inline u32 readU32(const u8 *data)
{
	return (data[0]<<24) | (data[1]<<16) | (data[2]<<8) | (data[3]<<0);
}

inline u16 readU16(const u8 *data)
{
	return (data[0]<<8) | (data[1]<<0);
}

#endif

inline void writeU8(u8 *data, u8 i)
{
	data[0] = ((i>> 0)&0xff);
}

inline u8 readU8(const u8 *data)
{
	return (data[0]<<0);
}

inline void writeS32(u8 *data, s32 i){
	writeU32(data, (u32)i);
}
inline s32 readS32(const u8 *data){
	return (s32)readU32(data);
}

inline void writeS16(u8 *data, s16 i){
	writeU16(data, (u16)i);
}
inline s16 readS16(const u8 *data){
	return (s16)readU16(data);
}

inline void writeS8(u8 *data, s8 i){
	writeU8(data, (u8)i);
}
inline s8 readS8(const u8 *data){
	return (s8)readU8(data);
}

inline void writeF1000(u8 *data, f32 i){
	writeS32(data, i*FIXEDPOINT_FACTOR);
}
inline f32 readF1000(const u8 *data){
	return (f32)readS32(data)*FIXEDPOINT_INVFACTOR;
}

inline void writeV3S32(u8 *data, v3s32 p)
{
	writeS32(&data[0], p.X);
	writeS32(&data[4], p.Y);
	writeS32(&data[8], p.Z);
}
inline v3s32 readV3S32(const u8 *data)
{
	v3s32 p;
	p.X = readS32(&data[0]);
	p.Y = readS32(&data[4]);
	p.Z = readS32(&data[8]);
	return p;
}

inline void writeV3F1000(u8 *data, v3f p)
{
	writeF1000(&data[0], p.X);
	writeF1000(&data[4], p.Y);
	writeF1000(&data[8], p.Z);
}
inline v3f readV3F1000(const u8 *data)
{
	v3f p;
	p.X = (float)readF1000(&data[0]);
	p.Y = (float)readF1000(&data[4]);
	p.Z = (float)readF1000(&data[8]);
	return p;
}

inline void writeV2F1000(u8 *data, v2f p)
{
	writeF1000(&data[0], p.X);
	writeF1000(&data[4], p.Y);
}
inline v2f readV2F1000(const u8 *data)
{
	v2f p;
	p.X = (float)readF1000(&data[0]);
	p.Y = (float)readF1000(&data[4]);
	return p;
}

inline void writeV2S16(u8 *data, v2s16 p)
{
	writeS16(&data[0], p.X);
	writeS16(&data[2], p.Y);
}

inline v2s16 readV2S16(const u8 *data)
{
	v2s16 p;
	p.X = readS16(&data[0]);
	p.Y = readS16(&data[2]);
	return p;
}

inline void writeV2S32(u8 *data, v2s32 p)
{
	writeS32(&data[0], p.X);
	writeS32(&data[4], p.Y);
}

inline v2s32 readV2S32(const u8 *data)
{
	v2s32 p;
	p.X = readS32(&data[0]);
	p.Y = readS32(&data[4]);
	return p;
}

inline void writeV3S16(u8 *data, v3s16 p)
{
	writeS16(&data[0], p.X);
	writeS16(&data[2], p.Y);
	writeS16(&data[4], p.Z);
}

inline v3s16 readV3S16(const u8 *data)
{
	v3s16 p;
	p.X = readS16(&data[0]);
	p.Y = readS16(&data[2]);
	p.Z = readS16(&data[4]);
	return p;
}

inline void writeARGB8(u8 *data, video::SColor p)
{
	writeU32(data, p.color);
}

inline video::SColor readARGB8(const u8 *data)
{
	video::SColor p(readU32(data));
	return p;
}

/*
	The above stuff directly interfaced to iostream
*/

inline void writeU8(std::ostream &os, u8 p)
{
	char buf[1];
	writeU8((u8*)buf, p);
	os.write(buf, 1);
}
inline u8 readU8(std::istream &is)
{
	char buf[1] = {0};
	is.read(buf, 1);
	return readU8((u8*)buf);
}

inline void writeU16(std::ostream &os, u16 p)
{
	char buf[2];
	writeU16((u8*)buf, p);
	os.write(buf, 2);
}
inline u16 readU16(std::istream &is)
{
	char buf[2] = {0};
	is.read(buf, 2);
	return readU16((u8*)buf);
}

inline void writeU32(std::ostream &os, u32 p)
{
	char buf[4];
	writeU32((u8*)buf, p);
	os.write(buf, 4);
}
inline u32 readU32(std::istream &is)
{
	char buf[4] = {0};
	is.read(buf, 4);
	return readU32((u8*)buf);
}

inline void writeS32(std::ostream &os, s32 p)
{
	writeU32(os, (u32) p);
}
inline s32 readS32(std::istream &is)
{
	return (s32)readU32(is);
}

inline void writeS16(std::ostream &os, s16 p)
{
	writeU16(os, (u16) p);
}
inline s16 readS16(std::istream &is)
{
	return (s16)readU16(is);
}

inline void writeS8(std::ostream &os, s8 p)
{
	writeU8(os, (u8) p);
}
inline s8 readS8(std::istream &is)
{
	return (s8)readU8(is);
}

inline void writeF1000(std::ostream &os, f32 p)
{
	char buf[4];
	writeF1000((u8*)buf, p);
	os.write(buf, 4);
}
inline f32 readF1000(std::istream &is)
{
	char buf[4] = {0};
	is.read(buf, 4);
	return readF1000((u8*)buf);
}

inline void writeV3F1000(std::ostream &os, v3f p)
{
	char buf[12];
	writeV3F1000((u8*)buf, p);
	os.write(buf, 12);
}
inline v3f readV3F1000(std::istream &is)
{
	char buf[12];
	is.read(buf, 12);
	return readV3F1000((u8*)buf);
}

inline void writeV2F1000(std::ostream &os, v2f p)
{
	char buf[8];
	writeV2F1000((u8*)buf, p);
	os.write(buf, 8);
}
inline v2f readV2F1000(std::istream &is)
{
	char buf[8] = {0};
	is.read(buf, 8);
	return readV2F1000((u8*)buf);
}

inline void writeV2S16(std::ostream &os, v2s16 p)
{
	char buf[4];
	writeV2S16((u8*)buf, p);
	os.write(buf, 4);
}
inline v2s16 readV2S16(std::istream &is)
{
	char buf[4] = {0};
	is.read(buf, 4);
	return readV2S16((u8*)buf);
}

inline void writeV2S32(std::ostream &os, v2s32 p)
{
	char buf[8];
	writeV2S32((u8*)buf, p);
	os.write(buf, 8);
}
inline v2s32 readV2S32(std::istream &is)
{
	char buf[8] = {0};
	is.read(buf, 8);
	return readV2S32((u8*)buf);
}

inline void writeV3S16(std::ostream &os, v3s16 p)
{
	char buf[6];
	writeV3S16((u8*)buf, p);
	os.write(buf, 6);
}
inline v3s16 readV3S16(std::istream &is)
{
	char buf[6] = {0};
	is.read(buf, 6);
	return readV3S16((u8*)buf);
}

inline void writeARGB8(std::ostream &os, video::SColor p)
{
	char buf[4];
	writeARGB8((u8*)buf, p);
	os.write(buf, 4);
}

inline video::SColor readARGB8(std::istream &is)
{
	char buf[4] = {0};
	is.read(buf, 4);
	return readARGB8((u8*)buf);
}

/*
	More serialization stuff
*/

// Creates a string with the length as the first two bytes
std::string serializeString(const std::string &plain);

// Creates a string with the length as the first two bytes from wide string
std::string serializeWideString(const std::wstring &plain);

// Reads a string with the length as the first two bytes
std::string deSerializeString(std::istream &is);

// Reads a wide string with the length as the first two bytes
std::wstring deSerializeWideString(std::istream &is);

// Creates a string with the length as the first four bytes
std::string serializeLongString(const std::string &plain);

// Reads a string with the length as the first four bytes
std::string deSerializeLongString(std::istream &is);

// Creates a string encoded in JSON format (almost equivalent to a C string literal)
std::string serializeJsonString(const std::string &plain);

// Reads a string encoded in JSON format
std::string deSerializeJsonString(std::istream &is);

MSGPACK_DEFINE_EXTERNAL(v2f, X, Y);
MSGPACK_DEFINE_EXTERNAL(v3f, X, Y, Z);
//MSGPACK_DEFINE_EXTERNAL(v2s16, X, Y);
MSGPACK_DEFINE_EXTERNAL(v2s32, X, Y);
MSGPACK_DEFINE_EXTERNAL(v3s16, X, Y, Z);
MSGPACK_DEFINE_EXTERNAL(v3s32, X, Y, Z);
MSGPACK_DEFINE_EXTERNAL(video::SColor, color);
MSGPACK_DEFINE_EXTERNAL(aabb3f, MinEdge, MaxEdge);

// Creates a string containing comma delimited values of a struct whose layout is
// described by the parameter format
bool serializeStructToString(std::string *out,
	std::string format, void *value);

// Reads a comma delimited string of values into a struct whose layout is
// decribed by the parameter format
bool deSerializeStringToStruct(std::string valstr,
	std::string format, void *out, size_t olen);

#endif


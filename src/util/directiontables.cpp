// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "directiontables.h"
#include "irr_v3d.h"

const v3pos_t g_6dirs[6] =
{
	// +right, +top, +back
	v3pos_t( 0, 0, 1), // back
	v3pos_t( 0, 1, 0), // top
	v3pos_t( 1, 0, 0), // right
	v3pos_t( 0, 0,-1), // front
	v3pos_t( 0,-1, 0), // bottom
	v3pos_t(-1, 0, 0) // left
};

const v3pos_t g_7dirs[7] =
{
	v3pos_t(0,0,1), // back
	v3pos_t(0,1,0), // top
	v3pos_t(1,0,0), // right
	v3pos_t(0,0,-1), // front
	v3pos_t(0,-1,0), // bottom
	v3pos_t(-1,0,0), // left
	v3pos_t(0,0,0), // self
};

const v3pos_t g_26dirs[26] =
{
	// +right, +top, +back
	v3pos_t( 0, 0, 1), // back
	v3pos_t( 0, 1, 0), // top
	v3pos_t( 1, 0, 0), // right
	v3pos_t( 0, 0,-1), // front
	v3pos_t( 0,-1, 0), // bottom
	v3pos_t(-1, 0, 0), // left
	// 6
	v3pos_t(-1, 1, 0), // top left
	v3pos_t( 1, 1, 0), // top right
	v3pos_t( 0, 1, 1), // top back
	v3pos_t( 0, 1,-1), // top front
	v3pos_t(-1, 0, 1), // back left
	v3pos_t( 1, 0, 1), // back right
	v3pos_t(-1, 0,-1), // front left
	v3pos_t( 1, 0,-1), // front right
	v3pos_t(-1,-1, 0), // bottom left
	v3pos_t( 1,-1, 0), // bottom right
	v3pos_t( 0,-1, 1), // bottom back
	v3pos_t( 0,-1,-1), // bottom front
	// 18
	v3pos_t(-1, 1, 1), // top back-left
	v3pos_t( 1, 1, 1), // top back-right
	v3pos_t(-1, 1,-1), // top front-left
	v3pos_t( 1, 1,-1), // top front-right
	v3pos_t(-1,-1, 1), // bottom back-left
	v3pos_t( 1,-1, 1), // bottom back-right
	v3pos_t(-1,-1,-1), // bottom front-left
	v3pos_t( 1,-1,-1), // bottom front-right
	// 26
};

const v3pos_t g_27dirs[27] =
{
	// +right, +top, +back
	v3pos_t( 0, 0, 1), // back
	v3pos_t( 0, 1, 0), // top
	v3pos_t( 1, 0, 0), // right
	v3pos_t( 0, 0,-1), // front
	v3pos_t( 0,-1, 0), // bottom
	v3pos_t(-1, 0, 0), // left
	// 6
	v3pos_t(-1, 1, 0), // top left
	v3pos_t( 1, 1, 0), // top right
	v3pos_t( 0, 1, 1), // top back
	v3pos_t( 0, 1,-1), // top front
	v3pos_t(-1, 0, 1), // back left
	v3pos_t( 1, 0, 1), // back right
	v3pos_t(-1, 0,-1), // front left
	v3pos_t( 1, 0,-1), // front right
	v3pos_t(-1,-1, 0), // bottom left
	v3pos_t( 1,-1, 0), // bottom right
	v3pos_t( 0,-1, 1), // bottom back
	v3pos_t( 0,-1,-1), // bottom front
	// 18
	v3pos_t(-1, 1, 1), // top back-left
	v3pos_t( 1, 1, 1), // top back-right
	v3pos_t(-1, 1,-1), // top front-left
	v3pos_t( 1, 1,-1), // top front-right
	v3pos_t(-1,-1, 1), // bottom back-left
	v3pos_t( 1,-1, 1), // bottom back-right
	v3pos_t(-1,-1,-1), // bottom front-left
	v3pos_t( 1,-1,-1), // bottom front-right
	// 26
	v3pos_t(0,0,0),
};

const u8 wallmounted_to_facedir[8] = {
	20,
	0,
	16 + 1,
	12 + 3,
	8,
	4 + 2,
	20 + 1, // special 1
	0 + 1 // special 2
};

const v3s16 wallmounted_dirs[8] = {
	v3s16(0, 1, 0),
	v3s16(0, -1, 0),
	v3s16(1, 0, 0),
	v3s16(-1, 0, 0),
	v3s16(0, 0, 1),
	v3s16(0, 0, -1),
};

const v3pos_t facedir_dirs[32] = {
	//0
	v3pos_t(0, 0, 1),
	v3pos_t(1, 0, 0),
	v3pos_t(0, 0, -1),
	v3pos_t(-1, 0, 0),
	//4
	v3pos_t(0, -1, 0),
	v3pos_t(1, 0, 0),
	v3pos_t(0, 1, 0),
	v3pos_t(-1, 0, 0),
	//8
	v3pos_t(0, 1, 0),
	v3pos_t(1, 0, 0),
	v3pos_t(0, -1, 0),
	v3pos_t(-1, 0, 0),
	//12
	v3pos_t(0, 0, 1),
	v3pos_t(0, -1, 0),
	v3pos_t(0, 0, -1),
	v3pos_t(0, 1, 0),
	//16
	v3pos_t(0, 0, 1),
	v3pos_t(0, 1, 0),
	v3pos_t(0, 0, -1),
	v3pos_t(0, -1, 0),
	//20
	v3pos_t(0, 0, 1),
	v3pos_t(-1, 0, 0),
	v3pos_t(0, 0, -1),
	v3pos_t(1, 0, 0),
};

const v3s16 fourdir_dirs[4] = {
	v3s16(0, 0, 1),
	v3s16(1, 0, 0),
	v3s16(0, 0, -1),
	v3s16(-1, 0, 0),
};

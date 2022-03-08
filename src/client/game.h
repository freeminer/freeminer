/*
database.h
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
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

#include "irrlichttypes.h"
#include <string>
<<<<<<< HEAD:src/database.h
#include "util/basic_macros.h"
=======
>>>>>>> 5.5.0:src/client/game.h

class InputHandler;
class ChatBackend;
class RenderingEngine;
struct SubgameSpec;
struct GameStartData;

<<<<<<< HEAD:src/database.h
	virtual void beginSave() {}
	virtual void endSave() {}

	virtual bool saveBlock(const v3s16 &pos, const std::string &data) = 0;
	virtual void loadBlock(const v3s16 &pos, std::string *block) = 0;
	virtual bool deleteBlock(const v3s16 &pos) = 0;

	static s64 getBlockAsInteger(const v3s16 &pos);
	static v3s16 getIntegerAsBlock(s64 i);

	virtual void listAllLoadableBlocks(std::vector<v3s16> &dst) = 0;

	virtual bool initialized() const { return true; }


	std::string getBlockAsString(const v3POS &pos) const;
	v3POS getStringAsBlock(const std::string &i) const;
	virtual void open() {};
	virtual void close() {};
=======
struct Jitter {
	f32 max, min, avg, counter, max_sample, min_sample, max_fraction;
>>>>>>> 5.5.0:src/client/game.h
};

struct RunStats {
	u64 drawtime; // (us)

	Jitter dtime_jitter, busy_time_jitter;
};

struct CameraOrientation {
	f32 camera_yaw;    // "right/left"
	f32 camera_pitch;  // "up/down"
};


void the_game(bool *kill,
		InputHandler *input,
		RenderingEngine *rendering_engine,
		const GameStartData &start_data,
		std::string &error_message,
		ChatBackend &chat_backend,
		bool *reconnect_requested);

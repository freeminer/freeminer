/*
filecache.h
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
Copyright (C) 2013 Jonathan Neuschäfer <j.neuschaefer@gmx.net>
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

#include <iostream>
#include <string>
#include <string_view>

class FileCache
{
public:
	/*
		'dir' is the file cache directory to use.
	*/
	FileCache(const std::string &dir) : m_dir(dir) {}

	bool update(const std::string &name, std::string_view data);
	bool load(const std::string &name, std::ostream &os);
	bool exists(const std::string &name);

	// Copy another file on disk into the cache
	bool updateCopyFile(const std::string &name, const std::string &src_path);

private:
	std::string m_dir;

	void createDir();
	bool loadByPath(const std::string &path, std::ostream &os);
	bool updateByPath(const std::string &path, std::string_view data);
};

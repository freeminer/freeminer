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

#include "log_types.h"

std::ostream & operator<<(std::ostream & s, v3s16 p) {
	s << "(" << p.X << "," << p.Y << "," << p.Z << ")";
	return s;
}

std::ostream & operator<<(std::ostream & s, v3f p) {
	s << "(" << p.X << "," << p.Y << "," << p.Z << ")";
	return s;
}

std::ostream & operator<<(std::ostream & s, std::map<v3s16, unsigned int> & p) {
	for (auto & i: p)
		s << i.first << "=" << i.second<<" ";
	return s;
}

std::ostream & operator<<(std::ostream & s, irr::video::SColor c) {
	s << "c32(" << c.color << ": a=" << c.getAlpha()<< ",r=" << c.getRed()<< ",g=" << c.getGreen()<< ",b=" << c.getBlue() << ")";
	return s;
}

#include "util/string.h"
std::ostream & operator<<(std::ostream & s, const std::wstring & w) {
	s << wide_to_narrow(w);
	return s;
}



#include "mapnode.h"
std::ostream & operator<<(std::ostream & s, MapNode n) {
	s << "node["<<(int)n.param0<<","<<(int)n.param1<<","<<(int)n.param1<<"]";
	return s;
}

#include "noise.h"
struct NoiseParams;
std::ostream & operator<<(std::ostream & s, NoiseParams np) {
	s << "noiseprms[offset="<<np.offset<<",scale="<<np.scale<<",spread="<<np.spread<<",seed="<<np.seed<<",octaves="<<np.octaves<<",persist="<<np.persist<<",lacunarity="<<np.lacunarity<<",flags="<<np.flags
	<<",farscale"<<np.farscale<<",farspread"<<np.farspread<<",farpersist"<<np.farpersist
	<<"]";
	return s;
}

#include "json/json.h"
Json::StyledWriter writer;
std::ostream & operator<<(std::ostream & s, Json::Value & json) {
	s << writer.write(json);
	return s;
}

#include "settings.h"
std::ostream & operator<<(std::ostream & s, Settings & settings) {
	Json::Value json;
	settings.toJson(json);
	s << json;
	return s;
}

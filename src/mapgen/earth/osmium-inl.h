#pragma once

#include <memory>
#include <string>

class MapgenEarth;
class handler_i;

namespace earth_osmium_detail
{

std::shared_ptr<handler_i> make_handler(
		MapgenEarth *mg, const std::string &path, const std::string &bbox);

} // namespace earth_osmium_detail

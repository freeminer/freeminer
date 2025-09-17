#pragma once
#include "../arnis_adapter.h"

namespace arnis
{

bool generate_world(
		//MapgenEarth *mg,
		WorldEditor &editor, const std::vector<ProcessedElement> &elements,
		//const XZBBox& xzbbox,
		//const Ground& ground,
		const Args &args = {});

bool generate_world(WorldEditor &editor,
		//MapgenEarth *mg,

		const ProcessedElement &elements,
		//const XZBBox& xzbbox,
		//const Ground& ground,
		const Args &args = {});
}
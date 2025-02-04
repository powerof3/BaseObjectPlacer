#pragma once

#include "ObjectData.h"

namespace Config
{
	using ObjectMap = StringMap<std::vector<ObjectData>>;

	struct Format
	{
		std::size_t size() const { return cells.size() + objects.size(); }
		bool        empty() const { return cells.empty() && objects.empty(); }

		void clear()
		{
			cells.clear();
			objects.clear();
		}

		// members
		ObjectMap cells;
		ObjectMap objects;
	};
}

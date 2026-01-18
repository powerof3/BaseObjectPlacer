#pragma once

#include "ObjectData.h"

namespace Config
{
	using ObjectMap = StringMap<std::vector<ObjectData>>;
	using ConditionMap = std::vector<
		std::pair<
			std::vector<std::string>,
			std::vector<ObjectData>>>;

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
		REL::Version version{ 1, 0, 0, 0 };
		ObjectMap    cells;
		ObjectMap    objects;
	};
}

template <>
struct glz::meta<Config::Format>
{
	using T = Config::Format;
	static constexpr auto value = object(
		"version", &T::version,
		"cells", &T::cells,
		"objects", &T::objects);
};

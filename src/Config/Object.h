#pragma once

#include "ObjectArray.h"
#include "SharedData.h"

namespace Game
{
	class Object;
}

namespace Config
{
	struct SharedData
	{
		bool RollChance(std::size_t seed) const;

		ConfigExtraData                                   extraData;
		BSScript::ConfigScripts                           scripts;
		std::vector<std::string>                          conditions;
		Data::MotionType                                  motionType;
		std::vector<std::string>                          whiteList;
		std::vector<std::string>                          blackList;
		REX::EnumSet<Data::ReferenceFlags, std::uint32_t> flags;
		float                                             chance{ 1.0f };

	private:
		GENERATE_HASH(SharedData,
			a_val.extraData,
			a_val.scripts,
			a_val.conditions,
			a_val.motionType,
			a_val.whiteList,
			a_val.blackList,
			a_val.flags.underlying(),
			a_val.chance);
	};

	class Object
	{
	public:
		void GenerateHash();
		void CreateGameObject(std::vector<Game::Object>& a_objectVec, const std::variant<RE::RawFormID, std::string_view>& a_attachID) const;

		// members
		std::string                       uuid;
		std::vector<std::string>          bases;
		std::vector<RE::BSTransformRange> transforms;
		ObjectArray                       array;
		SharedData                        data;
		std::size_t                       baseHash;
	};

	using ObjectMap = StringMap<std::vector<Object>>;

	struct Format
	{
		std::size_t size() const { return cells.size() + objects.size() + objectTypes.size(); }
		bool        empty() const { return cells.empty() && objects.empty() && objectTypes.empty(); }

		void merge(Format& a_rhs)
		{
			cells.merge(a_rhs.cells);
			objects.merge(a_rhs.objects);
			objectTypes.merge(a_rhs.objectTypes);
		}
		
		void clear()
		{
			cells.clear();
			objects.clear();
			objectTypes.clear();
		}

		// members
		REL::Version version{ 1, 0, 0, 0 };
		ObjectMap    cells;
		ObjectMap    objects;
		ObjectMap    objectTypes;
	};
}

using ConfigObject = Config::Object;

template <>
struct glz::meta<ConfigObject>
{
	using T = ConfigObject;

	static constexpr auto read_flags = [](T& s, const std::string& input) {
		if (!input.empty()) {
			const auto flagStrs = string::split(input, "|");

			for (const auto& flagStr : flagStrs) {
				switch (string::const_hash(flagStr)) {
				case "NoAIAcquire"_h:
					s.data.flags.set(Data::ReferenceFlags::kNoAIAcquire);
					break;
				case "InitiallyDisabled"_h:
					s.data.flags.set(Data::ReferenceFlags::kInitiallyDisabled);
					break;
				case "HiddenFromLocalMap"_h:
					s.data.flags.set(Data::ReferenceFlags::kHiddenFromLocalMap);
					break;
				case "Inaccessible"_h:
					s.data.flags.set(Data::ReferenceFlags::kInaccessible);
					break;
				case "OpenByDefault"_h:
					s.data.flags.set(Data::ReferenceFlags::kOpenByDefault);
					break;
				case "IgnoredBySandbox"_h:
					s.data.flags.set(Data::ReferenceFlags::kIgnoredBySandbox);
					break;
				case "IsFullLOD"_h:
					s.data.flags.set(Data::ReferenceFlags::kIsFullLOD);
					break;
				case "Temporary"_h:
					s.data.flags.set(Data::ReferenceFlags::kTemporary);
					break;
				default:
					break;
				}
			}
		}
	};
	static constexpr auto write_flags = [](auto&) -> auto& { return ""; };

	static constexpr auto read_chance = [](T& s, float input) {
		s.data.chance = input / 100.0f;
	};
	static constexpr auto write_chance = [](auto& s) {
		return s.data.chance * 100.0f;
	};

	static constexpr auto value = object(
		"uniqueID", &T::uuid,
		"baseObject", &T::bases,
		"transforms", &T::transforms,
		"array", &T::array,
		"flags", glz::custom<read_flags, write_flags>,
		"motionType", [](auto&& self) -> auto& { return self.data.motionType; },
		"blackList", [](auto&& self) -> auto& { return self.data.blackList; },
		"whiteList", [](auto&& self) -> auto& { return self.data.whiteList; },
		"extraData", [](auto&& self) -> auto& { return self.data.extraData; },
		"scripts", [](auto&& self) -> auto& { return self.data.scripts; },
		"conditions", [](auto&& self) -> auto& { return self.data.conditions; },
		"chance", glz::custom<read_chance, write_chance>);
};

template <>
struct glz::meta<Config::Format>
{
	using T = Config::Format;
	static constexpr auto value = object(
		"version", &T::version,
		"cells", &T::cells,
		"objects", &T::objects,
		"objectTypes", &T::objectTypes);
};

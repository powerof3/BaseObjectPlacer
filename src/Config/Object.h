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
		REX::EnumSet<Data::ReferenceFlags, std::uint32_t> flags;
		float                                             chance{ 1.0f };

	private:
		GENERATE_HASH(SharedData,
			a_val.extraData,
			a_val.scripts,
			a_val.conditions,
			a_val.motionType,
			a_val.flags.underlying(),
			a_val.chance);
	};

	class Object
	{
	public:
		void GenerateHash();
		void CreateGameObject(std::vector<Game::Object>& a_objectVec, const std::variant<RE::FormID, std::string_view>& a_attachID) const;

		// members
		std::string                       uuid;
		std::vector<std::string>          bases;
		std::vector<RE::BSTransformRange> transforms;
		ObjectArray                       array;
		SharedData                        data;
		std::size_t                       baseHash;
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
	static constexpr auto write_flags = [](auto& s) -> auto& { return ""; };

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
		"extraData", [](auto&& self) -> auto& { return self.data.extraData; },
		"scripts", [](auto&& self) -> auto& { return self.data.scripts; },
		"conditions", [](auto&& self) -> auto& { return self.data.conditions; },
		"chance", glz::custom<read_chance, write_chance>);
};

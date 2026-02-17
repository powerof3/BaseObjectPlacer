#pragma once

#include "ObjectArray.h"
#include "SharedData.h"

namespace Game
{
	class RootObject;
}

namespace Config
{
	static constexpr REL::Version minConfigVersion{ 1, 0, 0, 0 };
	static constexpr REL::Version minPrefabVersion{ 1, 0, 0, 0 };

	struct FilterData
	{
		bool RollChance(std::size_t seed) const;

		std::vector<std::string> conditions;
		std::vector<std::string> whiteList;
		std::vector<std::string> blackList;
		float                    chance{ 1.0f };

	private:
		GENERATE_HASH(FilterData,
			a_val.conditions,
			a_val.whiteList,
			a_val.blackList,
			a_val.chance);
	};

	struct ObjectData
	{
		ConfigExtraData                                   extraData;
		BSScript::ConfigScripts                           scripts;
		Data::MotionType                                  motionType;
		REX::EnumSet<Data::ReferenceFlags, std::uint32_t> flags;

	private:
		GENERATE_HASH(ObjectData,
			a_val.extraData,
			a_val.scripts,
			a_val.motionType
			//a_val.flags.underlying()
		);
	};

	struct PrefabObject
	{
		std::vector<RE::TESBoundObject*> GetBases() const;

		std::vector<std::string> bases;
		RE::BSTransformRange     transform;  // local
		ObjectData               data;

	private:
		GENERATE_HASH(PrefabObject, a_val.bases, a_val.transform, a_val.data);
	};

	struct Prefab : PrefabObject
	{
		std::string               uuid;
		std::vector<PrefabObject> children;

	private:
		GENERATE_HASH(Prefab, a_val.bases, a_val.transform, a_val.data, a_val.uuid, a_val.children);
	};

	using PrefabOrUUID = std::variant<Prefab, std::string>;

	class PrefabList
	{
	public:
		// members
		REL::Version        version{ 1, 0, 0, 0 };
		std::vector<Prefab> prefabs;
	};

	class Object
	{
	public:
		void GenerateHash();
		void CreateGameObject(std::vector<Game::RootObject>& a_objectVec, const std::variant<RE::RawFormID, std::string_view>& a_attachID) const;

		// members
		PrefabOrUUID                      prefab;
		std::vector<RE::BSTransformRange> transforms;  // global
		ObjectArray                       array;
		FilterData                        filter;
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
struct glz::meta<Config::FilterData>
{
	using T = Config::FilterData;
	static constexpr bool requires_key(std::string_view, bool)
	{
		return false;
	}
	static constexpr auto read_chance = [](T& s, float input) {
		s.chance = input / 100.0f;
	};
	static constexpr auto write_chance = [](auto& s) {
		return s.chance * 100.0f;
	};
	static constexpr auto value = object(
		"blackList", &T::blackList,
		"whiteList", &T::whiteList,
		"conditions", &T::conditions,
		"chance", glz::custom<read_chance, write_chance>);
};

template <>
struct glz::meta<Config::PrefabObject>
{
	using T = Config::PrefabObject;
	static constexpr bool requires_key(std::string_view a_key, bool)
	{
		return a_key == "baseObjects";
	}
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
				case "SequentialObjects"_h:
					s.data.flags.set(Data::ReferenceFlags::kSequentialObjects);
					break;
				case "PreventClipping"_h:
					s.data.flags.set(Data::ReferenceFlags::kPreventClipping);
					break;
				default:
					break;
				}
			}
		}
	};
	static constexpr auto write_flags = [](auto&) -> auto& { return ""; };
	static constexpr auto value = object(
		"baseObjects", &T::bases,
		"transform", &T::transform,
		"flags", glz::custom<read_flags, write_flags>,
		"motionType", [](auto&& self) -> auto& { return self.data.motionType; },
		"extraData", [](auto&& self) -> auto& { return self.data.extraData; },
		"scripts", [](auto&& self) -> auto& { return self.data.scripts; });
};

template <>
struct glz::meta<Config::Prefab>
{
	using T = Config::Prefab;
	static constexpr bool requires_key(std::string_view a_key, bool)
	{
		return a_key == "uniqueID" || a_key == "baseObjects";
	}
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
				case "SequentialObjects"_h:
					s.data.flags.set(Data::ReferenceFlags::kSequentialObjects);
					break;
				case "PreventClipping"_h:
					s.data.flags.set(Data::ReferenceFlags::kPreventClipping);
					break;
				default:
					break;
				}
			}
		}
	};
	static constexpr auto write_flags = [](auto&) -> auto& { return ""; };
	static constexpr auto value = object(
		"uniqueID", &T::uuid,
		"baseObjects", &T::bases,
		"transform", &T::transform,
		"flags", glz::custom<read_flags, write_flags>,
		"motionType", [](auto&& self) -> auto& { return self.data.motionType; },
		"extraData", [](auto&& self) -> auto& { return self.data.extraData; },
		"scripts", [](auto&& self) -> auto& { return self.data.scripts; },
		"children", &T::children);
};

template <>
struct glz::meta<Config::PrefabList>
{
	using T = Config::PrefabList;
	static constexpr bool requires_key(std::string_view, bool)
	{
		return true;
	}
	static constexpr auto read_version = [](T& s, const REL::Version& a_version, glz::context& ctx) {
		if (a_version < Config::minPrefabVersion) {
			ctx.error = glz::error_code::constraint_violated;
			ctx.custom_error_message = "version mismatch. required version :" + Config::minPrefabVersion.string();
		} else {
			s.version = a_version;
		}
	};
	static constexpr auto value = object(
		"version", glz::custom<read_version, &T::version>,
		"prefabs", &T::prefabs);
};

template <>
struct glz::meta<ConfigObject>
{
	using T = ConfigObject;
	static constexpr bool requires_key(std::string_view key, bool)
	{
		return key == "prefab" || key == "transforms";
	}
	static constexpr auto value = object(
		"prefab", &T::prefab,
		"transforms", &T::transforms,
		"array", &T::array,
		"rules", &T::filter);
};

template <>
struct glz::meta<Config::Format>
{
	using T = Config::Format;
	static constexpr bool requires_key(std::string_view a_key, bool)
	{
		return a_key == "version";
	}
	static constexpr auto read_version = [](T& s, const REL::Version& a_version, glz::context& ctx) {
		if (a_version < Config::minConfigVersion) {
			ctx.error = glz::error_code::constraint_violated;
			ctx.custom_error_message = "version too low. required version : " + Config::minConfigVersion.string() + " or higher";
		} else {
			s.version = a_version;
		}
	};
	static constexpr auto value = object(
		"version", glz::custom<read_version, &T::version>,
		"cells", &T::cells,
		"objects", &T::objects,
		"objectTypes", &T::objectTypes);
};

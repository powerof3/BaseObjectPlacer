#pragma once

#include "ObjectArray.h"
#include "SharedData.h"

namespace Game
{
	class RootObject;
	struct ObjectData;
	class Object;
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
		using ReferenceFlags = Base::ReferenceFlags;

		void        ReadReferenceFlags(const std::string& input);
		std::string WriteReferenceFlags() const;

		ConfigExtraData                             extraData;
		BSScript::ConfigScripts                     scripts;
		Base::MotionType                            motionType;
		REX::EnumSet<ReferenceFlags, std::uint32_t> flags;

	private:
		static constexpr std::array<std::pair<std::string_view, ReferenceFlags>, 13> flagArray{
			{ { "NoAIAcquire"sv, ReferenceFlags::kNoAIAcquire },
				{ "InitiallyDisabled"sv, ReferenceFlags::kInitiallyDisabled },
				{ "HiddenFromLocalMap"sv, ReferenceFlags::kHiddenFromLocalMap },
				{ "Inaccessible"sv, ReferenceFlags::kInaccessible },
				{ "OpenByDefault"sv, ReferenceFlags::kOpenByDefault },
				{ "IgnoredBySandbox"sv, ReferenceFlags::kIgnoredBySandbox },
				{ "IsFullLOD"sv, ReferenceFlags::kIsFullLOD },
				{ "Temporary"sv, ReferenceFlags::kTemporary },
				{ "SequentialObjects"sv, ReferenceFlags::kSequentialObjects },
				{ "PreventClipping"sv, ReferenceFlags::kPreventClipping },
				{ "InheritFlags"sv, ReferenceFlags::kInheritFlags },
				{ "InheritExtraData"sv, ReferenceFlags::kInheritExtraData },
				{ "InheritScripts"sv, ReferenceFlags::kInheritScripts } }
		};

		GENERATE_HASH(ObjectData,
			a_val.extraData,
			a_val.scripts,
			a_val.motionType,
			a_val.flags.underlying());
	};

	struct Prefab;

	using PrefabOrUUID = std::variant<Prefab, std::string>;

	struct Prefab
	{
		struct WeightedObject
		{
			std::string object{};
			float       weight{ 100.0f };

		private:
			GENERATE_HASH(WeightedObject, a_val.object, a_val.weight);
		};

		static const Prefab*                       GetPrefabFromVariant(const PrefabOrUUID& a_variant);
		Base::WeightedObjects<RE::TESBoundObject*> GetBaseObjects() const;

		void Resolve();

		// members
		std::string                 uuid;
		Base::WeightedObjectVariant bases;
		RE::BSTransformRange        transform;  // local
		ObjectData                  data;
		ObjectArray                 array;
		FilterData                  filter;
		std::vector<PrefabOrUUID>   children;

	private:
		GENERATE_HASH(Prefab, a_val.uuid, a_val.bases, a_val.transform, a_val.data, a_val.array, a_val.filter, a_val.children);
	};

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
		void                             GenerateHash();
		static std::vector<Game::Object> BuildChildObjects(const std::vector<PrefabOrUUID>& a_children, std::size_t a_parentRootHash, const Game::ObjectData& a_parentData);
		void                             CreateGameObject(std::vector<Game::RootObject>& a_objectVec, const std::variant<RE::RawFormID, std::string_view>& a_attachID) const;

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
struct glz::meta<Config::Prefab::WeightedObject>
{
	using T = Config::Prefab::WeightedObject;
	static constexpr bool requires_key(std::string_view a_key, bool)
	{
		return a_key == "object";
	}
	static constexpr auto value = object(
		"object", &T::object,
		"weight", &T::weight);
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
		s.data.ReadReferenceFlags(input);
	};
	static constexpr auto write_flags = [](auto& s) -> std::string {
		return s.data.WriteReferenceFlags();
	};
	static constexpr auto read_bases = [](T& s, const std::vector<Config::Prefab::WeightedObject>& input) {
		Base::WeightedObjects<std::string> bases;
		for (auto& [object, weight] : input) {
			bases.emplace_back(object, weight);
		};
		s.bases = std::move(bases);
	};
	static constexpr auto write_bases = [](T& s) {
		std::vector<Config::Prefab::WeightedObject> vec;
		if (auto ptr = std::get_if<Base::WeightedObjects<std::string>>(&s.bases)) {
			vec.reserve(ptr->size());
			for (auto [object, weight] : std::ranges::views::zip(ptr->objects, ptr->weights)) {
				vec.emplace_back(object, weight);
			}
		}
		return vec;
	};

	static constexpr auto value = object(
		"uniqueID", &T::uuid,
		"baseObjects", glz::custom<read_bases, write_bases>,
		"transform", &T::transform,
		"flags", glz::custom<read_flags, write_flags>,
		"motionType", [](auto&& self) -> auto& { return self.data.motionType; },
		"extraData", [](auto&& self) -> auto& { return self.data.extraData; },
		"scripts", [](auto&& self) -> auto& { return self.data.scripts; },
		"rules", &T::filter,
		"array", &T::array,
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

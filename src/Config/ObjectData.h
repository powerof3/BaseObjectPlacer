#pragma once

#include "Common.h"

namespace Game
{
	class SourceData;
}

namespace Config
{
	struct ObjectArray
	{
		enum class Flags
		{
			kRandomizeRotation = 1 << 0,
			kRandomizeScale = 1 << 1,
			kIncrementRotation = 1 << 2,
			kIncrementScale = 1 << 3
		};

		struct Grid
		{
			struct Dimension
			{
				std::uint32_t count{ 0 };
				float         offset{};
			};

			std::size_t GetCount() const;
			void        GetTransforms(const RE::BSTransform& a_pivot, std::vector<RE::BSTransform>& a_transforms) const;

			// members
			Dimension xArray{};
			Dimension yArray{};
			Dimension zArray{};
		};

		struct Radial
		{
			std::size_t GetCount() const { return count; };
			void        GetTransforms(const RE::BSTransform& a_pivot, std::vector<RE::BSTransform>& a_transforms) const;

			// members
			std::uint32_t count{ 0 };
			float         angle{ 360.0f };
			float         angleStep{};
			float         radius{ 0.0f };
		};

		struct Word
		{
			struct Letter
			{
				std::vector<RE::NiPoint3> path;
			};

			static void InitCharMap();

			std::size_t GetCount() const;
			void        GetTransforms(const RE::BSTransform& a_pivot, std::vector<RE::BSTransform>& a_transforms) const;

			// members
			std::string word;
			float       size;
			float       spacing;

		private:
			static inline FlatMap<std::string, std::vector<RE::NiPoint3>> charMap{};
		};

		RE::NiPoint3                 GetRotationStep(const RE::BSTransformRange& a_pivotRange, std::size_t a_count) const;
		std::vector<RE::BSTransform> GetTransforms(const RE::BSTransformRange& a_pivotRange) const;

		// members
		Grid                               grid;
		Radial                             radial;
		Word                               words;
		std::size_t                        seed;
		REX::EnumSet<Flags, std::uint32_t> flags;
		RE::NiPoint3                       rotate;
	};

	class ObjectData
	{
	public:
		void CreateGameSourceData(std::vector<Game::SourceData>& a_srcDataVec, RE::FormID a_attachFormID, const std::string& a_attachForm) const;

		template <class T>
		std::size_t GenerateHash(const T& a_attachForm, std::size_t a_baseIdx, std::size_t a_transformIdx) const
		{
			std::size_t seed = 0;
			boost::hash_combine(seed, a_attachForm);
			boost::hash_combine(seed, uuid);
			boost::hash_combine(seed, bases);
			boost::hash_combine(seed, a_baseIdx);
			boost::hash_combine(seed, a_transformIdx);
			return seed;
		}

		// members
		std::string                                       uuid;
		std::vector<std::string>                          bases;
		ConfigExtraData                                   extraData;
		REX::EnumSet<Data::ReferenceFlags, std::uint32_t> flags;
		std::string                                       encounterZone;
		Data::MotionType                                  motionType;
		std::vector<RE::BSTransformRange>                 transforms;
		ObjectArray                                       array;
	};
}

using ConfigObject = Config::ObjectData;
using ConfigObjectArray = Config::ObjectArray;

template <>
struct glz::meta<ConfigObjectArray::Grid>
{
	using T = ConfigObjectArray::Grid;
	static constexpr auto value = object(
		"x", &T::xArray,
		"y", &T::yArray,
		"z", &T::zArray);
};

template <>
struct glz::meta<ConfigObjectArray::Radial>
{
	using T = ConfigObjectArray::Radial;

	static constexpr auto read_angle = [](ConfigObjectArray::Radial& s, const float input) {
		s.angle = RE::deg_to_rad(input);
		s.angleStep = s.angle / static_cast<float>(s.angle == RE::NI_TWO_PI ? s.count : s.count - 1);
	};
	static constexpr auto write_angle = [](auto& s) -> auto& { return RE::rad_to_deg(s.angle); };

	static constexpr auto value = object(
		"count", &T::count,
		"angle", glz::custom<read_angle, write_angle>,
		"radius", &T::radius);
};

template <>
struct glz::meta<ConfigObjectArray>
{
	using T = ConfigObjectArray;

	static constexpr auto read_rot = [](T& s, const RE::NiPoint3& input) {
		s.rotate = input * RE::NI_PI / 180;
	};

	static constexpr auto read_flags = [](T& s, const std::string& input) {
		if (!input.empty()) {
			const auto flagStrs = string::split(input, "|");

			for (const auto& flagStr : flagStrs) {
				switch (string::const_hash(flagStr)) {
				case "RandomizeRotation"_h:
					s.flags.set(T::Flags::kRandomizeRotation);
					break;
				case "RandomizeScale"_h:
					s.flags.set(T::Flags::kRandomizeScale);
					break;
				case "IncrementRotation"_h:
					s.flags.set(T::Flags::kIncrementRotation);
					break;
				case "IncrementScale"_h:
					s.flags.set(T::Flags::kIncrementScale);
					break;
				default:
					break;
				}
			}
		}
	};
	static constexpr auto write_flags = [](auto& s) -> auto& { return ""; };

	static constexpr auto value = object(
		"grid", &T::grid,
		"radial", &T::radial,
		"words", &T::words,
		"seed", &T::seed,
		"flags", glz::custom<read_flags, write_flags>,
		"rotate", glz::custom<read_rot, &T::rotate>);
};

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
					s.flags.set(Data::ReferenceFlags::kNoAIAcquire);
					break;
				case "InitiallyDisabled"_h:
					s.flags.set(Data::ReferenceFlags::kInitiallyDisabled);
					break;
				case "HiddenFromLocalMap"_h:
					s.flags.set(Data::ReferenceFlags::kHiddenFromLocalMap);
					break;
				case "Inaccessible"_h:
					s.flags.set(Data::ReferenceFlags::kInaccessible);
					break;
				case "OpenByDefault"_h:
					s.flags.set(Data::ReferenceFlags::kOpenByDefault);
					break;
				case "IgnoredBySandbox"_h:
					s.flags.set(Data::ReferenceFlags::kIgnoredBySandbox);
					break;
				case "IsFullLOD"_h:
					s.flags.set(Data::ReferenceFlags::kIsFullLOD);
					break;
				case "Temporary"_h:
					s.flags.set(Data::ReferenceFlags::kTemporary);
					break;
				default:
					break;
				}
			}
		}
	};
	static constexpr auto write_flags = [](auto& s) -> auto& { return ""; };

	static constexpr auto value = object(
		"uniqueID", &T::uuid,
		"baseObject", &T::bases,
		"transforms", &T::transforms,
		"flags", glz::custom<read_flags, write_flags>,
		"motionType", &T::motionType,
		"encounterZone", &T::encounterZone,
		"extraData", &T::extraData,
		"array", &T::array);
};

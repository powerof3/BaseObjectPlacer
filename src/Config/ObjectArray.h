#pragma once

#include "SharedData.h"

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

			private:
				GENERATE_HASH(Dimension, a_val.count, a_val.offset)
			};

			void GetTransforms(const RE::BSTransform& a_pivot, std::vector<RE::BSTransform>& a_transforms) const;

			// members
			Dimension xArray{};
			Dimension yArray{};
			Dimension zArray{};

		private:
			GENERATE_HASH(Grid, a_val.xArray, a_val.yArray, a_val.zArray)
		};

		struct Radial
		{
			void GetTransforms(const RE::BSTransform& a_pivot, std::vector<RE::BSTransform>& a_transforms) const;

			// members
			std::uint32_t count{ 0 };
			float         angle{ 360.0f };
			float         angleStep{};
			float         radius{ 0.0f };

		private:
			GENERATE_HASH(Radial, a_val.count, a_val.angle, a_val.angleStep, a_val.radius)
		};

		struct Word
		{
			struct Letter
			{
				std::vector<RE::NiPoint3> path;
			};

			static void InitCharMap();

			void GetTransforms(const RE::BSTransform& a_pivot, std::vector<RE::BSTransform>& a_transforms) const;

			// members
			std::string word;
			float       size;
			float       spacing;

		private:
			static inline FlatMap<char, std::vector<RE::NiPoint3>> charMap{};  // case sensitive!

			GENERATE_HASH(Word, a_val.word, a_val.size, a_val.spacing)
		};

		using ArrayVariant = std::variant<
			std::monostate,
			Grid,
			Radial,
			Word>;

		static RE::NiPoint3          GetRotationStep(const RE::BSTransformRange& a_pivotRange, std::size_t a_count);
		std::vector<RE::BSTransform> GetTransforms(const RE::BSTransformRange& a_pivotRange, std::size_t a_hash) const;

		// members
		ArrayVariant                       array;
		std::size_t                        seed;
		REX::EnumSet<Flags, std::uint32_t> flags;
		RE::NiPoint3                       rotate;

	private:
		GENERATE_HASH(ObjectArray,
			a_val.array,
			a_val.seed,
			a_val.flags.underlying(),
			a_val.rotate)
	};
}

using ConfigObjectArray = Config::ObjectArray;

template <>
struct glz::meta<ConfigObjectArray::Grid>
{
	using T = ConfigObjectArray::Grid;
	static constexpr bool requires_key(std::string_view, bool)
	{
		return true;
	}
	static constexpr auto value = object(
		"x", &T::xArray,
		"y", &T::yArray,
		"z", &T::zArray);
};

template <>
struct glz::meta<ConfigObjectArray::Radial>
{
	using T = ConfigObjectArray::Radial;
	static constexpr bool requires_key(std::string_view, bool)
	{
		return true;
	}
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
	
	static constexpr bool requires_key(std::string_view, bool)
	{
		return false;
	}

	template <class Type>
	static Type* access(T& s)
	{
		if (std::holds_alternative<std::monostate>(s.array)) {
			return &s.array.emplace<Type>();
		}
		if (std::holds_alternative<Type>(s.array)) {
			return std::get_if<Type>(&s.array);
		}
		return nullptr;
	}

	static constexpr auto read_rot = [](T& s, const RE::NiPoint3& input) {
		s.rotate.x = RE::deg_to_rad(input.x);
		s.rotate.y = RE::deg_to_rad(input.y);
		s.rotate.z = RE::deg_to_rad(input.z);
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
	static constexpr auto write_flags = [](auto&) -> auto& { return ""; };
	static constexpr auto value = object(
		"grid", [](T& s) { return access<ConfigObjectArray::Grid>(s); },
		"radial", [](T& s) { return access<ConfigObjectArray::Radial>(s); },
		"words", [](T& s) { return access<ConfigObjectArray::Word>(s); },
		"seed", &T::seed,
		"flags", glz::custom<read_flags, write_flags>,
		"rotate", glz::custom<read_rot, &T::rotate>);
};

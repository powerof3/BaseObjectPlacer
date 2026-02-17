#pragma once

namespace RE
{
	struct Point3Range
	{
		enum class Convert : std::uint8_t
		{
			kNone,
			kDegToRad,
			kRadToDeg
		};

		Point3Range() = default;
		Point3Range(const Point3Range& a_rhs, Convert type) :
			x(convert(a_rhs.x, type)),
			y(convert(a_rhs.y, type)),
			z(convert(a_rhs.z, type)),
			relative(a_rhs.relative)
		{}

		bool                   operator==(const Point3Range& a_rhs) const;
		[[nodiscard]] NiPoint3 min() const;
		[[nodiscard]] NiPoint3 max() const;
		[[nodiscard]] NiPoint3 value(std::size_t seed) const;

		// members
		Range<float> x;
		Range<float> y;
		Range<float> z;
		bool         relative{ false };

	private:
		static Range<float> convert(const Range<float>& range, Convert type)
		{
			switch (type) {
			case Convert::kDegToRad:
				return range.deg_to_rad();
			case Convert::kRadToDeg:
				return range.rad_to_deg();
			default:
				return range;
			}
		}

		GENERATE_HASH(Point3Range, a_val.x, a_val.y, a_val.z, a_val.relative)
	};

	struct ScaleRange : Range<float>
	{
		ScaleRange() :
			Range(1.0f)
		{}

		bool relative{ false };

	private:
		GENERATE_HASH(ScaleRange, a_val.min, a_val.max, a_val.relative)
	};

	class BSTransform;

	class BSTransformRange
	{
	public:
		BSTransformRange() = default;

		// members
		Point3Range rotate;
		Point3Range translate;
		ScaleRange  scale;

	private:
		GENERATE_HASH(BSTransformRange, a_val.rotate, a_val.translate, a_val.scale)
	};

	struct BoundingBox
	{
		BoundingBox() = default;
		BoundingBox(TESObjectREFR* a_ref);

		NiPoint3 pos;
		NiPoint3 boundMin;
		NiPoint3 boundMax;
		NiPoint3 extents;
	};

	class BSTransform
	{
	public:
		constexpr BSTransform() noexcept :
			rotate(0.f, 0.f, 0.f),
			translate(0.f, 0.f, 0.f),
			scale(1.0f)
		{}

		BSTransform(const BSTransformRange& a_transformRange, std::size_t a_seed) noexcept :
			rotate(a_transformRange.rotate.value(a_seed)),
			translate(a_transformRange.translate.value(a_seed)),
			scale(a_transformRange.scale.value(a_seed))
		{}

		bool operator==(const BSTransform& a_rhs) const;

		void ValidatePosition(TESObjectCELL* a_cell, TESObjectREFR* a_ref, const BoundingBox& a_refBB, const RE::NiPoint3& a_spawnExtents);

		// members
		NiPoint3 rotate;
		NiPoint3 translate;
		float    scale;

	private:
		GENERATE_HASH(BSTransform,
			a_val.rotate,
			a_val.translate,
			a_val.scale)
	};
}

template <>
struct glz::meta<RE::Point3Range>
{
	using T = RE::Point3Range;
	static constexpr bool requires_key(std::string_view a_key, bool)
	{
		return a_key != "relative";
	}
	static constexpr auto value = object(
		"x", &T::x,
		"y", &T::y,
		"z", &T::z,
		"relative", &T::relative);
};

template <>
struct glz::meta<RE::ScaleRange>
{
	using T = RE::ScaleRange;
	static constexpr bool requires_key(std::string_view a_key, bool)
	{
		return a_key == "min";
	}
	static constexpr auto value = object(
		"min", &T::min,
		"max", &T::max,
		"relative", &T::relative);
};

template <>
struct glz::meta<RE::BSTransformRange>
{
	using T = RE::BSTransformRange;
	static constexpr bool requires_key(std::string_view, bool)
	{
		return false;
	}
	static constexpr auto read_rot = [](T& s, const RE::Point3Range& input) {
		s.rotate = RE::Point3Range(input, RE::Point3Range::Convert::kDegToRad);
	};
	static constexpr auto write_rot = [](const T& s) {
		return RE::Point3Range(s.rotate, RE::Point3Range::Convert::kRadToDeg);
	};
	static constexpr auto value = object(
		"translate", &T::translate,
		"rotate", glz::custom<read_rot, write_rot>,
		"scale", &T::scale);
};

template <>
struct glz::meta<RE::BSTransform>
{
	using T = RE::BSTransform;
	static constexpr bool requires_key(std::string_view, bool)
	{
		return false;
	}
	static constexpr auto read_rot = [](T& s, const RE::NiPoint3& input) {
		s.rotate = input * RE::NI_PI / 180;
	};
	static constexpr auto value = object(
		"translate", &T::translate,
		"rotate", glz::custom<read_rot, &T::rotate>,
		"scale", &T::scale);
};

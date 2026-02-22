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
			z(convert(a_rhs.z, type))
		{}

		bool                   operator==(const Point3Range& a_rhs) const;
		[[nodiscard]] NiPoint3 min() const;
		[[nodiscard]] NiPoint3 max() const;
		[[nodiscard]] NiPoint3 value(std::size_t seed) const;

		// members
		Range<float> x;
		Range<float> y;
		Range<float> z;

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

		GENERATE_HASH(Point3Range, a_val.x, a_val.y, a_val.z)
	};

	using ScaleRange = Range<float>;

	class BSTransformRange
	{
	public:
		enum class Flags
		{
			kNone = 0,
			kRelativeRotation = 1 << 0,
			kRelativeTranslation = 1 << 1,
			kRelativeScale = 1 << 2
		};

		BSTransformRange() = default;

		// members
		Point3Range                       rotate;
		Point3Range                       translate;
		ScaleRange                        scale;
		REX::EnumSet<Flags, std::uint8_t> flags;

	private:
		GENERATE_HASH(BSTransformRange, a_val.rotate, a_val.translate, a_val.scale, a_val.flags.underlying())
	};

	struct BoundingBox
	{
		BoundingBox() = default;
		BoundingBox(TESObjectREFR* a_ref);

		NiPoint3 pos;
		NiPoint3 rot;
		NiPoint3 boundMin;
		NiPoint3 boundMax;
		NiPoint3 extents;
		float    scale;
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

namespace glz
{
	struct Point3Range : RE::Point3Range
	{
		Point3Range() = default;
		Point3Range(const RE::Point3Range& a_rhs, Convert type, bool a_relative) :
			RE::Point3Range(a_rhs, type),
			relative(a_relative)
		{}
		Point3Range(const RE::Point3Range& a_rhs, bool a_relative) :
			RE::Point3Range(a_rhs),
			relative(a_relative)
		{}

		bool relative{ false };
	};

	template <>
	struct meta<Point3Range>
	{
		using T = Point3Range;
		static constexpr bool requires_key(std::string_view a_key, bool)
		{
			return a_key == "x" || a_key == "y" || a_key == "z";
		}
		static constexpr auto value = object(
			"x", &T::x,
			"y", &T::y,
			"z", &T::z,
			"relative", &T::relative);
	};

	struct ScaleRange : RE::Range<float>
	{
		ScaleRange() = default;
		ScaleRange(const RE::ScaleRange& a_rhs, bool a_relative) :
			RE::ScaleRange(a_rhs),
			relative(a_relative)
		{}

		bool relative{ false };
	};

	template <>
	struct meta<ScaleRange>
	{
		using T = ScaleRange;
		static constexpr bool requires_key(std::string_view a_key, bool)
		{
			return a_key == "min";
		}
		static constexpr auto value = object(
			"min", &T::min,
			"max", &T::max,
			"relative", &T::relative);
	};
}

template <>
struct glz::meta<RE::BSTransformRange>
{
	using T = RE::BSTransformRange;
	static constexpr bool requires_key(std::string_view, bool)
	{
		return false;
	}
	static constexpr auto read_rot = [](T& s, const Point3Range& input) {
		s.rotate = RE::Point3Range(input, RE::Point3Range::Convert::kDegToRad);
		if (input.relative) {
			s.flags.set(RE::BSTransformRange::Flags::kRelativeRotation);
		}
	};
	static constexpr auto write_rot = [](const T& s) {
		return Point3Range(s.rotate, RE::Point3Range::Convert::kRadToDeg, s.flags.any(RE::BSTransformRange::Flags::kRelativeRotation));
	};
	static constexpr auto read_translate = [](T& s, const Point3Range& input) {
		s.translate = RE::Point3Range(input);
		if (input.relative) {
			s.flags.set(RE::BSTransformRange::Flags::kRelativeTranslation);
		}
	};
	static constexpr auto write_translate = [](const T& s) {
		return Point3Range(s.translate, s.flags.any(RE::BSTransformRange::Flags::kRelativeTranslation));
	};
	static constexpr auto read_scale = [](T& s, const ScaleRange& input) {
		s.scale = RE::ScaleRange(input);
		if (input.relative) {
			s.flags.set(RE::BSTransformRange::Flags::kRelativeScale);
		}
	};
	static constexpr auto write_scale = [](const T& s) {
		return ScaleRange(s.scale, s.flags.any(RE::BSTransformRange::Flags::kRelativeTranslation));
	};
	static constexpr auto value = object(
		"translate", glz::custom<read_translate, write_translate>,
		"rotate", glz::custom<read_rot, write_rot>,
		"scale", glz::custom<read_scale, write_scale>);
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
		s.rotate.x = RE::deg_to_rad(input.x);
		s.rotate.y = RE::deg_to_rad(input.y);
		s.rotate.z = RE::deg_to_rad(input.z);
	};
	static constexpr auto write_rot = [](T& s) {
		return RE::NiPoint3{
			RE::rad_to_deg(s.rotate.x),
			RE::rad_to_deg(s.rotate.y),
			RE::rad_to_deg(s.rotate.z)
		};
	};
	static constexpr auto value = object(
		"translate", &T::translate,
		"rotate", glz::custom<read_rot, write_rot>,
		"scale", &T::scale);
};

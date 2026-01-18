#pragma once

namespace RE
{
	template <class T>
	struct Range
	{
		bool operator==(const Range& a_rhs) const { return min == a_rhs.min && max == a_rhs.max; }

		T value(std::size_t seed) const
		{
			return (!max || min == *max) ? min :
			                               clib_util::RNG(seed).generate<T>(min, *max);
		}
		void deg_to_rad()
		{
			min = RE::deg_to_rad(min);
			if (max) {
				*max = RE::deg_to_rad(*max);
			}
		}

		// memebrs
		T                min{};
		std::optional<T> max{};

	private:
		GENERATE_HASH(Range, a_val.min, a_val.max)
	};

	struct Point3Range
	{
		bool         operator==(const Point3Range& a_rhs) const;
		RE::NiPoint3 min() const;
		RE::NiPoint3 max() const;
		RE::NiPoint3 value(std::size_t seed) const;
		Point3Range  deg_to_rad() const;

		// members
		Range<float> x{};
		Range<float> y{};
		Range<float> z{};

	private:
		GENERATE_HASH(Point3Range, a_val.x, a_val.y, a_val.z)
	};

	class BSTransform
	{
	public:
		constexpr BSTransform() noexcept
		{
			rotate = { 0.f, 0.f, 0.f };
			translate = { 0.f, 0.f, 0.f };

			scale = 1.0f;
		}

		constexpr BSTransform(const NiPoint3& a_rotate, const NiPoint3& a_translate, float a_scale) noexcept :
			rotate(a_rotate),
			translate(a_translate),
			scale(a_scale)
		{}
		bool operator==(const BSTransform& a_rhs) const;

		// members
		RE::NiPoint3 rotate{};
		RE::NiPoint3 translate{};
		float        scale{};

	private:
		GENERATE_HASH(BSTransform, a_val.rotate, a_val.translate, a_val.scale)
	};

	class BSTransformRange
	{
	public:
		BSTransform value(std::size_t a_seed) const;

		// members
		Point3Range  rotate;
		Point3Range  translate;
		Range<float> scale{ .min = 1.0f };

	private:
		GENERATE_HASH(BSTransformRange, a_val.rotate, a_val.translate, a_val.scale)
	};
}

template <>
struct glz::meta<RE::BSTransform>
{
	using T = RE::BSTransform;
	static constexpr auto read_rot = [](T& s, const RE::NiPoint3& input) {
		s.rotate = input * RE::NI_PI / 180;
	};
	static constexpr auto value = object(
		"translate", &T::translate,
		"rotate", glz::custom<read_rot, &T::rotate>,
		"scale", &T::scale);
};

template <>
struct glz::meta<RE::BSTransformRange>
{
	using T = RE::BSTransformRange;
	static constexpr auto read_rot = [](T& s, const RE::Point3Range& input) {
		s.rotate = input.deg_to_rad();
	};
	static constexpr auto value = object(
		"translate", &T::translate,
		"rotate", glz::custom<read_rot, &T::rotate>,
		"scale", &T::scale);
};

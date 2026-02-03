#pragma once

namespace RE
{
	template <class T>
		requires std::is_arithmetic_v<T>
	struct Range
	{
		Range() = default;

		explicit Range(T a_min) :
			min(a_min)
		{}

		bool operator==(const Range& a_rhs) const { return min == a_rhs.min && max == a_rhs.max; }

		[[nodiscard]] T value(std::size_t seed) const
		{
			return (!max || min == *max) ? min :
			                               clib_util::RNG(seed).generate<T>(min, *max);
		}

		[[nodiscard]] Range deg_to_rad() const
			requires std::is_floating_point_v<T>
		{
			Range range;
			range.min = RE::deg_to_rad(min);
			if (max) {
				range.max = RE::deg_to_rad(*max);
			}
			return range;
		}

		[[nodiscard]] Range rad_to_deg() const
			requires std::is_floating_point_v<T>
		{
			Range range;
			range.min = RE::rad_to_deg(min);
			if (max) {
				range.max = RE::rad_to_deg(*max);
			}
			return range;
		}

		// memebrs
		T                min{};
		std::optional<T> max{};

	private:
		GENERATE_HASH(Range, a_val.min, a_val.max)
	};

	// BGSNumericIndex
	inline bool operator<(const BGSNumericIDIndex& lhs, const BGSNumericIDIndex& rhs)
	{
		return std::tie(lhs.data1, lhs.data2, lhs.data3) < std::tie(rhs.data1, rhs.data2, rhs.data3);
	}

	inline bool operator==(const BGSNumericIDIndex& lhs, const BGSNumericIDIndex& rhs)
	{
		return std::tie(lhs.data1, lhs.data2, lhs.data3) == std::tie(rhs.data1, rhs.data2, rhs.data3);
	}

	inline std::size_t hash_value(const BGSNumericIDIndex& a_val) noexcept
	{
		return hash::combine(a_val.data1, a_val.data2, a_val.data3);
	}

	// NiPoint3
	inline std::size_t hash_value(const NiPoint3& a_val) noexcept
	{
		return hash::combine(a_val.x, a_val.y, a_val.z);
	}

	TESForm*    GetForm(const std::string& a_str);
	FormID      GetRawFormID(const std::string& a_str, bool a_checkEDID = false);
	FormID      GetFormID(const std::string& a_str);
	std::string GetEditorID(const std::string& a_str);

	// game function returns true for dynamic refs
	bool CanBeMoved(const TESObjectREFRPtr& a_refr);
	bool CanBeMoved(const TESForm* a_base);

	std::uint32_t GetNumReferenceHandles();
	bool          GetMaxFormIDReached();
	bool          IsFormIDInUse(FormID a_formID);

	NiPoint3 ApplyRotation(const NiPoint3& point, const NiPoint3& pivot, const NiMatrix3& rotationMatrix);

	void WrapAngle(NiPoint3& a_angle);

	void        SplitValue(std::size_t value, float& lowFloat, float& highFloat);
	std::size_t RecombineValue(float lowFloat, float highFloat);

	void InitScripts(TESObjectREFR* a_ref);
}

template <class T>
struct glz::meta<RE::Range<T>>
{
	using U = RE::Range<T>;
	static constexpr auto value = object(
		"min", &U::min,
		"max", &U::max);
};

template <>
struct glz::meta<REL::Version>
{
	using T = REL::Version;
	static constexpr auto read = [](REL::Version& input, const std::array<T::value_type, 4>& value) {
		input[0] = value[0];
		input[1] = value[1];
		input[2] = value[2];
		input[3] = value[3];
	};
	static constexpr auto write = [](REL::Version& input) -> auto {
		std::array<T::value_type, 4> vec{};
		vec[0] = input[0];
		vec[1] = input[1];
		vec[2] = input[2];
		vec[3] = input[3];
		return vec;
	};
	static constexpr auto value = custom<read, write>;
};

template <>
struct glz::meta<RE::NiPoint3>
{
	using T = RE::NiPoint3;
	static constexpr auto value = array(&T::x, &T::y, &T::z);
};

template <>
struct glz::meta<RE::NiMatrix3>
{
	static constexpr auto read = [](RE::NiMatrix3& input, const std::array<float, 3>& vec) {
		input.SetEulerAnglesXYZ(RE::deg_to_rad(vec[0]), RE::deg_to_rad(vec[1]), RE::deg_to_rad(vec[2]));
	};
	static constexpr auto write = [](auto& input) -> auto {
		std::array<float, 3> vec{};
		input.ToEulerAnglesXYZ(vec[0], vec[1], vec[2]);
		vec[0] = RE::rad_to_deg(vec[0]);
		vec[1] = RE::rad_to_deg(vec[1]);
		vec[2] = RE::rad_to_deg(vec[2]);
		return vec;
	};
	static constexpr auto value = custom<read, write>;
};

template <>
struct glz::meta<RE::NiTransform>
{
	using T = RE::NiTransform;
	static constexpr auto value = object(
		"translation", &T::translate,
		"rotation", &T::rotate,
		"scale", &T::scale);
};

template <>
struct glz::meta<RE::BGSNumericIDIndex>
{
	using T = RE::BGSNumericIDIndex;
	static constexpr auto value = array(&T::data1, &T::data2, &T::data3);
};

template <>
struct glz::meta<RE::LOCK_LEVEL>
{
	using enum RE::LOCK_LEVEL;
	static constexpr auto value = enumerate(
		"VeryEasy", kVeryEasy,
		"Easy", kEasy,
		"Average", kAverage,
		"Hard", kHard,
		"VeryHard", kVeryHard,
		"RequiresKey", kRequiresKey);
};

template <>
struct glz::meta<RE::SOUL_LEVEL>
{
	using enum RE::SOUL_LEVEL;
	static constexpr auto value = enumerate(
		"Petty", kPetty,
		"Lesser", kLesser,
		"Common", kCommon,
		"Greater", kGreater,
		"Grand", kGrand);
};

template <>
struct glz::meta<RE::hkpMotion::MotionType>
{
	using enum RE::hkpMotion::MotionType;
	static constexpr auto value = enumerate(
		"Invalid", kInvalid,
		"Dynamic", kDynamic,
		"SphereInertia", kSphereInertia,
		"BoxIntertia", kBoxInertia,
		"Keyframed", kKeyframed,
		"Fixed", kFixed,
		"ThinBoxInertia", kThinBoxInertia,
		"Character", kCharacter);
};

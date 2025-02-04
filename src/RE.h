#pragma once

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

namespace RE
{
	template <class T>
	void AttachNode(RE::NiNode* a_root, T* a_obj)
	{
		if (RE::TaskQueueInterface::ShouldUseTaskQueue()) {
			RE::TaskQueueInterface::GetSingleton()->QueueNodeAttach(a_obj, a_root);
		} else {
			a_root->AttachChild(a_obj);
		}
	}

	FormID      GetFormID(const std::string& a_str);
	std::string GetEditorID(const std::string& a_str);

	bool CanBeMoved(const TESObjectREFRPtr& a_refr);

	bool GetMaxFormIDReached();
	bool IsFormIDInUse(FormID a_formID);

	NiPoint3 ApplyRotation(const NiPoint3& point, const NiPoint3& pivot, const NiPoint3& rotationOffset);

	void WrapAngle(NiPoint3& a_angle);

	void        SplitValue(std::size_t value, float& lowFloat, float& highFloat);
	std::size_t RecombineValue(float lowFloat, float highFloat);
}

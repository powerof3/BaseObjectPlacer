#pragma once

namespace RE
{
	template <class T>
	struct Range
	{
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

		T avg() const
		{
			return max ? (min + *max) / 2 : min;
		}

		T                min;
		std::optional<T> max;
	};

	struct Point3Range
	{
		RE::NiPoint3 min() const
		{
			return RE::NiPoint3(x.min, y.min, z.min);
		}

		RE::NiPoint3 max() const
		{
			return RE::NiPoint3(x.max.value_or(0.0f), y.max.value_or(0.0f), z.max.value_or(0.0f));
		}

		RE::NiPoint3 value(std::size_t seed) const
		{
			return RE::NiPoint3(x.value(seed), y.value(seed), z.value(seed));
		}

		Point3Range deg_to_rad() const
		{
			Point3Range tmp = *this;
			tmp.x.deg_to_rad();
			tmp.y.deg_to_rad();
			tmp.z.deg_to_rad();
			return tmp;
		}

		Range<float> x{};
		Range<float> y{};
		Range<float> z{};
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
			scale(a_scale){};

		bool operator==(const BSTransform& a_rhs) const
		{
			return std::tie(rotate, translate, scale) == std::tie(a_rhs.rotate, a_rhs.translate, a_rhs.scale);
		}

		RE::NiPoint3 rotate{};
		RE::NiPoint3 translate{};
		float        scale{};
	};

	class BSTransformRange
	{
	public:
		BSTransform value(std::size_t a_seed) const
		{
			return BSTransform(rotate.value(a_seed), translate.value(a_seed), scale.value(a_seed));
		}

		Point3Range  rotate;
		Point3Range  translate;
		Range<float> scale{ .min = 1.0f };
	};
}

namespace Data
{
	enum class ReferenceFlags : uint32_t
	{
		kNoAIAcquire = 1 << 0,         // AI will not attempt to pick up containers or objects marked with this flag.
		kInitiallyDisabled = 1 << 1,   // The reference starts disabled ("not there") and must be enabled through script or game events.
		kHiddenFromLocalMap = 1 << 2,  // The reference will not be displayed on the local map.
		kInaccessible = 1 << 3,        // Only for Doors; makes them inoperable and shows "Inaccessible".
		kOpenByDefault = 1 << 4,       // For Doors/Containers; begins in the open state.
		kIgnoredBySandbox = 1 << 5,    // Prevents Sandboxing NPCs from using this reference (useful for furniture/idle markers).
		kIsFullLOD = 1 << 6,           // Reference will not fade from a distance.

		kTemporary = 1 << 3
	};

	struct MotionType
	{
		RE::hkpMotion::MotionType type{ RE::hkpMotion::MotionType::kInvalid };
		bool                      allowActivate;
	};
}

namespace Extra
{
	template <class T>
	struct Teleport
	{
		Teleport<T>() = default;
		Teleport<T>(const Teleport<T>& other) :
			linkedDoor(other.linkedDoor),
			position(other.position),
			rotation(other.rotation)
		{}

		Teleport<T>(const Teleport<std::string>& other)
			requires std::is_same_v<RE::FormID, T>
			:
			linkedDoor(RE::GetFormID(other.linkedDoor)),
			position(other.position),
			rotation(other.rotation)
		{}

		T            linkedDoor{};
		RE::NiPoint3 position{};
		RE::NiPoint3 rotation{};
	};

	template <class T>
	struct Lock
	{
		Lock<T>() = default;
		Lock<T>(const Lock<T>& other) :
			lockLevel(other.lockLevel),
			key(other.key)
		{}
		Lock<T>(const Lock<std::string>& other)
			requires std::is_same_v<RE::FormID, T>
			:
			lockLevel(other.lockLevel),
			key(RE::GetFormID(other.key))
		{}

		RE::LOCK_LEVEL lockLevel{ RE::LOCK_LEVEL::kUnlocked };
		T              key{};
	};

	template <class T>
	struct ExtraData
	{
		ExtraData<T>() = default;
		ExtraData<T>(const ExtraData<T>& other) :
			ownership(other.ownership),
			teleport(other.teleport),
			lock(other.lock),
			displayName(other.displayName),
			count(other.count),
			charge(other.charge),
			soul(other.soul)
		{}
		ExtraData<T>(const ExtraData<std::string>& other)
			requires std::is_same_v<RE::FormID, T>
			:
			ownership(RE::GetFormID(other.ownership)),
			teleport(other.teleport),
			lock(other.lock),
			displayName(other.displayName),
			count(other.count),
			charge(other.charge),
			soul(other.soul)
		{}

		void AddExtraData(RE::TESObjectREFR* a_ref) const
			requires std::is_same_v<RE::FormID, T>
		{
			if (auto owner = RE::TESForm::LookupByID(ownership)) {
				auto xData = RE::BSExtraData::Create<RE::ExtraOwnership>();
				xData->owner = owner;
				a_ref->extraList.Add(xData);
			}

			if (auto teleportRef = RE::TESForm::LookupByID<RE::TESObjectREFR>(teleport.linkedDoor); teleportRef && teleportRef->GetBaseObject() && teleportRef->GetBaseObject()->Is(RE::FormType::Door)) {
				RE::TESObjectDOOR::LinkRandomTeleportDoors(a_ref, teleportRef);
			}

			if (lock.lockLevel != RE::LOCK_LEVEL::kUnlocked || lock.key != 0) {
				if (auto lockData = a_ref->AddLock()) {
					lockData->baseLevel = static_cast<std::int8_t>(lock.lockLevel);
					if (lock.key != 0) {
						lockData->key = RE::TESForm::LookupByID<RE::TESKey>(lock.key);
					}
					lockData->SetLocked(true);
				}
			}

			if (soul != RE::SOUL_LEVEL::kNone) {
				a_ref->extraList.Add(new RE::ExtraSoul(soul));
			}

			if (!displayName.empty()) {
				auto xData = RE::BSExtraData::Create<RE::ExtraTextDisplayData>();
				xData->SetName(displayName.c_str());
				a_ref->extraList.Add(xData);
			}

			if (count > 1) {
				a_ref->extraList.SetCount(static_cast<std::uint16_t>(count));
			}

			if (charge > 0.0f) {
				auto xData = new RE::ExtraCharge();
				xData->charge = charge;
				a_ref->extraList.Add(xData);
			}
		}

		// members
		T              ownership{};
		Teleport<T>    teleport{};
		Lock<T>        lock{};
		RE::SOUL_LEVEL soul{ RE::SOUL_LEVEL::kNone };
		std::string    displayName{};
		std::uint32_t  count{};
		float          charge{};
	};
}

using ConfigExtraData = Extra::ExtraData<std::string>;
using GameExtraData = Extra::ExtraData<RE::FormID>;

template <>
struct glz::meta<Extra::Teleport<std::string>>
{
	using T = Extra::Teleport<std::string>;
	static constexpr auto value = object(
		"linkedDoor", &T::linkedDoor,
		"position", &T::position,
		"rotation", &T::rotation);
};

template <>
struct glz::meta<Extra::Lock<std::string>>
{
	using T = Extra::Lock<std::string>;
	static constexpr auto value = object(
		"level", &T::lockLevel,
		"key", &T::key);
};

template <>
struct glz::meta<ConfigExtraData>
{
	using T = ConfigExtraData;
	static constexpr auto value = object(
		"ownership", &T::ownership,
		"teleport", &T::teleport,
		"lock", &T::lock,
		"displayName", &T::displayName,
		"count", &T::count,
		"charge", &T::charge,
		"soul", &T::soul);
};

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

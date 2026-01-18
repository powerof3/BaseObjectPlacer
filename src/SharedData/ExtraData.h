#pragma once

#include "Transform.h"

// Find place to store this
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

		kTemporary = 1 << 14
	};

	struct MotionType
	{
		RE::hkpMotion::MotionType type{ RE::hkpMotion::MotionType::kInvalid };
		bool                      allowActivate;

	private:
		GENERATE_HASH(MotionType, a_val.type, a_val.allowActivate)
	};
}

namespace Extra
{
	template <class T>
	struct ActivateParent
	{
		ActivateParent<T>() = default;
		ActivateParent<T>(const ActivateParent<T>& other) :
			reference(other.reference),
			delay(other.delay)
		{}
		ActivateParent<T>(const ActivateParent<std::string>& other)
			requires std::is_same_v<RE::FormID, T>
			:
			reference(RE::GetFormID(other.reference)),
			delay(other.delay)
		{}

		T     reference{};
		float delay{ 0.0f };

	private:
		GENERATE_HASH(ActivateParent<T>, a_val.reference, a_val.delay)
	};

	template <class T>
	struct ActivateParents
	{
		ActivateParents<T>() = default;
		ActivateParents<T>(const ActivateParents<T>& other) :
			parents(other.parents),
			parentActivateOnly(other.parentActivateOnly)
		{}
		ActivateParents<T>(const ActivateParents<std::string>& other)
			requires std::is_same_v<RE::FormID, T>
			:
			parentActivateOnly(other.parentActivateOnly)
		{
			parents.reserve(other.parents.size());
			for (const auto& parent : other.parents) {
				parents.emplace_back(parent);
			}
		}

		bool empty() const { return parents.empty(); }

		std::vector<ActivateParent<T>> parents;
		bool                           parentActivateOnly{ false };

	private:
		GENERATE_HASH(ActivateParents<T>, a_val.parents, a_val.parentActivateOnly)
	};

	template <class T>
	struct EnableStateParent
	{
		EnableStateParent<T>() = default;
		EnableStateParent<T>(const EnableStateParent<T>& other) :
			reference(other.reference),
			oppositeState(other.oppositeState),
			popIn(other.popIn)
		{}
		EnableStateParent<T>(const EnableStateParent<std::string>& other)
			requires std::is_same_v<RE::FormID, T>
			:
			reference(RE::GetFormID(other.reference)),
			oppositeState(other.oppositeState),
			popIn(other.popIn)
		{}

		T    reference{};
		bool oppositeState{ false };
		bool popIn{ false };

	private:
		GENERATE_HASH(EnableStateParent<T>, a_val.reference, a_val.oppositeState, a_val.popIn)
	};

	template <class T>
	struct LinkedRef
	{
		LinkedRef<T>() = default;
		LinkedRef<T>(const LinkedRef<T>& other) :
			reference(other.reference),
			keyword(other.keyword)
		{}
		LinkedRef<T>(const LinkedRef<std::string>& other)
			requires std::is_same_v<RE::FormID, T>
			:
			reference(RE::GetFormID(other.reference)),
			keyword(RE::GetFormID(other.keyword))
		{}

		T reference{};
		T keyword{};

	private:
		GENERATE_HASH(LinkedRef<T>, a_val.reference, a_val.keyword)
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

	private:
		GENERATE_HASH(Lock<T>, a_val.lockLevel, a_val.key)
	};

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

	private:
		GENERATE_HASH(Teleport, a_val.linkedDoor, a_val.position, a_val.rotation)
	};

	template <class T>
	struct ExtraData
	{
		ExtraData<T>() = default;
		ExtraData<T>(const ExtraData<T>& other) :
			teleport(other.teleport),
			lock(other.lock),
			enableStateParent(other.enableStateParent),
			activateParents(other.activateParents),
			linkedRefs(other.linkedRefs),
			encounterZone(other.encounterZone),
			ownership(other.ownership),
			displayName(other.displayName),
			count(other.count),
			charge(other.charge),
			soul(other.soul)

		{}
		ExtraData<T>(const ExtraData<std::string>& other)
			requires std::is_same_v<RE::FormID, T>
			:
			teleport(other.teleport),
			lock(other.lock),
			enableStateParent(other.enableStateParent),
			activateParents(other.activateParents),
			encounterZone(RE::GetFormID(other.encounterZone)),
			ownership(RE::GetFormID(other.ownership)),
			displayName(other.displayName),
			count(other.count),
			charge(other.charge),
			soul(other.soul)
		{
			linkedRefs.reserve(other.linkedRefs.size());
			for (const auto& linkedRef : other.linkedRefs) {
				linkedRefs.emplace_back(linkedRef);
			}
		}

		void AddExtraData(RE::TESObjectREFR* a_ref, std::size_t a_hash) const
			requires std::is_same_v<RE::FormID, T>
		{
			if (encounterZone != 0) {
				if (auto encounterZoneForm = RE::TESForm::LookupByID<RE::BGSEncounterZone>(encounterZone)) {
					a_ref->extraList.SetEncounterZone(encounterZoneForm);
				}
			}

			if (ownership != 0) {
				if (auto owner = RE::TESForm::LookupByID(ownership)) {
					auto xData = RE::BSExtraData::Create<RE::ExtraOwnership>();
					xData->owner = owner;
					a_ref->extraList.Add(xData);
				}
			}

			if (enableStateParent.reference != 0) {
				if (auto parentRef = RE::TESForm::LookupByID<RE::TESObjectREFR>(enableStateParent.reference)) {
					auto xDataParent = RE::BSExtraData::Create<RE::ExtraEnableStateParent>();
					xDataParent->parent = parentRef->CreateRefHandle();
					if (enableStateParent.oppositeState) {
						xDataParent->flags |= 1 << 1;
					}
					if (enableStateParent.popIn) {
						xDataParent->flags |= 1 << 2;
					}
					a_ref->extraList.Add(xDataParent);
					auto xDataChildren = parentRef->extraList.GetByType<RE::ExtraEnableStateChildren>();
					if (!xDataChildren) {
						xDataChildren = RE::BSExtraData::Create<RE::ExtraEnableStateChildren>();
						parentRef->extraList.Add(xDataChildren);
					}
					if (xDataChildren) {
						xDataChildren->children.emplace_front(a_ref->CreateRefHandle());
					}
				}
			}

			if (teleport.linkedDoor != 0) {
				if (auto teleportRef = RE::TESForm::LookupByID<RE::TESObjectREFR>(teleport.linkedDoor); teleportRef && teleportRef->GetBaseObject() && teleportRef->GetBaseObject()->Is(RE::FormType::Door)) {
					RE::TESObjectDOOR::LinkRandomTeleportDoors(a_ref, teleportRef);
				}
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
				a_ref->extraList.SetOverrideName(displayName.c_str());
			}

			if (auto countVal = count.value(a_hash); countVal > 1) {
				a_ref->extraList.SetCount(static_cast<std::uint16_t>(countVal));
			}

			if (charge > 0.0f) {
				auto xData = new RE::ExtraCharge();
				xData->charge = charge;
				a_ref->extraList.Add(xData);
			}

			if (!activateParents.empty()) {
				for (auto& [refID, delay] : activateParents.parents) {
					if (auto parentRef = RE::TESForm::LookupByID<RE::TESObjectREFR>(refID)) {
						a_ref->extraList.SetActivateParent(parentRef, delay);
						parentRef->extraList.AddActivateRefChild(a_ref);
					}
				}
				if (activateParents.parentActivateOnly) {
					if (auto xData = a_ref->extraList.GetByType<RE::ExtraActivateRef>()) {
						xData->activateFlags |= 1 << 0;
					}
				}
			}

			if (!linkedRefs.empty()) {
				for (auto& [refID, keywordID] : linkedRefs) {
					if (auto parentRef = RE::TESForm::LookupByID<RE::TESObjectREFR>(refID)) {
						const auto keyword = RE::TESForm::LookupByID<RE::BGSKeyword>(keywordID);
						a_ref->extraList.SetLinkedRef(parentRef, keyword);

						auto xDataChildren = parentRef->extraList.GetByType<RE::ExtraLinkedRefChildren>();
						if (!xDataChildren) {
							xDataChildren = RE::BSExtraData::Create<RE::ExtraLinkedRefChildren>();
							parentRef->extraList.Add(xDataChildren);
						}

						xDataChildren->linkedChildren.push_back(RE::ExtraLinkedRefChildren::LinkedRefChild{ keyword, a_ref->CreateRefHandle() });
					}
				}
			}
		}

		// members
		Teleport<T>               teleport{};
		Lock<T>                   lock{};
		EnableStateParent<T>      enableStateParent{};
		ActivateParents<T>        activateParents{};
		std::vector<LinkedRef<T>> linkedRefs{};
		T                         encounterZone{};
		T                         ownership{};
		std::string               displayName{};
		RE::Range<std::uint32_t>  count{};
		float                     charge{};
		RE::SOUL_LEVEL            soul{ RE::SOUL_LEVEL::kNone };

	private:
		GENERATE_HASH(ExtraData<T>,
			a_val.teleport,
			a_val.lock,
			a_val.enableStateParent,
			a_val.activateParents,
			a_val.linkedRefs,
			a_val.encounterZone,
			a_val.ownership,
			a_val.displayName,
			a_val.count,
			a_val.charge,
			a_val.soul)
	};
}

using ConfigExtraData = Extra::ExtraData<std::string>;
using GameExtraData = Extra::ExtraData<RE::FormID>;

template <>
struct glz::meta<Extra::ActivateParent<std::string>>
{
	using T = Extra::ActivateParent<std::string>;
	static constexpr auto value = object(
		"reference", &T::reference,
		"delay", &T::delay);
};

template <>
struct glz::meta<Extra::ActivateParents<std::string>>
{
	using T = Extra::ActivateParents<std::string>;
	static constexpr auto value = object(
		"parents", &T::parents,
		"parentActivateOnly", &T::parentActivateOnly);
};

template <>
struct glz::meta<Extra::EnableStateParent<std::string>>
{
	using T = Extra::EnableStateParent<std::string>;
	static constexpr auto value = object(
		"reference", &T::reference,
		"oppositeState", &T::oppositeState,
		"popIn", &T::popIn);
};

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
struct glz::meta<Extra::LinkedRef<std::string>>
{
	using T = Extra::LinkedRef<std::string>;
	static constexpr auto value = object(
		"reference", &T::reference,
		"keyword", &T::keyword);
};

template <>
struct glz::meta<ConfigExtraData>
{
	using T = ConfigExtraData;
	static constexpr auto value = object(
		"ownership", &T::ownership,
		"activateParents", &T::activateParents,
		"linkedRefs", &T::linkedRefs,
		"enableStateParent", &T::enableStateParent,
		"teleport", &T::teleport,
		"lock", &T::lock,
		"displayName", &T::displayName,
		"count", &T::count,
		"charge", &T::charge,
		"soul", &T::soul);
};

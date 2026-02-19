#pragma once

#include "Transform.h"

namespace Extra
{
	template <class T>
	struct ActivateParent
	{
		ActivateParent() = default;
		ActivateParent(const ActivateParent& other) :
			reference(other.reference),
			delay(other.delay)
		{}

		explicit ActivateParent(const ActivateParent<std::string>& other)
			requires std::is_same_v<RE::FormID, T>
			:
			reference(RE::GetFormID(other.reference)),
			delay(other.delay)
		{}

		bool none() const { return reference == 0; }

		T     reference{};
		float delay{ 0.0f };

	private:
		GENERATE_HASH(ActivateParent, a_val.reference, a_val.delay)
	};

	template <class T>
	struct ActivateParents
	{
		ActivateParents() = default;
		ActivateParents(const ActivateParents& other) :
			parents(other.parents),
			parentActivateOnly(other.parentActivateOnly)
		{}

		explicit ActivateParents(const ActivateParents<std::string>& other)
			requires std::is_same_v<RE::FormID, T>
			:
			parents(other.parents.begin(), other.parents.end()),
			parentActivateOnly(other.parentActivateOnly)
		{}

		bool none() const { return parents.empty(); }

		void AddExtraData(RE::TESObjectREFR* a_ref) const
			requires std::is_same_v<RE::FormID, T>
		{
			for (auto& [refID, delay] : parents) {
				if (auto parentRef = RE::TESForm::LookupByID<RE::TESObjectREFR>(refID)) {
					a_ref->extraList.SetActivateParent(parentRef, delay);
					parentRef->extraList.AddActivateRefChild(a_ref);
					parentRef->AddChange(RE::TESObjectREFR::ChangeFlags::kActivatingChildren);
				}
			}
			if (parentActivateOnly) {
				if (auto xData = a_ref->extraList.GetByType<RE::ExtraActivateRef>()) {
					xData->activateFlags |= 1 << 0;
				}
			}
		}

		// members
		std::vector<ActivateParent<T>> parents;
		bool                           parentActivateOnly{ false };

	private:
		GENERATE_HASH(ActivateParents, a_val.parents, a_val.parentActivateOnly)
	};

	template <class T>
	struct EnableStateParent
	{
		EnableStateParent() = default;
		EnableStateParent(const EnableStateParent& other) :
			reference(other.reference),
			oppositeState(other.oppositeState),
			popIn(other.popIn)
		{}

		explicit EnableStateParent(const EnableStateParent<std::string>& other)
			requires std::is_same_v<RE::FormID, T>
			:
			reference(RE::GetFormID(other.reference)),
			oppositeState(other.oppositeState),
			popIn(other.popIn)
		{}

		bool none() const { return reference == 0; }

		void AddExtraData(RE::TESObjectREFR* a_ref) const
			requires std::is_same_v<RE::FormID, T>
		{
			if (reference != 0) {
				if (auto parentRef = RE::TESForm::LookupByID<RE::TESObjectREFR>(reference)) {
					auto xDataParent = RE::BSExtraData::Create<RE::ExtraEnableStateParent>();
					xDataParent->parent = parentRef->CreateRefHandle();
					if (oppositeState) {
						xDataParent->flags |= 1 << 1;
					}
					if (popIn) {
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
		}

		T    reference{};
		bool oppositeState{ false };
		bool popIn{ false };

	private:
		GENERATE_HASH(EnableStateParent, a_val.reference, a_val.oppositeState, a_val.popIn)
	};

	template <class T>
	struct LinkedRef
	{
		LinkedRef() = default;
		LinkedRef(const LinkedRef& other) :
			reference(other.reference),
			keyword(other.keyword)
		{}

		explicit LinkedRef(const LinkedRef<std::string>& other)
			requires std::is_same_v<RE::FormID, T>
			:
			reference(RE::GetFormID(other.reference)),
			keyword(RE::GetFormID(other.keyword))
		{}

		bool none() const { return reference == 0; }

		void AddExtraData(RE::TESObjectREFR* a_ref) const
			requires std::is_same_v<RE::FormID, T>
		{
			if (auto parentRef = RE::TESForm::LookupByID<RE::TESObjectREFR>(reference)) {
				const auto keywordForm = RE::TESForm::LookupByID<RE::BGSKeyword>(keyword);
				a_ref->extraList.SetLinkedRef(parentRef, keywordForm);

				auto xDataChildren = parentRef->extraList.GetByType<RE::ExtraLinkedRefChildren>();
				if (!xDataChildren) {
					xDataChildren = RE::BSExtraData::Create<RE::ExtraLinkedRefChildren>();
					parentRef->extraList.Add(xDataChildren);
				}

				xDataChildren->linkedChildren.push_back(RE::ExtraLinkedRefChildren::LinkedRefChild{ keywordForm, a_ref->CreateRefHandle() });
			}
		}

		// members
		T reference{};
		T keyword{};

	private:
		GENERATE_HASH(LinkedRef, a_val.reference, a_val.keyword)
	};

	template <class T>
	struct Lock
	{
		Lock() = default;
		Lock(const Lock& other) :
			lockLevel(other.lockLevel),
			key(other.key)
		{}

		explicit Lock(const Lock<std::string>& other)
			requires std::is_same_v<RE::FormID, T>
			:
			lockLevel(other.lockLevel),
			key(RE::GetFormID(other.key))
		{}

		bool none() const { return lockLevel == RE::LOCK_LEVEL::kUnlocked && key == 0; }

		void AddExtraData(RE::TESObjectREFR* a_ref) const
			requires std::is_same_v<RE::FormID, T>
		{
			if (lockLevel != RE::LOCK_LEVEL::kUnlocked || key != 0) {
				if (auto lockData = a_ref->AddLock()) {
					lockData->baseLevel = static_cast<std::int8_t>(lockLevel);
					if (key != 0) {
						lockData->key = RE::TESForm::LookupByID<RE::TESKey>(key);
					}
					lockData->SetLocked(true);
					a_ref->AddChange(RE::TESObjectREFR::ChangeFlags::kLockExtra);
				}
			}
		}

		// members
		RE::LOCK_LEVEL lockLevel{ RE::LOCK_LEVEL::kUnlocked };
		T              key{};

	private:
		GENERATE_HASH(Lock, a_val.lockLevel, a_val.key)
	};

	template <class T>
	struct Teleport
	{
		Teleport() = default;
		Teleport(const Teleport& other) :
			linkedDoor(other.linkedDoor),
			position(other.position),
			rotation(other.rotation)
		{}

		explicit Teleport(const Teleport<std::string>& other)
			requires std::is_same_v<RE::FormID, T>
			:
			linkedDoor(RE::GetFormID(other.linkedDoor)),
			position(other.position),
			rotation(other.rotation)
		{}

		bool none() const { return linkedDoor == 0; }

		void AddExtraData(RE::TESObjectREFR* a_ref) const
			requires std::is_same_v<RE::FormID, T>
		{
			if (linkedDoor != 0) {
				if (auto teleportRef = RE::TESForm::LookupByID<RE::TESObjectREFR>(linkedDoor); teleportRef && teleportRef->GetBaseObject() && teleportRef->GetBaseObject()->Is(RE::FormType::Door)) {
					RE::TESObjectDOOR::LinkRandomTeleportDoors(a_ref, teleportRef);
					if (auto xData = a_ref->extraList.GetByType<RE::ExtraTeleport>()) {
						if (xData->teleportData) {
							if (position != RE::NiPoint3::Zero()) {
								xData->teleportData->position = position;
							}
							if (rotation != RE::NiPoint3::Zero()) {
								xData->teleportData->rotation = rotation;
							}
						}
					}
				}
			}
		}

		// members
		T            linkedDoor{};
		RE::NiPoint3 position{};
		RE::NiPoint3 rotation{};

	private:
		GENERATE_HASH(Teleport, a_val.linkedDoor, a_val.position, a_val.rotation)
	};

	template <class T>
	class ExtraData
	{
	public:
		ExtraData() = default;
		ExtraData(const ExtraData& other) :
			teleport(other.teleport),
			lock(other.lock),
			enableStateParent(other.enableStateParent),
			activateParents(other.activateParents),
			linkedRefs(other.linkedRefs),
			encounterZone(other.encounterZone),
			ownership(other.ownership),
			displayName(other.displayName),
			count(other.count),
			charge(other.charge)
		{}
		ExtraData(const ExtraData<std::string>& other)
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
			charge(other.charge)
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
					a_ref->AddChange(RE::TESObjectREFR::ChangeFlags::kEncZoneExtra);
				}
			}

			if (ownership != 0) {
				if (auto owner = RE::TESForm::LookupByID(ownership)) {
					a_ref->SetOwner(owner);
				}
			}

			if (!displayName.empty()) {
				a_ref->extraList.SetOverrideName(displayName.c_str());
			}

			if (auto countVal = count.value(a_hash); countVal > 1) {
				a_ref->extraList.SetCount(static_cast<std::uint16_t>(countVal));
				//a_ref->AddChange(RE::TESObjectREFR::ChangeFlags::kItemExtraData); // persists even without change flag
			}

			// ??
			if (charge > 0.0f) {
				auto xData = new RE::ExtraCharge();
				xData->charge = charge;
				a_ref->extraList.Add(xData);
			}

			activateParents.AddExtraData(a_ref);
			enableStateParent.AddExtraData(a_ref);
			teleport.AddExtraData(a_ref);
			lock.AddExtraData(a_ref);

			for (const auto& linkedRef : linkedRefs) {
				linkedRef.AddExtraData(a_ref);
			}
		}

		void Merge(const ExtraData& a_source)
			requires std::is_same_v<RE::FormID, T>
		{
			if (encounterZone == 0) {
				encounterZone = a_source.encounterZone;
			}
			if (ownership == 0) {
				ownership = a_source.ownership;
			}
			/*if (displayName.empty()) {
				displayName = a_source.displayName;
			}
			if (count.min == 0) {
				count = a_source.count;
			}*/
			if (charge == 0.0f) {
				charge = a_source.charge;
			}
			if (activateParents.none()) {
				activateParents = a_source.activateParents;
			}
			if (enableStateParent.none()) {
				enableStateParent = a_source.enableStateParent;
			}
			if (teleport.none()) {
				teleport = a_source.teleport;
			}
			if (lock.none()) {
				lock = a_source.lock;
			}
			if (linkedRefs.empty()) {
				linkedRefs = a_source.linkedRefs;
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

	private:
		GENERATE_HASH(ExtraData,
			a_val.teleport,
			a_val.lock,
			a_val.enableStateParent,
			a_val.activateParents,
			a_val.linkedRefs,
			a_val.encounterZone,
			a_val.ownership,
			a_val.displayName,
			a_val.count,
			a_val.charge)
	};
}

using ConfigExtraData = Extra::ExtraData<std::string>;
using GameExtraData = Extra::ExtraData<RE::FormID>;

template <>
struct glz::meta<Extra::ActivateParent<std::string>>
{
	using T = Extra::ActivateParent<std::string>;
	static constexpr bool requires_key(std::string_view a_key, bool)
	{
		return a_key == "reference";
	}
	static constexpr auto value = object(
		"reference", &T::reference,
		"delay", &T::delay);
};

template <>
struct glz::meta<Extra::ActivateParents<std::string>>
{
	using T = Extra::ActivateParents<std::string>;
	static constexpr bool requires_key(std::string_view a_key, bool)
	{
		return a_key == "parents";
	}
	static constexpr auto value = object(
		"parents", &T::parents,
		"parentActivateOnly", &T::parentActivateOnly);
};

template <>
struct glz::meta<Extra::EnableStateParent<std::string>>
{
	using T = Extra::EnableStateParent<std::string>;
	static constexpr bool requires_key(std::string_view a_key, bool)
	{
		return a_key == "reference";
	}
	static constexpr auto value = object(
		"reference", &T::reference,
		"oppositeState", &T::oppositeState,
		"popIn", &T::popIn);
};

template <>
struct glz::meta<Extra::Teleport<std::string>>
{
	using T = Extra::Teleport<std::string>;
	static constexpr bool requires_key(std::string_view a_key, bool)
	{
		return a_key == "linkedDoor";
	}
	static constexpr auto value = object(
		"linkedDoor", &T::linkedDoor,
		"position", &T::position,
		"rotation", &T::rotation);
};

template <>
struct glz::meta<Extra::Lock<std::string>>
{
	using T = Extra::Lock<std::string>;
	static constexpr bool requires_key(std::string_view a_key, bool)
	{
		return a_key == "level";
	}
	static constexpr auto value = object(
		"level", &T::lockLevel,
		"key", &T::key);
};

template <>
struct glz::meta<Extra::LinkedRef<std::string>>
{
	using T = Extra::LinkedRef<std::string>;
	static constexpr bool requires_key(std::string_view a_key, bool)
	{
		return a_key == "reference";
	}
	static constexpr auto value = object(
		"reference", &T::reference,
		"keyword", &T::keyword);
};

template <>
struct glz::meta<ConfigExtraData>
{
	using T = ConfigExtraData;
	static constexpr bool requires_key(std::string_view, bool)
	{
		return false;
	}
	static constexpr auto value = object(
		"ownership", &T::ownership,
		"activateParents", &T::activateParents,
		"linkedRefs", &T::linkedRefs,
		"enableStateParent", &T::enableStateParent,
		"teleport", &T::teleport,
		"lock", &T::lock,
		"displayName", &T::displayName,
		"count", &T::count,
		"charge", &T::charge);
};

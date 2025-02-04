#include "GameData.h"

#include "Config/ObjectData.h"
#include "Manager.h"

Game::SourceData::SourceData(const Config::ObjectData& a_data, std::uint32_t a_attachID) :
	attachID(a_attachID),
	motionType(a_data.motionType),
	flags(a_data.flags),
	encounterZone(RE::GetFormID(a_data.encounterZone)),
	extraData(a_data.extraData)
{}

Game::SourceData::SourceData(const Config::ObjectData& a_data):
	SourceData(a_data, 0)
{}

bool Game::SourceData::IsTemporary(const RE::TESObjectREFRPtr& a_ref) const
{
	return flags.any(Data::ReferenceFlags::kTemporary) || !RE::CanBeMoved(a_ref);
}

RE::BSTransform Game::SourceData::GetTransform(RE::TESObjectREFR* a_ref) const
{
	RE::BSTransform    newTransform = transform;
	RE::TESObjectREFR* ref = a_ref;
	if (!ref) {
		ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(attachID);
	}
	if (ref) {
		newTransform.translate += ref->GetPosition();
		newTransform.rotate += ref->GetAngle();
	}
	return newTransform;
}

void Game::SourceData::SetPropertiesHavok([[maybe_unused]] RE::TESObjectREFR* a_ref, RE::NiAVObject* a_root) const
{
	if (a_root && motionType.type != RE::hkpMotion::MotionType::kInvalid) {
		a_root->SetMotionType(motionType.type, true, false, motionType.allowActivate);
	}
}

void Game::SourceData::SetPropertiesFlags(RE::TESObjectREFR* a_ref) const
{
	if (flags.any(Data::ReferenceFlags::kNoAIAcquire)) {
		a_ref->formFlags |= RE::TESObjectREFR::RecordFlags::kNoAIAcquire;
	}
	if (flags.any(Data::ReferenceFlags::kInitiallyDisabled)) {
		a_ref->formFlags |= RE::TESObjectREFR::RecordFlags::kInitiallyDisabled;
	}
	if (flags.any(Data::ReferenceFlags::kHiddenFromLocalMap)) {
		a_ref->SetOnLocalMap(false);
	}
	if (flags.any(Data::ReferenceFlags::kInaccessible)) {
		a_ref->formFlags |= RE::TESObjectREFR::RecordFlags::kInaccessible;
	}
	if (flags.any(Data::ReferenceFlags::kIgnoredBySandbox)) {
		a_ref->formFlags |= RE::TESObjectACTI::RecordFlags::kIgnoresObjectInteraction;
	}
	if (flags.any(Data::ReferenceFlags::kOpenByDefault)) {
		RE::BGSOpenCloseForm::SetOpenState(a_ref, true, true);
	}
	if (flags.any(Data::ReferenceFlags::kIsFullLOD)) {
		a_ref->formFlags |= RE::TESObjectREFR::RecordFlags::kIsFullLOD;
	}
}

void Game::SourceData::SetProperties(RE::TESObjectREFR* a_ref) const
{
	a_ref->SetScale(transform.scale);

	SetPropertiesFlags(a_ref);

	if (auto encounterZoneForm = RE::TESForm::LookupByID<RE::BGSEncounterZone>(encounterZone)) {
		a_ref->extraList.SetEncounterZone(encounterZoneForm);
	}

	extraData.AddExtraData(a_ref);
}

void Game::SourceData::SpawnObject(RE::TESObjectREFR* a_ref, RE::TESObjectCELL* a_cell, RE::TESWorldSpace* a_worldSpace) const
{
	if (Manager::GetSingleton()->GetSavedObject(hash) || RE::GetMaxFormIDReached()) {  // max id reached
		return;
	}

	if (auto form = RE::TESForm::LookupByID<RE::TESBoundObject>(baseID)) {
		auto newTransform = GetTransform(a_ref);
		auto createdRefHandle = RE::TESDataHandler::GetSingleton()->CreateReferenceAtLocation(form, newTransform.translate, newTransform.rotate, a_cell, a_worldSpace, nullptr, nullptr, {}, false, true);

		if (auto createdRef = createdRefHandle.get()) {
			Manager::GetSingleton()->SerializeObject(hash, createdRef, IsTemporary(createdRef));
			SetProperties(createdRef.get());
		}
	}
}

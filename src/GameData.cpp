#include "GameData.h"

#include "Config/ObjectData.h"
#include "Manager.h"

Game::SharedData::SharedData(const Config::SharedData& a_data) :
	extraData(a_data.extraData),
	conditions(ConditionParser::BuildCondition(a_data.conditions)),
	motionType(a_data.motionType),
	flags(a_data.flags),
	chance(a_data.chance)
{
	scripts.reserve(a_data.scripts.size());
	for (const auto& script : a_data.scripts) {
		scripts.emplace_back(script);
	}
}

bool Game::SharedData::IsTemporary() const
{
	return flags.any(Data::ReferenceFlags::kTemporary) || conditions != nullptr;
}

void Game::SharedData::SetPropertiesHavok([[maybe_unused]] RE::TESObjectREFR* a_ref, RE::NiAVObject* a_root) const
{
	if (a_root && motionType.type != RE::hkpMotion::MotionType::kInvalid) {
		a_root->SetMotionType(motionType.type, true, false, motionType.allowActivate);
	}
}

void Game::SharedData::SetPropertiesFlags(RE::TESObjectREFR* a_ref) const
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

void Game::SharedData::AttachScripts(RE::TESObjectREFR* a_ref) const
{
	if (scripts.empty()) {
		return;
	}

	bool  attachedScripts = false;
	auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
	auto* handlePolicy = vm->GetObjectHandlePolicy();
	auto* bindPolicy = vm->GetObjectBindPolicy();

	RE::VMHandle handle = handlePolicy->GetHandleForObject(a_ref->GetFormType(), a_ref);
	if (!handle) {
		return;
	}

	for (const auto& [scriptName, properties, autoFill] : scripts) {
		bool alreadyAttached = false;

		if (auto it = vm->attachedScripts.find(handle); it != vm->attachedScripts.end()) {
			for (auto& attachedScript : it->second) {
				if (auto* typeInfo = attachedScript->GetTypeInfo()) {
					if (string::iequals(typeInfo->GetName(), scriptName)) {
						alreadyAttached = true;
						break;
					}
				}
			}
		}

		if (alreadyAttached) {
			continue;
		}

		RE::BSTSmartPointer<RE::BSScript::Object> objectPtr;
		if (!vm->CreateObject(scriptName, objectPtr) || !objectPtr) {
			continue;
		}

		StringSet manualProperties;

		if (!properties.empty()) {
			for (auto& [propName, property] : properties) {
				auto propInfo = objectPtr->GetProperty(propName);
				if (!propInfo) {
					continue;
				}
				std::visit(overload{
							   [&](std::monostate) {},
							   [&](const RE::BSFixedString& a_val) {
								   propInfo->SetString(a_val);
							   },
							   [&](const RE::TESForm* a_val) {
								   RE::BSTSmartPointer<RE::BSScript::Object> obj;
								   RE::VMHandle                              handle = handlePolicy->GetHandleForObject(a_val->GetFormType(), a_val);
								   if (handle && vm->CreateObject(scriptName, obj) && obj) {
									   if (bindPolicy) {
										   bindPolicy->BindObject(obj, handle);
										   propInfo->SetObject(obj);
									   }
								   }
							   },
							   [&](const std::int32_t& a_val) {
								   propInfo->SetSInt(a_val);
							   },
							   [&](const float& a_val) {
								   propInfo->SetFloat(a_val);
							   },
							   [&](const bool& a_val) {
								   propInfo->SetBool(a_val);
							   },
							   [&](const std::vector<RE::TESForm*>& a_val) {
								   CreateScriptArray(scriptName, propInfo, a_val);
							   },
							   [&](const std::vector<std::string>& a_val) {
								   CreateScriptArray(scriptName, propInfo, a_val);
							   },
							   [&](const std::vector<std::int32_t>& a_val) {
								   CreateScriptArray(scriptName, propInfo, a_val);
							   },
							   [&](const std::vector<float>& a_val) {
								   CreateScriptArray(scriptName, propInfo, a_val);
							   },
							   [&](const std::vector<bool>& a_val) {
								   CreateScriptArray(scriptName, propInfo, a_val);
							   } },
					property);
				manualProperties.emplace(propName);
			}
		}

		if (autoFill) {
			if (const auto typeInfo = objectPtr->GetTypeInfo()) {
				auto* scriptProperties = typeInfo->GetPropertyIter();
				for (std::uint32_t i = 0; i < typeInfo->propertyCount; i++) {
					auto& [propertyName, propertyInfo] = scriptProperties[i];

					if (manualProperties.contains(propertyName)) {
						continue;
					}

					auto* form = RE::TESForm::LookupByEditorID(propertyName);
					if (!form) {
						continue;
					}

					auto* propertyVariable = objectPtr->GetProperty(propertyName);
					if (!propertyVariable || !propertyVariable->IsObject()) {
						continue;
					}

					RE::VMHandle                              propHandle = handlePolicy->GetHandleForObject(form->GetFormType(), form);
					RE::BSTSmartPointer<RE::BSScript::Object> propObject;
					if (handle && vm->CreateObject(scriptName, propObject) && propObject) {
						if (bindPolicy) {
							bindPolicy->BindObject(propObject, propHandle);
							propertyVariable->SetObject(propObject);
						}
					}
				}
			}
		}

		if (bindPolicy) {
			bindPolicy->BindObject(objectPtr, handle);
			attachedScripts = true;
		}
	}

	if (attachedScripts) {
		RE::InitScripts(a_ref);
	}
}

Game::SourceData::SourceData(std::shared_ptr<SharedData> a_data, std::uint32_t a_attachID) :
	attachID(a_attachID),
	data(a_data)
{}

Game::SourceData::SourceData(std::shared_ptr<SharedData> a_data) :
	SourceData(std::move(a_data), 0)
{}

bool Game::SourceData::IsTemporary(const RE::TESObjectREFRPtr& a_ref) const
{
	return data->IsTemporary() || !RE::CanBeMoved(a_ref);
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
		RE::WrapAngle(newTransform.rotate);
	}
	return newTransform;
}

void Game::SourceData::SetProperties(RE::TESObjectREFR* a_ref) const
{
	a_ref->SetScale(transform.scale);
	data->SetPropertiesFlags(a_ref);
	data->AttachScripts(a_ref);
	data->extraData.AddExtraData(a_ref, hash);
}

void Game::SourceData::SpawnObject(RE::TESObjectREFR* a_ref, RE::TESObjectCELL* a_cell, RE::TESWorldSpace* a_worldSpace) const
{
	if (Manager::GetSingleton()->GetSavedObject(hash) || RE::GetNumReferenceHandles() >= 1000000 || RE::GetMaxFormIDReached()) {  // max id reached
		return;
	}

	if (data->conditions) {
		auto conditionRef = a_ref ? a_ref : RE::PlayerCharacter::GetSingleton();
		if (!data->conditions->IsTrue(conditionRef, conditionRef)) {
			return;
		}
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

#include "Game/Object.h"

#include "Config/Object.h"
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
	bool changedFlags = false;
	if (flags.any(Data::ReferenceFlags::kNoAIAcquire)) {
		a_ref->formFlags |= RE::TESObjectREFR::RecordFlags::kNoAIAcquire;
		changedFlags = true;
	}
	if (flags.any(Data::ReferenceFlags::kInitiallyDisabled)) {
		a_ref->formFlags |= RE::TESObjectREFR::RecordFlags::kInitiallyDisabled;
		changedFlags = true;
	}
	if (flags.any(Data::ReferenceFlags::kHiddenFromLocalMap)) {
		a_ref->SetOnLocalMap(false);
		changedFlags = true;
	}
	if (flags.any(Data::ReferenceFlags::kInaccessible)) {
		a_ref->formFlags |= RE::TESObjectREFR::RecordFlags::kInaccessible;
		changedFlags = true;
	}
	if (flags.any(Data::ReferenceFlags::kIgnoredBySandbox)) {
		a_ref->formFlags |= RE::TESObjectACTI::RecordFlags::kIgnoresObjectInteraction;
		changedFlags = true;
	}
	if (flags.any(Data::ReferenceFlags::kOpenByDefault)) {
		RE::BGSOpenCloseForm::SetOpenState(a_ref, true, true);
		a_ref->AddChange(RE::TESObjectREFR::ChangeFlags::kGameOnlyExtra);
	}
	if (flags.any(Data::ReferenceFlags::kIsFullLOD)) {
		a_ref->formFlags |= RE::TESObjectREFR::RecordFlags::kIsFullLOD;
		changedFlags = true;
	}
	if (changedFlags) {
		a_ref->AddChange(RE::TESForm::ChangeFlags::kFlags);
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
								   RE::VMHandle                              a_handle = handlePolicy->GetHandleForObject(a_val->GetFormType(), a_val);
								   if (a_handle && vm->CreateObject(scriptName, obj) && obj) {
									   if (bindPolicy) {
										   bindPolicy->BindObject(obj, a_handle);
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
					if (propHandle && vm->CreateObject(scriptName, propObject) && propObject) {
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

Game::Object::Instance::Instance(std::uint32_t a_baseIndex, const RE::BSTransformRange& a_range, const RE::BSTransform& a_transform, Flags a_flags, std::size_t a_hash) :
	baseIndex(a_baseIndex),
	transform(a_transform),
	transformRange(a_range),
	flags(a_flags),
	hash(a_hash)
{}

Game::Object::Instance::Instance(std::uint32_t a_baseIndex, const RE::BSTransformRange& a_range, Flags a_flags, std::size_t a_hash) :
	baseIndex(a_baseIndex),
	transform(a_range, a_hash),
	transformRange(a_range),
	flags(a_flags),
	hash(a_hash)
{}

Game::Object::Instance::Flags Game::Object::Instance::GetInstanceFlags(const RE::BSTransformRange& a_range, const Config::ObjectArray& a_array)
{
	REX::EnumSet flags(Flags::kNone);
	if (a_range.translate.relative) {
		flags.set(Flags::kRelativeTranslate);
	}
	if (a_range.rotate.relative) {
		flags.set(Flags::kRelativeRotate);
	}
	if (a_range.scale.relative) {
		flags.set(Flags::kRelativeScale);
	}
	if (a_array.flags.any(Config::ObjectArray::Flags::kRandomizeRotation)) {
		flags.set(Flags::kRandomizeRotation);
	}
	if (a_array.flags.any(Config::ObjectArray::Flags::kRandomizeScale)) {
		flags.set(Flags::kRandomizeScale);
	}
	return *flags;
}

RE::BSTransform Game::Object::Instance::GetWorldTransform(const RE::NiPoint3& a_refPos, const RE::NiPoint3& a_refAngle, std::size_t a_hash) const
{
	RE::BSTransform newTransform = transform;
	newTransform.translate += a_refPos;
	if (flags.any(Flags::kRandomizeRotation)) {
		newTransform.rotate = transformRange.rotate.value(a_hash);
		RE::WrapAngle(newTransform.rotate);
	}
	if (flags.any(Flags::kRelativeRotate)) {
		newTransform.rotate += a_refAngle;
		RE::WrapAngle(newTransform.rotate);
	}
	if (flags.any(Flags::kRandomizeScale)) {
		newTransform.scale = transformRange.scale.value(a_hash);
	}
	return newTransform;
}

Game::Object::Object(const Config::SharedData& a_data) :
	data(a_data)
{}

void Game::Object::SetProperties(RE::TESObjectREFR* a_ref, std::size_t hash) const
{
	data.SetPropertiesFlags(a_ref);
	data.AttachScripts(a_ref);
	data.extraData.AddExtraData(a_ref, hash);
}

void Game::Object::SpawnObject(RE::TESDataHandler* a_dataHandler, RE::TESObjectREFR* a_ref, RE::TESObjectCELL* a_cell, RE::TESWorldSpace* a_worldSpace, bool) const
{
	if (data.conditions) {
		auto conditionRef = a_ref ? a_ref : RE::PlayerCharacter::GetSingleton();
		if (!data.conditions->IsTrue(conditionRef, conditionRef)) {
			return;
		}
	}

	const auto refPos = a_ref ? a_ref->GetPosition() : RE::NiPoint3();
	const auto refAngle = a_ref ? a_ref->GetAngle() : RE::NiPoint3();
	const auto refScale = a_ref ? a_ref->GetScale() : 1.0f;

	std::vector<RE::TESBoundObject*> forms;
	forms.reserve(bases.size());
	for (auto& baseID : bases) {
		forms.emplace_back(RE::TESForm::LookupByID<RE::TESBoundObject>(baseID));
	}

	for (auto& instance : instances) {
		auto hash = instance.hash;
		if (a_ref && !a_ref->IsDynamicForm()) {
			hash = hash::combine(instance.hash, a_ref->GetLocalFormID(), a_ref->GetFile(0)->fileName);
			Manager::GetSingleton()->AddConfigObject(hash, this);
		}

		if (auto id = Manager::GetSingleton()->GetSavedObject(hash); id != 0) {
			logger::info("\t[{:X}]{:X} already exists, skipping spawn.", hash, id);
			continue;
		}

		if (RE::GetNumReferenceHandles() >= 1000000 || RE::GetMaxFormIDReached()) {  // max id reached
			continue;
		}

		const auto baseObject = forms[instance.baseIndex];
		auto       transform = instance.GetWorldTransform(refPos, refAngle, hash);
		/*if (a_doRayCast && (data.motionType.type == RE::hkpMotion::MotionType::kKeyframed || !RE::CanBeMoved(baseObject))) {
			RE::NiPoint3 halfExtents{
				static_cast<float>(baseObject->boundData.boundMax.x - baseObject->boundData.boundMin.x),
				static_cast<float>(baseObject->boundData.boundMax.y - baseObject->boundData.boundMin.y),
				static_cast<float>(baseObject->boundData.boundMax.z - baseObject->boundData.boundMin.z)
			};
			transform.translate = RE::ValidateSpawnPosition(a_cell, a_ref, refPos, transform, halfExtents);
		}*/

		auto createdRefHandle = a_dataHandler->CreateReferenceAtLocation(
			baseObject,
			transform.translate,
			transform.rotate,
			a_cell,
			a_worldSpace,
			nullptr,
			nullptr,
			{},
			false,
			true);

		if (auto createdRef = createdRefHandle.get()) {
			if (float scale = transform.scale; scale != 1.0f) {
				if (instance.flags.any(Instance::Flags::kRelativeScale)) {
					scale *= refScale;
				}
				createdRef->SetScale(scale);
				createdRef->AddChange(RE::TESObjectREFR::ChangeFlags::kScale);				
			}

			SetProperties(createdRef.get(), hash);

			Manager::GetSingleton()->SerializeObject(hash, createdRef, data.IsTemporary());

			logger::info("\tSpawning new object {:X} with hash {:X}.", createdRef->GetFormID(), hash);
		}
	}
}

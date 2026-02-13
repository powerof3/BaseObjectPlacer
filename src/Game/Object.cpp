#include "Game/Object.h"

#include "Config/Object.h"
#include "Manager.h"

Game::ObjectFilter::Input::Input(RE::TESObjectREFR* a_ref, RE::TESObjectCELL* a_cell) :
	ref(a_ref),
	baseObj(a_ref ? a_ref->GetBaseObject() : nullptr),
	cell(a_cell),
	fileName(a_ref ? a_ref->GetFile(0)->fileName : ""sv)
{}

Game::ObjectFilter::ObjectFilter(const Config::SharedData& a_data)
{
	whiteList.reserve(a_data.whiteList.size());
	for (const auto& str : a_data.whiteList) {
		if (const auto formID = RE::GetRawFormID(str, true)) {
			whiteList.emplace_back(formID.id);
		} else {
			whiteList.emplace_back(str);
		}
	}

	blackList.reserve(a_data.blackList.size());
	for (const auto& str : a_data.blackList) {
		if (const auto formID = RE::GetRawFormID(str, true)) {
			blackList.emplace_back(formID.id);
		} else {
			blackList.emplace_back(str);
		}
	}
}

bool Game::ObjectFilter::IsAllowed(RE::TESObjectREFR* a_ref, RE::TESObjectCELL* a_cell) const
{
	if (blackList.empty() && whiteList.empty()) {
		return true;
	}

	const Input input(a_ref, a_cell);

	if (CheckList(blackList, input)) {
		return false;
	}
	if (!whiteList.empty() && !CheckList(whiteList, input)) {
		return false;
	}
	return true;
}

bool Game::ObjectFilter::CheckList(const std::vector<FilterEntry>& a_list, const Input& input)
{
	for (const auto& entry : a_list) {
		bool matched = false;
		std::visit(overload{
					   [&](RE::FormID a_id) {
						   matched = MatchFormID(a_id, input);
					   },
					   [&](const std::string& a_str) {
						   matched = MatchString(a_str, input);
					   } },
			entry);
		if (matched) {
			return true;
		}
	}
	return false;
}

bool Game::ObjectFilter::MatchFormID(RE::FormID a_id, const Input& input)
{
	if (input.ref && input.ref->GetFormID() == a_id || input.baseObj && input.baseObj->GetFormID() == a_id || input.cell->GetFormID() == a_id) {
		return true;
	}

	if (const auto list = RE::TESForm::LookupByID<RE::BGSListForm>(a_id)) {
		bool hasForm = false;
		list->ForEachForm([&](auto* form) {
			if (form == input.ref || form == input.baseObj) {
				hasForm = true;
				return RE::BSContainer::ForEachResult::kStop;
			}
			return RE::BSContainer::ForEachResult::kContinue;
		});
		if (hasForm) {
			return true;
		}
	}

	return false;
}

bool Game::ObjectFilter::MatchString(const std::string& a_str, const Input& input)
{
	return a_str == input.fileName || a_str == input.cell->GetFormEditorID();
}

Game::SharedData::SharedData(const Config::SharedData& a_data) :
	extraData(a_data.extraData),
	conditions(ConditionParser::BuildCondition(a_data.conditions)),
	motionType(a_data.motionType),
	filter(a_data),
	flags(a_data.flags),
	chance(a_data.chance)
{
	scripts.reserve(a_data.scripts.size());
	for (const auto& script : a_data.scripts) {
		scripts.emplace_back(script);
	}
}

bool Game::SharedData::PassesFilters(RE::TESObjectREFR* a_ref, RE::TESObjectCELL* a_cell) const
{
	if (conditions) {
		if (auto conditionRef = a_ref ? a_ref : RE::PlayerCharacter::GetSingleton(); !conditions->IsTrue(conditionRef, conditionRef)) {
			return false;
		}
	}

	if (!filter.IsAllowed(a_ref, a_cell)) {
		return false;
	}

	return true;
}

bool Game::SharedData::IsTemporary() const
{
	return flags.any(Data::ReferenceFlags::kTemporary) || conditions != nullptr;
}

void Game::SharedData::SetProperties(RE::TESObjectREFR* a_ref, std::size_t a_hash) const
{
	SetPropertiesFlags(a_ref);
	AttachScripts(a_ref);
	extraData.AddExtraData(a_ref, a_hash);
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

Game::Object::Instance::Instance(const RE::BSTransformRange& a_range, const RE::BSTransform& a_transform, Flags a_flags, std::size_t a_hash) :
	transform(a_transform),
	transformRange(a_range),
	flags(a_flags),
	hash(a_hash)
{}

Game::Object::Instance::Instance(const RE::BSTransformRange& a_range, Flags a_flags, std::size_t a_hash) :
	transform(a_range, a_hash),
	transformRange(a_range),
	flags(a_flags),
	hash(a_hash)
{}

Game::Object::Instance::Flags Game::Object::Instance::GetInstanceFlags(const Config::SharedData& a_data, const RE::BSTransformRange& a_range, const Config::ObjectArray& a_array)
{
	REX::EnumSet flags(Flags::kNone);
	if (a_data.flags.any(Data::ReferenceFlags::kSequentialObjects)) {
		flags.set(Flags::kSequentialObjects);
	}
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

void Game::Object::SpawnObject(RE::TESDataHandler* a_dataHandler, RE::TESObjectREFR* a_ref, RE::TESObjectCELL* a_cell, RE::TESWorldSpace* a_worldSpace, bool) const
{
	if (!data.PassesFilters(a_ref, a_cell)) {
		return;
	}

	const RefInfo refInfo(a_ref);

	const auto baseSize = static_cast<std::uint32_t>(bases.size());

	for (auto&& [idx, instance] : std::views::enumerate(instances)) {
		auto baseIndex = static_cast<std::uint32_t>(idx % baseSize);

		auto hash = instance.hash;
		if (a_ref) {
			hash = hash::combine(instance.hash, refInfo.id);
		}

		if (instance.flags.none(Instance::Flags::kSequentialObjects)) {
			baseIndex = clib_util::RNG(hash).generate<std::uint32_t>(0, baseSize - 1);
		}

		hash = hash::combine(hash, baseIndex);
		Manager::GetSingleton()->AddConfigObject(hash, this);

		if (auto id = Manager::GetSingleton()->GetSavedObject(hash); id != 0) {
			logger::info("\t[{:X}]{:X} already exists, skipping spawn.", hash, id);
			continue;
		}

		if (RE::GetNumReferenceHandles() >= 1000000 || RE::GetMaxFormIDReached()) {  // max id reached
			logger::info("\t[{:X}] Maximum number of handles/FF formIDs reached. Skipping.", hash);
			continue;
		}

		const auto baseObject = bases[baseIndex];
		auto       transform = instance.GetWorldTransform(refInfo.position, refInfo.angle, hash);
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
					scale *= refInfo.scale;
				}
				createdRef->SetScale(scale);
				createdRef->AddChange(RE::TESObjectREFR::ChangeFlags::kScale);
			}

			data.SetProperties(createdRef.get(), hash);

			Manager::GetSingleton()->SerializeObject(hash, createdRef, data.IsTemporary());

			logger::info("\tSpawning object with hash {:X} using base index {}.", hash, baseIndex);
		}
	}
}

void Game::Format::SpawnInCell(RE::TESObjectCELL* a_cell)
{
	if (const auto it = cells.find(a_cell->GetFormEditorID()); it != cells.end()) {
		const auto dataHandler = RE::TESDataHandler::GetSingleton();
		for (const auto& object : it->second) {
			object.SpawnObject(dataHandler, nullptr, a_cell, a_cell->worldSpace, false);
		}
	}
}

void Game::Format::SpawnAtReference(RE::TESObjectREFR* a_ref)
{
	if (objects.empty() && objectTypes.empty()) {
		return;
	}

	const auto dataHandler = RE::TESDataHandler::GetSingleton();
	const auto cell = a_ref->GetParentCell();
	const auto worldSpace = a_ref->GetWorldspace();

	const auto                       base = a_ref->GetBaseObject();
	const std::vector<Game::Object>* objectsToSpawn = nullptr;

	const auto find_objects = [&](auto&& key) -> std::vector<Game::Object>* {
		if (auto it = objects.find(key); it != objects.end()) {
			return &it->second;
		}
		return nullptr;
	};

	if (!objectsToSpawn) {
		objectsToSpawn = find_objects(a_ref->GetFormID());
	}

	if (!objectsToSpawn && base) {
		objectsToSpawn = find_objects(base->GetFormID());
		if (!objectsToSpawn) {
			objectsToSpawn = find_objects(clib_util::editorID::get_editorID(base));
		}
	}

	if (objectsToSpawn) {
		for (const auto& object : *objectsToSpawn) {
			object.SpawnObject(dataHandler, a_ref, cell, worldSpace, true);
		}
	}

	if (const auto it = objectTypes.find(base->GetFormType()); it != objectTypes.end()) {
		for (const auto& object : it->second) {
			object.SpawnObject(dataHandler, a_ref, cell, worldSpace, true);
		}
	}
}

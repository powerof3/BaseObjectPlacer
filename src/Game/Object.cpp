#include "Game/Object.h"

#include "Config/Object.h"
#include "Manager.h"

Game::ObjectFilter::Input::Input(RE::TESObjectREFR* a_ref, RE::TESObjectCELL* a_cell) :
	ref(a_ref),
	baseObj(a_ref ? a_ref->GetBaseObject() : nullptr),
	cell(a_cell),
	fileName(a_ref ? a_ref->GetFile(0)->fileName : ""sv)
{}

Game::ObjectFilter::ObjectFilter(const Config::FilterData& a_filter)
{
	whiteList.reserve(a_filter.whiteList.size());
	for (const auto& str : a_filter.whiteList) {
		if (const auto formID = RE::GetRawFormID(str, true)) {
			whiteList.emplace_back(formID.id);
		} else {
			whiteList.emplace_back(str);
		}
	}

	blackList.reserve(a_filter.blackList.size());
	for (const auto& str : a_filter.blackList) {
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
	if (input.ref && input.ref->GetFormID() == a_id || input.baseObj && input.baseObj->GetFormID() == a_id || input.cell && input.cell->GetFormID() == a_id) {
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
	return a_str == input.fileName || input.cell && a_str == input.cell->GetFormEditorID();
}

Game::FilterData::FilterData(const Config::FilterData& a_filter) :
	conditions(ConditionParser::BuildCondition(a_filter.conditions)),
	filter(a_filter)
{}

bool Game::FilterData::PassesFilters(RE::TESObjectREFR* a_ref, RE::TESObjectCELL* a_cell) const
{
	if (!filter.IsAllowed(a_ref, a_cell)) {
		return false;
	}

	if (conditions) {
		if (auto conditionRef = a_ref ? a_ref : RE::PlayerCharacter::GetSingleton(); !conditions->IsTrue(conditionRef, conditionRef)) {
			return false;
		}
	}

	return true;
}

Game::ObjectData::ObjectData(const Config::ObjectData& a_data) :
	extraData(a_data.extraData),
	motionType(a_data.motionType),
	flags(a_data.flags)
{
	scripts.reserve(a_data.scripts.size());
	for (const auto& script : a_data.scripts) {
		scripts.emplace_back(script);
	}
}

void Game::ObjectData::Merge(const Game::ObjectData& a_parent)
{
	if (flags.none(Data::ReferenceFlags::kInheritFromParent)) {
		return;
	}

	flags = a_parent.flags;

	extraData.Merge(a_parent.extraData);

	if (motionType.type == RE::hkpMotion::MotionType::kInvalid) {
		motionType = a_parent.motionType;
	}

	if (scripts.empty() && !a_parent.scripts.empty()) {
		scripts.reserve(a_parent.scripts.size());
		for (const auto& script : a_parent.scripts) {
			scripts.emplace_back(script);
		}
	}
}

bool Game::ObjectData::PreventClipping(const RE::TESBoundObject* a_base) const
{
	return flags.any(ReferenceFlags::kPreventClipping) || (motionType.type == RE::hkpMotion::MotionType::kKeyframed || motionType.type == RE::hkpMotion::MotionType::kFixed || !RE::CanBeMoved(a_base));
}

void Game::ObjectData::SetProperties(RE::TESObjectREFR* a_ref, std::size_t a_hash) const
{
	SetPropertiesFlags(a_ref);
	AttachScripts(a_ref);
	extraData.AddExtraData(a_ref, a_hash);
}

void Game::ObjectData::SetPropertiesHavok([[maybe_unused]] RE::TESObjectREFR* a_ref, RE::NiAVObject* a_root) const
{
	if (a_root && motionType.type != RE::hkpMotion::MotionType::kInvalid) {
		a_root->SetMotionType(motionType.type, true, false, motionType.allowActivate);
	}
}

void Game::ObjectData::SetPropertiesFlags(RE::TESObjectREFR* a_ref) const
{
	bool changedFlags = false;
	if (flags.any(ReferenceFlags::kNoAIAcquire)) {
		a_ref->formFlags |= RE::TESObjectREFR::RecordFlags::kNoAIAcquire;
		changedFlags = true;
	}
	if (flags.any(ReferenceFlags::kInitiallyDisabled)) {
		a_ref->formFlags |= RE::TESObjectREFR::RecordFlags::kInitiallyDisabled;
		changedFlags = true;
	}
	if (flags.any(ReferenceFlags::kHiddenFromLocalMap)) {
		a_ref->SetOnLocalMap(false);
		changedFlags = true;
	}
	if (flags.any(ReferenceFlags::kInaccessible)) {
		a_ref->formFlags |= RE::TESObjectREFR::RecordFlags::kInaccessible;
		changedFlags = true;
	}
	if (flags.any(ReferenceFlags::kIgnoredBySandbox)) {
		a_ref->formFlags |= RE::TESObjectACTI::RecordFlags::kIgnoresObjectInteraction;
		changedFlags = true;
	}
	if (flags.any(ReferenceFlags::kOpenByDefault)) {
		RE::BGSOpenCloseForm::SetOpenState(a_ref, true, true);
		a_ref->AddChange(RE::TESObjectREFR::ChangeFlags::kGameOnlyExtra);
	}
	if (flags.any(ReferenceFlags::kIsFullLOD)) {
		a_ref->formFlags |= RE::TESObjectREFR::RecordFlags::kIsFullLOD;
		changedFlags = true;
	}
	if (changedFlags) {
		a_ref->AddChange(RE::TESForm::ChangeFlags::kFlags);
	}
}

void Game::ObjectData::AttachScripts(RE::TESObjectREFR* a_ref) const
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

Game::Object::Params::RefParams::RefParams(RE::TESObjectREFR* a_ref) :
	id(a_ref->GetFormID()),
	bb(a_ref),
	scale(a_ref->GetScale())
{}

Game::Object::Params::Params(RE::TESObjectREFR* a_ref) :
	refParams(a_ref),
	ref(a_ref),
	cell(a_ref->GetParentCell()),
	worldspace(a_ref->GetWorldspace())
{}

Game::Object::Params::Params(RE::TESObjectCELL* a_cell) :
	ref(nullptr),
	cell(a_cell),
	worldspace(a_cell->worldSpace)
{}

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

REX::EnumSet<Game::Object::Instance::Flags> Game::Object::Instance::GetInstanceFlags(const Game::ObjectData& a_data, const RE::BSTransformRange& a_range, const Config::ObjectArray& a_array)
{
	REX::EnumSet flags(Flags::kNone);
	if (a_data.flags.any(ReferenceFlags::kSequentialObjects)) {
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
	return flags;
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

Game::Object::Object(const Config::ObjectData& a_data) :
	data(a_data)
{}

void Game::Object::SpawnObject(RE::TESDataHandler* a_dataHandler, const Params& a_params, std::uint32_t& a_numHandles, bool a_isTemporary, std::size_t a_parentHash, const std::vector<Object>& a_childObjects) const
{
	auto [refParams, ref, cell, worldSpace] = a_params;

	const auto baseSize = static_cast<std::uint32_t>(bases.size());
	const auto refAngle = ref->GetAngle();

	for (auto&& [idx, instance] : std::views::enumerate(instances)) {
		auto baseIndex = static_cast<std::uint32_t>(idx % baseSize);

		auto hash = instance.hash;
		if (ref) {
			if (a_parentHash != 0 || ref->IsDynamicForm()) {
				hash = hash::combine(instance.hash, a_parentHash);
			} else {
				hash = hash::combine(instance.hash, refParams.id);
			}
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

		if (a_numHandles >= 1000000 || RE::GetMaxFormIDReached()) {  // max id reached
			logger::info("\t[{:X}] Maximum number of handles/FF formIDs reached. Skipping.", hash);
			continue;
		}

		a_numHandles++;

		const auto baseObject = bases[baseIndex];
		auto       transform = instance.GetWorldTransform(refParams.bb.pos, refAngle, hash);
		if (ref && data.PreventClipping(baseObject)) {
			RE::NiPoint3 baseObjectExtents{
				static_cast<float>(baseObject->boundData.boundMax.x - baseObject->boundData.boundMin.x),
				static_cast<float>(baseObject->boundData.boundMax.y - baseObject->boundData.boundMin.y),
				static_cast<float>(baseObject->boundData.boundMax.z - baseObject->boundData.boundMin.z)
			};
			transform.ValidatePosition(cell, ref, refParams.bb, baseObjectExtents);
		}

		auto createdRefHandle = a_dataHandler->CreateReferenceAtLocation(
			baseObject,
			transform.translate,
			transform.rotate,
			cell,
			worldSpace,
			nullptr,
			nullptr,
			{},
			false,
			true);

		if (auto createdRef = createdRefHandle.get()) {
			if (float scale = transform.scale; scale != 1.0f) {
				if (instance.flags.any(Instance::Flags::kRelativeScale)) {
					scale *= refParams.scale;
				}
				createdRef->SetScale(scale);
				createdRef->AddChange(RE::TESObjectREFR::ChangeFlags::kScale);
			}

			data.SetProperties(createdRef.get(), hash);

			Manager::GetSingleton()->SerializeObject(hash, createdRef, a_isTemporary);

			logger::info("\tSpawning object {:X} with hash {:X}.", createdRef->GetFormID(), hash);

			if (!a_childObjects.empty()) {
				const Params createdParams(createdRef.get());
				for (const auto& childObject : a_childObjects) {
					childObject.SpawnObject(a_dataHandler, createdParams, a_numHandles, a_isTemporary, hash);
				}
			}
		}
	}
}

Game::RootObject::RootObject(const Config::FilterData& a_filter, const Config::ObjectData& a_data) :
	Object(a_data),
	filter(a_filter)
{}

bool Game::RootObject::IsTemporary() const
{
	return data.flags.any(ReferenceFlags::kTemporary) || filter.conditions != nullptr;
}

void Game::RootObject::SpawnObject(RE::TESDataHandler* a_dataHandler, const Params& a_params, std::uint32_t& a_numHandles) const
{
	if (!filter.PassesFilters(a_params.ref, a_params.cell)) {
		return;
	}

	Object::SpawnObject(a_dataHandler, a_params, a_numHandles, IsTemporary(), 0, childObjects);
}

std::vector<Game::RootObject>* Game::Format::FindObjects(const RE::TESObjectREFR* a_ref, const RE::TESBoundObject* a_base)
{
	if (const auto it = objects.find(a_ref->GetFormID()); it != objects.end()) {
		return &it->second;
	}
	if (a_base) {
		if (const auto it = objects.find(a_base->GetFormID()); it != objects.end()) {
			return &it->second;
		}
		if (const auto it = objects.find(clib_util::editorID::get_editorID(a_base)); it != objects.end()) {
			return &it->second;
		}
	}
	return nullptr;
}

std::vector<Game::RootObject>* Game::Format::FindObjects(const RE::TESBoundObject* a_base)
{
	if (!a_base) {
		return nullptr;
	}

	if (const auto it = objectTypes.find(a_base->GetFormType()); it != objectTypes.end()) {
		return &it->second;
	}

	return nullptr;
}

void Game::Format::SpawnInCell(RE::TESObjectCELL* a_cell)
{
	if (const auto it = cells.find(a_cell->GetFormEditorID()); it != cells.end()) {
		const auto           dataHandler = RE::TESDataHandler::GetSingleton();
		const Object::Params objectParams(a_cell);
		auto                 numHandles = RE::GetNumReferenceHandles();
		for (const auto& object : it->second) {
			object.SpawnObject(dataHandler, objectParams, numHandles);
		}
	}
}

void Game::Format::SpawnAtReference(RE::TESObjectREFR* a_ref)
{
	if (objects.empty() && objectTypes.empty()) {
		return;
	}

	const auto base = a_ref->GetBaseObject();

	auto objectsToSpawn = FindObjects(a_ref, base);
	auto objectsToSpawnFromTypes = FindObjects(base);

	if (objectsToSpawn || objectsToSpawnFromTypes) {
		const auto           dataHandler = RE::TESDataHandler::GetSingleton();
		const Object::Params params(a_ref);
		auto                 numHandles = RE::GetNumReferenceHandles();
		if (objectsToSpawn) {
			for (const auto& object : *objectsToSpawn) {
				object.SpawnObject(dataHandler, params, numHandles);
			}
		}
		if (objectsToSpawnFromTypes) {
			for (const auto& object : *objectsToSpawnFromTypes) {
				object.SpawnObject(dataHandler, params, numHandles);
			}
		}
	}
}

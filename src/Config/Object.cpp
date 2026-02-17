#include "Config/Object.h"

#include "Game/Object.h"
#include "Manager.h"

namespace Config
{
	bool FilterData::RollChance(std::size_t seed) const
	{
		if (chance < 1.0f) {
			auto roll = clib_util::RNG(seed).generate();
			if (roll > chance) {
				return false;
			}
		}
		return true;
	}

	void ObjectData::ReadReferenceFlags(const std::string& input)
	{
		if (!input.empty()) {
			const auto flagStrs = string::split(input, "|");
			for (const auto& flagStr : flagStrs) {
				switch (string::const_hash(flagStr)) {
				case "NoAIAcquire"_h:
					flags.set(ReferenceFlags::kNoAIAcquire);
					break;
				case "InitiallyDisabled"_h:
					flags.set(ReferenceFlags::kInitiallyDisabled);
					break;
				case "HiddenFromLocalMap"_h:
					flags.set(ReferenceFlags::kHiddenFromLocalMap);
					break;
				case "Inaccessible"_h:
					flags.set(ReferenceFlags::kInaccessible);
					break;
				case "OpenByDefault"_h:
					flags.set(ReferenceFlags::kOpenByDefault);
					break;
				case "IgnoredBySandbox"_h:
					flags.set(ReferenceFlags::kIgnoredBySandbox);
					break;
				case "IsFullLOD"_h:
					flags.set(ReferenceFlags::kIsFullLOD);
					break;
				case "Temporary"_h:
					flags.set(ReferenceFlags::kTemporary);
					break;
				case "SequentialObjects"_h:
					flags.set(ReferenceFlags::kSequentialObjects);
					break;
				case "PreventClipping"_h:
					flags.set(ReferenceFlags::kPreventClipping);
					break;
				case "InheritFromParent"_h:
					flags.set(ReferenceFlags::kInheritFromParent);
					break;
				default:
					break;
				}
			}
		}
	}

	std::string ObjectData::WriteReferenceFlags() const
	{
		static constexpr std::pair<ReferenceFlags, std::string_view> flagNames[] = {
			{ ReferenceFlags::kNoAIAcquire, "NoAIAcquire" },
			{ ReferenceFlags::kInitiallyDisabled, "InitiallyDisabled" },
			{ ReferenceFlags::kHiddenFromLocalMap, "HiddenFromLocalMap" },
			{ ReferenceFlags::kInaccessible, "Inaccessible" },
			{ ReferenceFlags::kOpenByDefault, "OpenByDefault" },
			{ ReferenceFlags::kIgnoredBySandbox, "IgnoredBySandbox" },
			{ ReferenceFlags::kIsFullLOD, "IsFullLOD" },
			{ ReferenceFlags::kTemporary, "Temporary" },
			{ ReferenceFlags::kSequentialObjects, "SequentialObjects" },
			{ ReferenceFlags::kPreventClipping, "PreventClipping" },
			{ ReferenceFlags::kInheritFromParent, "InheritFromParent" }
		};

		std::string result;

		for (const auto& [flag, name] : flagNames) {
			if (flags.any(flag)) {
				if (!result.empty()) {
					result += '|';
				}
				result += name;
			}
		}

		return result;
	}

	const std::vector<RE::TESBoundObject*>& PrefabObject::GetBases() const
	{
		return resolvedBases;
	}

	std::vector<RE::TESBoundObject*> PrefabObject::GetBasesOnDemand() const
	{
		std::vector<RE::TESBoundObject*> checkedBases;
		checkedBases.reserve(bases.size());
		for (const auto& base : bases) {
			if (const auto form = RE::GetForm(base)) {
				if (form->IsBoundObject()) {
					checkedBases.emplace_back(form->As<RE::TESBoundObject>());
				} else if (const auto list = form->As<RE::BGSListForm>()) {
					list->ForEachForm([&](auto* listForm) {
						if (listForm->IsBoundObject()) {
							checkedBases.emplace_back(listForm->As<RE::TESBoundObject>());
						}
						return RE::BSContainer::ForEachResult::kContinue;
					});
				}
			}
		}
		return checkedBases;
	}

	void PrefabObject::ResolveBasesOnLoad()
	{
		resolvedBases = GetBasesOnDemand();
	}

	void Object::GenerateHash()
	{
		baseHash = hash::combine(
			transforms,
			array,
			filter);
	}

	void Object::CreateGameObject(std::vector<Game::RootObject>& a_objectVec, const std::variant<RE::RawFormID, std::string_view>& a_attachID) const
	{
		using ObjectInstance = Game::Object::Instance;

		const Prefab*                    resolvedPrefab = nullptr;
		std::vector<RE::TESBoundObject*> resolvedBases;
		bool                             cachedPrefab = false;

		std::visit(overload{
					   [&](const Prefab& a_prefab) {
						   resolvedPrefab = &a_prefab;
						   resolvedBases = resolvedPrefab->GetBasesOnDemand();
					   },
					   [&](const std::string& str) {
						   resolvedPrefab = Manager::GetSingleton()->GetPrefab(str);
						   if (!resolvedPrefab) {
							   logger::info("Prefab {} not found, skipping object.", str);
						   }
						   resolvedBases = resolvedPrefab->GetBases();
						   cachedPrefab = true;
					   } },
			prefab);

		if (!resolvedPrefab || resolvedBases.empty()) {
			return;
		}

		logger::info("Processing object with prefab {}", resolvedPrefab->uuid);

		Game::RootObject  rootObject(filter, resolvedPrefab->data);
		const std::size_t rootHash = hash::combine(baseHash, a_attachID, *resolvedPrefab);

		for (auto&& [transformIdx, transformRange] : std::views::enumerate(transforms)) {
			auto flags = ObjectInstance::GetInstanceFlags(rootObject.data, transformRange, array);

			std::size_t objectHash = hash::combine(rootHash, transformIdx);
			if (auto arrayTransforms = array.GetTransforms(transformRange, objectHash); arrayTransforms.empty()) {
				if (!filter.RollChance(objectHash)) {
					continue;
				}
				rootObject.instances.emplace_back(transformRange, flags.get(), objectHash);

			} else {
				for (auto&& [arrayIdx, arrayTransform] : std::views::enumerate(arrayTransforms)) {
					objectHash = hash::combine(rootHash, transformIdx, arrayIdx, array.seed);
					if (!filter.RollChance(objectHash)) {
						continue;
					}
					rootObject.instances.emplace_back(transformRange, arrayTransform, flags.get(), objectHash);
				}
			}
		}

		if (rootObject.instances.empty()) {
			if (transforms.empty()) {
				logger::warn("\t[FAIL] No instances generated (zero transforms)");
			} else {
				logger::warn("\t[FAIL] No instances generated.");
			}
			return;
		}

		logger::info("\tGenerated {} instances with {} bases.", rootObject.instances.size(), resolvedBases.size());

		if (!resolvedPrefab->children.empty()) {
			std::vector<Game::Object> childObjects;
			childObjects.reserve(resolvedPrefab->children.size());
			for (const auto& child : resolvedPrefab->children) {
				auto childBases = cachedPrefab ? child.GetBases() : child.GetBasesOnDemand();
				if (childBases.empty()) {
					continue;
				}
				const std::size_t childHash = hash::combine(rootHash, child);

				Game::Object childObject(child.data);
				childObject.data.Merge(rootObject.data);
				childObject.bases = std::move(childBases);

				auto childFlags = ObjectInstance::GetInstanceFlags(childObject.data, child.transform, array);
				childFlags.set(ObjectInstance::Flags::kRelativeTranslate);
				childFlags.set(ObjectInstance::Flags::kRelativeRotate);

				childObject.instances.emplace_back(child.transform, childFlags.get(), childHash);
				childObjects.push_back(std::move(childObject));
			}
			logger::info("\tGenerated {} child objects.", childObjects.size());
			rootObject.childObjects = std::move(childObjects);
		}

		rootObject.bases = std::move(resolvedBases);
		a_objectVec.push_back(std::move(rootObject));
	}
}

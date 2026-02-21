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
		static constexpr auto map = clib_util::constexpr_map{ flagArray };

		if (!input.empty()) {
			const auto flagStrs = string::split(input, "|");
			for (const auto& flagStr : flagStrs) {
				flags.set(map.at(flagStr));
			}
		}
	}

	std::string ObjectData::WriteReferenceFlags() const
	{
		std::string result;

		for (const auto& [name, flag] : flagArray) {
			if (flags.any(flag)) {
				if (!result.empty()) {
					result += '|';
				}
				result += name;
			}
		}

		return result;
	}

	const Prefab* Prefab::GetPrefabFromVariant(const PrefabOrUUID& a_variant)
	{
		const Prefab* prefab = nullptr;

		std::visit(overload{
					   [&](const Prefab& a_prefab) {
						   prefab = &a_prefab;
					   },
					   [&](const std::string& str) {
						   prefab = Manager::GetSingleton()->GetPrefab(str);
						   if (!prefab) {
							   logger::info("Prefab {} not found, skipping object.", str);
						   }
					   } },
			a_variant);

		return prefab;
	}

	Base::WeightedObjects<RE::TESBoundObject*> Prefab::GetBaseObjects() const
	{
		Base::WeightedObjects<RE::TESBoundObject*> resolvedBases;

		std::visit(overload{
					   [&](const Base::WeightedObjects<std::string>& a_bases) {
						   resolvedBases = std::move(Base::WeightedObjects<RE::TESBoundObject*>(a_bases));
					   },
					   [&](const Base::WeightedObjects<RE::TESBoundObject*>& a_bases) {
						   resolvedBases = a_bases;
					   } },
			bases);

		return resolvedBases;
	}

	void Prefab::Resolve()
	{
		if (const auto basePtr = std::get_if<Base::WeightedObjects<std::string>>(&bases)) {
			bases = Base::WeightedObjects<RE::TESBoundObject*>(*basePtr);
		}

		for (auto& child : children) {
			if (const auto prefabPtr = std::get_if<Prefab>(&child)) {
				prefabPtr->Resolve();
			}
		}
	}

	void Object::GenerateHash()
	{
		baseHash = hash::combine(
			transforms,
			array,
			filter);
	}

	std::vector<Game::Object> Object::BuildChildObjects(const std::vector<PrefabOrUUID>& a_children, std::size_t a_parentRootHash, const Game::ObjectData& a_parentData)
	{
		using ObjectInstance = Game::Object::Instance;

		std::vector<Game::Object> result;
		result.reserve(a_children.size());

		for (const auto& childVariant : a_children) {
			const auto childPrefab = Prefab::GetPrefabFromVariant(childVariant);
			if (!childPrefab) {
				continue;
			}

			const auto childBases = childPrefab->GetBaseObjects();
			if (childBases.empty()) {
				continue;
			}

			const std::size_t childHash = hash::combine(a_parentRootHash, *childPrefab);

			Game::Object childObject(childPrefab->data);
			childObject.data.Merge(a_parentData);
			childObject.bases = childBases;

			auto childFlags = ObjectInstance::GetInstanceFlags(childObject.data, childPrefab->transform, childPrefab->array);

			if (auto arrayTransforms = childPrefab->array.GetTransforms(childPrefab->transform, childHash); !arrayTransforms.empty()) {
				for (auto&& [arrayIdx, arrayTransform] : std::views::enumerate(arrayTransforms)) {
					const auto instanceHash = hash::combine(childHash, arrayIdx, childPrefab->array.seed);
					childObject.instances.emplace_back(childPrefab->transform, arrayTransform, childFlags.get(), instanceHash);
				}
			} else {
				childObject.instances.emplace_back(childPrefab->transform, childFlags.get(), childHash);
			}

			if (!childPrefab->children.empty()) {
				childObject.childObjects = BuildChildObjects(childPrefab->children, childHash, childObject.data);
			}

			result.push_back(std::move(childObject));
		}

		return result;
	}

	void Object::CreateGameObject(std::vector<Game::RootObject>& a_objectVec, const std::variant<RE::RawFormID, std::string_view>& a_attachID) const
	{
		using ObjectInstance = Game::Object::Instance;

		const Prefab* resolvedPrefab = Prefab::GetPrefabFromVariant(prefab);
		if (!resolvedPrefab) {
			return;
		}

		const auto resolvedBases = resolvedPrefab->GetBaseObjects();
		if (resolvedBases.empty()) {
			return;
		}

		logger::info("Processing object with prefab {}", resolvedPrefab->uuid);

		Game::RootObject  rootObject(filter, resolvedPrefab->data);
		const std::size_t rootHash = hash::combine(baseHash, a_attachID, *resolvedPrefab);

		for (auto&& [transformIdx, transformRange] : std::views::enumerate(transforms)) {
			auto flags = ObjectInstance::GetInstanceFlags(rootObject.data, transformRange, array);

			std::size_t objectHash = hash::combine(rootHash, transformIdx);
			if (auto arrayTransforms = array.GetTransforms(transformRange, objectHash); !arrayTransforms.empty()) {
				for (auto&& [arrayIdx, arrayTransform] : std::views::enumerate(arrayTransforms)) {
					objectHash = hash::combine(rootHash, transformIdx, arrayIdx, array.seed);
					if (!filter.RollChance(objectHash)) {
						continue;
					}
					rootObject.instances.emplace_back(transformRange, arrayTransform, flags.get(), objectHash);
				}
			} else {
				if (!filter.RollChance(objectHash)) {
					continue;
				}
				rootObject.instances.emplace_back(transformRange, flags.get(), objectHash);
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
			rootObject.childObjects = BuildChildObjects(resolvedPrefab->children, rootHash, rootObject.data);
		}

		rootObject.bases = resolvedBases;
		a_objectVec.push_back(std::move(rootObject));
	}
}

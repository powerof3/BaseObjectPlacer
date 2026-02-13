#include "Config/Object.h"

#include "Game/Object.h"

namespace Config
{
	bool SharedData::RollChance(std::size_t seed) const
	{
		if (chance < 1.0f) {
			auto roll = clib_util::RNG(seed).generate();
			if (roll > chance) {
				return false;
			}
		}
		return true;
	}

	void Object::GenerateHash()
	{
		baseHash = hash::combine(
			uuid,
			bases,
			transforms,
			array,
			data);
	}

	void Object::CreateGameObject(std::vector<Game::Object>& a_objectVec, const std::variant<RE::RawFormID, std::string_view>& a_attachID) const
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

		if (checkedBases.empty()) {
			return;
		}

		Game::Object      gameObject(data);
		const std::size_t rootHash = hash::combine(baseHash, a_attachID);

		for (auto&& [transformIdx, transformRange] : std::views::enumerate(transforms)) {
			auto flags = Game::Object::Instance::GetInstanceFlags(data, transformRange, array);

			std::size_t objectHash = hash::combine(rootHash, transformIdx);
			if (auto arrayTransforms = array.GetTransforms(transformRange, objectHash); arrayTransforms.empty()) {
				if (!data.RollChance(objectHash)) {
					continue;
				}
				gameObject.instances.emplace_back(transformRange, flags, objectHash);

			} else {
				for (auto&& [arrayIdx, arrayTransform] : std::views::enumerate(arrayTransforms)) {
					objectHash = hash::combine(rootHash, transformIdx, arrayIdx, array.seed);
					if (!data.RollChance(objectHash)) {
						continue;
					}
					gameObject.instances.emplace_back(transformRange, arrayTransform, flags, objectHash);
				}
			}
		}

		if (gameObject.instances.empty()) {
			return;
		}

		gameObject.bases = std::move(checkedBases);
		a_objectVec.push_back(std::move(gameObject));
	}
}

#include "Config\ObjectData.h"

#include "GameData.h"
#include "Manager.h"

namespace Config
{
	void ObjectData::GenerateHash()
	{
		baseHash = hash::combine(
			uuid,
			bases,
			transforms,
			array,
			data);
	}

	void ObjectData::CreateGameSourceData(std::vector<Game::SourceData>& a_srcDataVec, RE::FormID a_attachFormID, std::string_view a_attachForm) const
	{
		std::vector<RE::FormID> checkedBases;
		checkedBases.reserve(bases.size());
		for (const auto& base : bases) {
			auto formID = RE::GetFormID(base);
			if (formID == 0) {
				continue;
			}
			checkedBases.emplace_back(formID);
		}
		if (checkedBases.empty()) {
			return;
		}

		const auto roll_chance = [&](std::size_t seed) {
			if (data.chance < 1.0f) {
				auto roll = clib_util::RNG(seed).generate();
				if (roll > data.chance) {
					return false;
				}
			}
			return true;
		};

		auto sharedData = std::make_shared<Game::SharedData>(data);

		for (auto [transformIdx, transform] : std::views::enumerate(transforms)) {
			bool shouldBeOrdered = checkedBases.size() == transforms.size();

			if (auto arrayTransforms = array.GetTransforms(transform); arrayTransforms.empty()) {
				Game::SourceData newData(sharedData, a_attachFormID);

				std::size_t baseIdx = 0;
				if (shouldBeOrdered) {
					baseIdx = transformIdx;
				} else {
					if (checkedBases.size() > 1) {
						std::size_t rngSeed = hash::combine(transformIdx);
						baseIdx = clib_util::RNG(rngSeed).generate<std::size_t>(0, checkedBases.size() - 1);
					}
				}

				newData.baseID = checkedBases[baseIdx];
				newData.hash = a_attachFormID != 0 ?
				                   GenerateHash(a_attachFormID, baseIdx, transformIdx) :
				                   GenerateHash(a_attachForm, baseIdx, transformIdx);

				if (!roll_chance(newData.hash)) {
					continue;
				}

				newData.transform = transform.value(newData.hash);

				a_srcDataVec.push_back(std::move(newData));
			} else {
				shouldBeOrdered = checkedBases.size() == arrayTransforms.size();

				for (auto [arrayIdx, arrayTransform] : std::views::enumerate(arrayTransforms)) {
					Game::SourceData newData(sharedData, a_attachFormID);

					std::size_t uniqueIdx = hash::combine(transformIdx, arrayIdx);
					std::size_t baseIdx = 0;
					if (shouldBeOrdered) {
						baseIdx = arrayIdx;
					} else {
						if (checkedBases.size() > 1) {
							std::size_t rngSeed = hash::combine(array.seed, uniqueIdx);
							baseIdx = clib_util::RNG(rngSeed).generate<std::size_t>(0, checkedBases.size() - 1);
						}
					}

					newData.baseID = checkedBases[baseIdx];
					newData.hash = a_attachFormID != 0 ?
					                   GenerateHash(a_attachFormID, baseIdx, uniqueIdx) :
					                   GenerateHash(a_attachForm, baseIdx, uniqueIdx);

					if (!roll_chance(newData.hash)) {
						continue;
					}

					newData.transform = arrayTransform;

					a_srcDataVec.push_back(std::move(newData));
				}
			}
		}
	}
}

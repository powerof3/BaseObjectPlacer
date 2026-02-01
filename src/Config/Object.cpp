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

	void Object::CreateGameObject(std::vector<Game::Object>& a_objectVec, const std::variant<RE::FormID, std::string_view>& a_attachID) const
	{
		std::vector<RE::FormID> checkedBases;
		checkedBases.reserve(bases.size());

		for (const auto& base : bases) {
			if (auto formID = RE::GetFormID(base); formID != 0) {
				checkedBases.emplace_back(formID);
			}
		}

		if (checkedBases.empty()) {
			return;
		}

		Game::Object gameObject(data);

		const auto        checkedBaseSize = checkedBases.size();
		const std::size_t rootHash = hash::combine(baseHash, a_attachID);

		bool shouldBeOrdered = checkedBaseSize == transforms.size();

		const auto get_base_idx = [&](std::size_t a_objectHash, bool a_shouldBeOrdered, std::size_t a_idx, bool a_mixArraySeed) {
			if (a_shouldBeOrdered) {
				return static_cast<std::uint32_t>(a_idx);
			}
			if (checkedBaseSize > 1) {
				std::size_t rngSeed = a_mixArraySeed ? hash::combine(a_objectHash, array.seed) : a_objectHash;
				return static_cast<std::uint32_t>(clib_util::RNG(rngSeed).generate<std::size_t>(0, checkedBaseSize - 1));
			}
			return static_cast<std::uint32_t>(0);
		};

		for (const auto [transformIdx, transform] : std::views::enumerate(transforms)) {
			if (auto arrayTransforms = array.GetTransforms(transform); arrayTransforms.empty()) {
				std::size_t   objectHash = hash::combine(rootHash, transformIdx);
				std::uint32_t baseIdx = get_base_idx(objectHash, shouldBeOrdered, transformIdx, false);

				auto instanceHash = hash::combine(objectHash, baseIdx);
				if (!data.RollChance(instanceHash)) {
					continue;
				}
				gameObject.instances.emplace_back(baseIdx, transform, instanceHash);

			} else {
				shouldBeOrdered = checkedBaseSize == arrayTransforms.size();

				for (const auto [arrayIdx, arrayTransform] : std::views::enumerate(arrayTransforms)) {
					std::size_t   objectHash = hash::combine(rootHash, transformIdx, arrayIdx);
					std::uint32_t baseIdx = get_base_idx(objectHash, shouldBeOrdered, arrayIdx, true);

					auto instanceHash = hash::combine(objectHash, baseIdx);
					if (!data.RollChance(instanceHash)) {
						continue;
					}
					gameObject.instances.emplace_back(baseIdx, arrayTransform, instanceHash);
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

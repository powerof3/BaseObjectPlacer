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

		const auto  checkedBaseSize = checkedBases.size();
		const auto  transformsSize = transforms.size();
		std::size_t rootHash = hash::combine(baseHash, a_attachID);

		bool shouldBeOrdered = checkedBaseSize == transformsSize;

		for (const auto [transformIdx, transform] : std::views::enumerate(transforms)) {
			if (auto arrayTransforms = array.GetTransforms(transform); arrayTransforms.empty()) {
				std::size_t   objectHash = hash::combine(rootHash, transformIdx);
				std::uint32_t baseIdx = 0;
				if (shouldBeOrdered) {
					baseIdx = static_cast<std::uint32_t>(transformIdx);
				} else {
					if (checkedBaseSize > 1) {
						baseIdx = static_cast<std::uint32_t>(clib_util::RNG(objectHash).generate<std::size_t>(0, checkedBaseSize - 1));
					}
				}

				auto instanceHash = hash::combine(objectHash, baseIdx);

				if (!data.RollChance(instanceHash)) {
					continue;
				}

				Game::Object::Instance instance{};
				instance.baseIndex = baseIdx;
				instance.hash = instanceHash;
				instance.transform = transform.value(instanceHash);
				gameObject.instances.push_back(std::move(instance));

			} else {
				shouldBeOrdered = checkedBaseSize == arrayTransforms.size();

				for (const auto [arrayIdx, arrayTransform] : std::views::enumerate(arrayTransforms)) {
					std::size_t   objectHash = hash::combine(rootHash, transformIdx, arrayIdx);
					std::uint32_t baseIdx = 0;
					if (shouldBeOrdered) {
						baseIdx = static_cast<std::uint32_t>(arrayIdx);
					} else {
						if (checkedBaseSize > 1) {
							std::size_t rngSeed = hash::combine(objectHash, array.seed);
							baseIdx = static_cast<std::uint32_t>(clib_util::RNG(rngSeed).generate<std::size_t>(0, checkedBaseSize - 1));
						}
					}

					auto instanceHash = hash::combine(objectHash, baseIdx);

					if (!data.RollChance(instanceHash)) {
						continue;
					}

					Game::Object::Instance instance{};
					instance.baseIndex = baseIdx;
					instance.hash = instanceHash;
					instance.transform = arrayTransform;
					gameObject.instances.push_back(std::move(instance));
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

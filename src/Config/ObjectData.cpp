#include "Config\ObjectData.h"

#include "GameData.h"
#include "Manager.h"

namespace Config
{
	std::size_t ObjectArray::Grid::GetCount() const
	{
		const auto xCount = xArray.count > 0 ? xArray.count : 1;
		const auto yCount = yArray.count > 0 ? yArray.count : 1;
		const auto zCount = zArray.count > 0 ? zArray.count : 1;

		return xCount * yCount * zCount;
	}

	void ObjectArray::Grid::GetTransforms(const RE::BSTransform& a_pivot, std::vector<RE::BSTransform>& a_transforms) const
	{
		if (xArray.count == 0 && yArray.count == 0 && zArray.count == 0) {
			return;
		}

		const auto xCount = xArray.count > 0 ? xArray.count : 1;
		const auto yCount = yArray.count > 0 ? yArray.count : 1;
		const auto zCount = zArray.count > 0 ? zArray.count : 1;

		const float xStart = xArray.count > 0 ? -(xArray.offset * (xCount - 1) / 2.0f) : 0.0f;
		const float yStart = yArray.count > 0 ? -(yArray.offset * (yCount - 1) / 2.0f) : 0.0f;
		const float zStart = zArray.count > 0 ? -(zArray.offset * (zCount - 1) / 2.0f) : 0.0f;

		for (std::uint32_t x = 0; x < xCount; x++) {
			float xPos = xStart + x * xArray.offset;

			for (std::uint32_t y = 0; y < yCount; y++) {
				float yPos = yStart + y * zArray.offset;

				for (std::uint32_t z = 0; z < zCount; z++) {
					float zPos = zStart + z * zArray.offset;

					RE::BSTransform newTransform = a_pivot;
					newTransform.translate.x += xPos;
					newTransform.translate.y += yPos;
					newTransform.translate.z += zPos;
					a_transforms.emplace_back(newTransform);
				}
			}
		}
	}

	void ObjectArray::Radial::GetTransforms(const RE::BSTransform& a_pivot, std::vector<RE::BSTransform>& a_transforms) const
	{
		if (count == 0) {
			return;
		}

		for (std::uint32_t i = 0; i < count; ++i) {
			float theta = i * angleStep;

			RE::NiPoint3 r_offset{
				radius * std::cos(theta),
				radius * std::sin(theta),
				0.0f,
			};

			RE::BSTransform newTransform = a_pivot;
			newTransform.translate += r_offset;
			a_transforms.emplace_back(newTransform);
		}
	}

	std::size_t ObjectArray::Word::GetCount() const
	{
		return word.size();
	}

	void ObjectArray::Word::GetTransforms(const RE::BSTransform& a_pivot, std::vector<RE::BSTransform>& a_transforms) const
	{
		if (word.empty() || size == 0) {
			return;
		}

		RE::BSTransform transform = a_pivot;
		std::uint32_t   newLine = 1;

		for (char letter : word) {
			std::string str(1, static_cast<char>(std::toupper(letter)));
			if (str == "\n") {
				transform.translate = a_pivot.translate + (RE::NiPoint3(0, spacing, 0) * size) * (float)newLine;
				newLine++;
			} else {
				if (str == "\t") {
					transform.translate += (RE::NiPoint3(-spacing, 0, 0) * size);
				} else if (auto it = charMap.find(str); it != charMap.end()) {
					for (auto& point : it->second) {
						RE::BSTransform newTransform = transform;
						RE::NiPoint3    newPoint = RE::NiPoint3(point * size);
						newPoint *= RE::NiPoint3(-1, 1, 1);
						newTransform.translate += newPoint;
						a_transforms.emplace_back(newTransform);
					}
				}
				transform.translate += (RE::NiPoint3(-spacing, 0, 0) * size);
			}
		}
	}

	void ObjectArray::Word::InitCharMap()
	{
		std::filesystem::path dir{ R"(Data\BaseObjectPlacer\WordPlacement)" };

		std::error_code ec;
		if (!std::filesystem::exists(dir, ec)) {
			return;
		}

		std::string buffer;

		for (auto i = std::filesystem::recursive_directory_iterator(dir); i != std::filesystem::recursive_directory_iterator(); ++i) {
			if (i->is_directory() || i->path().extension() != ".json"sv) {
				continue;
			}
			auto err = glz::read_file_json(charMap, i->path().string(), buffer);
			if (err) {
				logger::error("\tchar error:{}", glz::format_error(err, buffer));
			}
		}
	}

	RE::NiPoint3 ObjectArray::GetRotationStep(const RE::BSTransformRange& a_pivotRange, std::size_t a_count) const
	{
		auto rotMin = a_pivotRange.rotate.min();
		auto rotMax = a_pivotRange.rotate.max();

		const auto get_wrapped_range = [](float min, float max) {
			return (max >= min) ? (max - min) : (RE::NI_TWO_PI - min + max);
		};

		auto rotX = get_wrapped_range(rotMin.x, rotMax.x);
		auto rotY = get_wrapped_range(rotMin.y, rotMax.y);
		auto rotZ = get_wrapped_range(rotMin.z, rotMax.z);

		return RE::NiPoint3(rotX, rotY, rotZ) / static_cast<float>(a_count);
	}

	std::vector<RE::BSTransform> ObjectArray::GetTransforms(const RE::BSTransformRange& a_pivot) const
	{
		std::vector<RE::BSTransform> arrayTransforms;

		auto arrayCount = grid.GetCount() + radial.GetCount();
		if (arrayCount <= 1 && words.GetCount() == 0) {
			return arrayTransforms;
		}

		RE::BSTransform pivot = a_pivot.value(seed);

		arrayTransforms.reserve(arrayCount);

		grid.GetTransforms(pivot, arrayTransforms);
		radial.GetTransforms(pivot, arrayTransforms);
		words.GetTransforms(pivot, arrayTransforms);

		if (flags.any(Flags::kRandomizeRotation, Flags::kRandomizeScale, Flags::kIncrementRotation, Flags::kIncrementScale)) {
			bool randomizeRot = flags.any(Flags::kRandomizeRotation);
			bool randomizeScale = flags.any(Flags::kRandomizeScale);

			bool incrementRot = flags.any(Flags::kIncrementRotation);
			bool incrementScale = flags.any(Flags::kIncrementScale);

			auto count = arrayTransforms.size() - 1;
			auto rotStep = GetRotationStep(a_pivot, count);
			auto scaleStep = a_pivot.scale.max ? ((a_pivot.scale.max.value() - a_pivot.scale.min) / static_cast<float>(count)) : 0.0f;

			for (std::size_t idx = 0; idx < arrayTransforms.size(); ++idx) {
				auto& transform = arrayTransforms[idx];
				if (randomizeRot || randomizeScale) {
					std::size_t rngSeed = seed;
					boost::hash_combine(rngSeed, idx);
					if (randomizeRot) {
						transform.rotate = a_pivot.rotate.value(rngSeed);
					}
					if (randomizeScale) {
						transform.scale = a_pivot.scale.value(rngSeed);
					}
				} else {
					if (incrementRot) {
						auto rot = a_pivot.rotate.min() + (rotStep * static_cast<float>(idx));
						RE::WrapAngle(rot);
						transform.rotate = rot;
					}
					if (incrementScale) {
						transform.scale = a_pivot.scale.min + (scaleStep * static_cast<float>(idx));
					}
				}
			}
		}

		if (rotate != RE::NiPoint3::Zero()) {
			for (auto& transform : arrayTransforms) {
				transform.translate = RE::ApplyRotation(transform.translate, pivot.translate, rotate);
			}
		}

		return arrayTransforms;
	}

	void ObjectData::CreateGameSourceData(std::vector<Game::SourceData>& a_srcDataVec, RE::FormID a_attachFormID, const std::string& a_attachForm) const
	{
		for (auto [transformIdx, transform] : std::views::enumerate(transforms)) {
			Game::SourceData newData(*this, a_attachFormID);

			if (auto arrayTransforms = array.GetTransforms(transform); arrayTransforms.empty()) {
				for (const auto [baseIdx, base] : std::views::enumerate(bases)) {
					newData.baseID = RE::GetFormID(base);
					if (newData.baseID == 0) {
						continue;
					}
					newData.hash = a_attachFormID != 0 ? GenerateHash(a_attachFormID, baseIdx, transformIdx) : GenerateHash(a_attachForm, baseIdx, transformIdx);
					newData.transform = transform.value(newData.hash);

					a_srcDataVec.push_back(newData);
				}
			} else {
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
					continue;
				}

				bool shouldBeOrdered = checkedBases.size() == arrayTransforms.size();

				for (auto [arrayIdx, arrayTransform] : std::views::enumerate(arrayTransforms)) {
					std::size_t uniqueIdx = transformIdx + arrayIdx;
					newData.transform = arrayTransform;
					if (shouldBeOrdered) {
						for (const auto [baseIdx, base] : std::views::enumerate(bases)) {
							newData.baseID = checkedBases[baseIdx];
							newData.hash = a_attachFormID != 0 ? GenerateHash(a_attachFormID, baseIdx, uniqueIdx) : GenerateHash(a_attachForm, baseIdx, uniqueIdx);

							a_srcDataVec.push_back(newData);
						}
					} else {
						std::size_t baseIdx{ 0 };
						if (checkedBases.size() > 1) {
							std::size_t rngSeed;
							boost::hash_combine(rngSeed, array.seed);
							boost::hash_combine(rngSeed, uniqueIdx);
							baseIdx = clib_util::RNG(rngSeed).generate<std::size_t>(0, checkedBases.size() - 1);
						}
						newData.baseID = checkedBases[baseIdx];
						newData.hash = a_attachFormID != 0 ? GenerateHash(a_attachFormID, baseIdx, uniqueIdx) : GenerateHash(a_attachForm, baseIdx, uniqueIdx);

						a_srcDataVec.push_back(newData);
					}
				}
			}
		}
	}
}

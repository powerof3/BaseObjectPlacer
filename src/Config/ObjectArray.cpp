#include "Config/ObjectArray.h"

namespace Config
{
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
				float yPos = yStart + y * yArray.offset;

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

	void ObjectArray::Word::GetTransforms(const RE::BSTransform& a_pivot, std::vector<RE::BSTransform>& a_transforms) const
	{
		if (word.empty() || size == 0.0f) {
			return;
		}

		RE::BSTransform transform = a_pivot;
		std::uint32_t   newLine = 1;

		const auto verticalSpacing = RE::NiPoint3(0, spacing, 0) * size;
		const auto horizontalSpacing = RE::NiPoint3(-spacing, 0, 0) * size;

		for (char letter : word) {
			if (letter == '\n') {
				transform.translate += a_pivot.translate + verticalSpacing * static_cast<float>(newLine);
				newLine++;
				continue;
			}
			if (letter == '\t') {
				transform.translate += horizontalSpacing;
			} else {
				if (auto it = charMap.find(static_cast<char>(std::toupper(letter))); it != charMap.end()) {
					for (auto& point : it->second) {
						RE::BSTransform newTransform = transform;
						RE::NiPoint3    newPoint = { point * size };
						newPoint *= RE::NiPoint3(-1, 1, 1);
						newTransform.translate += newPoint;
						a_transforms.emplace_back(newTransform);
					}
				}
			}
			transform.translate += horizontalSpacing;
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

	RE::NiPoint3 ObjectArray::GetRotationStep(const RE::BSTransformRange& a_pivotRange, std::size_t a_count)
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

	std::vector<RE::BSTransform> ObjectArray::GetTransforms(const RE::BSTransformRange& a_pivotRange, std::size_t a_hash) const
	{
		std::vector<RE::BSTransform> arrayTransforms{};
		RE::BSTransform              pivot{};

		const std::size_t rngSeed = hash::combine(a_hash, seed);

		std::visit(overload{
					   [](std::monostate) {
					   },
					   [&](const auto& generator) {
						   pivot = RE::BSTransform(a_pivotRange, rngSeed);
						   generator.GetTransforms(pivot, arrayTransforms);
					   } },
			array);

		if (arrayTransforms.empty()) {
			return arrayTransforms;
		}

		const bool randomizeRot = flags.any(Flags::kRandomizeRotation);
		const bool randomizeScale = flags.any(Flags::kRandomizeScale);
		const bool incrementRot = flags.any(Flags::kIncrementRotation);
		const bool incrementScale = flags.any(Flags::kIncrementScale);

		if (randomizeRot || randomizeScale || incrementRot || incrementScale) {
			RE::NiPoint3 rotStep{};
			float        scaleStep{};
			
			if (incrementRot || incrementScale) {
				const auto count = arrayTransforms.size() - 1;
				if (incrementRot) {
					rotStep = GetRotationStep(a_pivotRange, count);
				}
				if (incrementScale) {
					scaleStep = a_pivotRange.scale.max ? ((a_pivotRange.scale.max.value() - a_pivotRange.scale.min) / static_cast<float>(count)) : 0.0f;
				}
			}

			for (std::size_t idx = 0; idx < arrayTransforms.size(); ++idx) {
				auto&      transform = arrayTransforms[idx];
				const auto idxSeed = hash::combine(rngSeed, idx);

				if (randomizeRot) {
					transform.rotate = a_pivotRange.rotate.value(idxSeed);
				} else if (incrementRot) {
					auto rot = a_pivotRange.rotate.min() + (rotStep * static_cast<float>(idx));
					RE::WrapAngle(rot);
					transform.rotate = rot;
				}

				if (randomizeScale) {
					transform.scale = a_pivotRange.scale.value(idxSeed);
				} else if (incrementScale) {
					transform.scale = a_pivotRange.scale.min + (scaleStep * static_cast<float>(idx));
				}
			}
		}

		if (rotate != RE::NiPoint3::Zero()) {
			RE::NiMatrix3 rotationMatrix;
			rotationMatrix.SetEulerAnglesXYZ(rotate.x, rotate.y, rotate.z);

			for (auto& transform : arrayTransforms) {
				transform.translate = RE::ApplyRotation(transform.translate, pivot.translate, rotationMatrix);
			}
		}

		return arrayTransforms;
	}
}

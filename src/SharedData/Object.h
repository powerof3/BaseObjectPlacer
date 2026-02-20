#pragma once

namespace Base
{
	enum class ReferenceFlags : uint32_t
	{
		kNoAIAcquire = 1 << 0,         // AI will not attempt to pick up containers or objects marked with this flag.
		kInitiallyDisabled = 1 << 1,   // The reference starts disabled ("not there") and must be enabled through script or game events.
		kHiddenFromLocalMap = 1 << 2,  // The reference will not be displayed on the local map.
		kInaccessible = 1 << 3,        // Only for Doors; makes them inoperable and shows "Inaccessible".
		kOpenByDefault = 1 << 4,       // For Doors/Containers; begins in the open state.
		kIgnoredBySandbox = 1 << 5,    // Prevents Sandboxing NPCs from using this reference (useful for furniture/idle markers).
		kIsFullLOD = 1 << 6,           // Reference will not fade from a distance.
		//user
		kTemporary = 1 << 14,
		kSequentialObjects = 1 << 15,
		kPreventClipping = 1 << 16,
		kInheritFlags = 1 << 17,
		kInheritExtraData = 1 << 18,
		kInheritScripts = 1 << 19,
	};

	struct MotionType
	{
		RE::hkpMotion::MotionType type{ RE::hkpMotion::MotionType::kInvalid };
		bool                      allowActivate{ false };

	private:
		GENERATE_HASH(MotionType, a_val.type, a_val.allowActivate)
	};

	template <class T>
	struct WeightedObjects
	{
		WeightedObjects() = default;
		WeightedObjects(const WeightedObjects& other) :
			objects(other.objects),
			weights(other.weights)
		{}

		explicit WeightedObjects(const WeightedObjects<std::string>& other)
			requires std::is_same_v<RE::TESBoundObject*, T>
		{
			objects.reserve(other.size());
			for (const auto& [base, weight] : std::ranges::zip_view(other.objects, other.weights)) {
				if (const auto form = RE::GetForm(base)) {
					if (auto obj = form->As<RE::TESBoundObject>()) {
						emplace_back(obj, weight);
					} else if (const auto list = form->As<RE::BGSListForm>()) {
						list->ForEachForm([&](auto* listForm) {
							if (auto listObj = listForm->As<RE::TESBoundObject>()) {
								emplace_back(listObj, weight);
							}
							return RE::BSContainer::ForEachResult::kContinue;
						});
					}
				}
			}
		}
		
		bool        empty() const { return objects.empty(); }
		std::size_t size() const { return objects.size(); }

		void reserve(std::size_t a_count)
		{
			objects.reserve(a_count);
			weights.reserve(a_count);
		}

		void emplace_back(const std::string& a_base, float a_weight)
				requires std::is_same_v<std::string, T>
		{
			objects.emplace_back(a_base);
			weights.emplace_back(a_weight);
		}

		void emplace_back(RE::TESBoundObject* a_base, float a_weight)
			requires std::is_same_v<RE::TESBoundObject*, T>
		{
			objects.emplace_back(a_base);
			weights.emplace_back(a_weight);
		}

		bool are_weights_equal() const
		{
			return std::ranges::all_of(weights, [&](const auto& w) { return w == weights.front(); });
		}

		// members
		std::vector<T>     objects{};
		std::vector<float> weights{};

	private:
		GENERATE_HASH(WeightedObjects, a_val.objects, a_val.weights);
	};

	using WeightedObjectVariant = std::variant<WeightedObjects<std::string>, WeightedObjects<RE::TESBoundObject*>>;
}

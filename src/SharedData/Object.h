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
		kInheritFromParent = 1 << 17,
	};

	struct MotionType
	{
		RE::hkpMotion::MotionType type{ RE::hkpMotion::MotionType::kInvalid };
		bool                      allowActivate{ false };

	private:
		GENERATE_HASH(MotionType, a_val.type, a_val.allowActivate)
	};

	struct WeightedObjects
	{
		bool        empty() const { return objects.empty(); }
		std::size_t size() const { return objects.size(); }

		void reserve(std::size_t a_count)
		{
			objects.reserve(a_count);
			weights.reserve(a_count);
		}

		void emplace_back(RE::TESBoundObject* a_base, float a_weight)
		{
			objects.emplace_back(a_base);
			weights.emplace_back(a_weight);
		}

		bool are_weights_equal() const
		{
			return std::ranges::all_of(weights, [&](const auto& w) { return w == weights.front(); });
		}

		std::vector<RE::TESBoundObject*> objects{};
		std::vector<float>               weights{};
	};
}

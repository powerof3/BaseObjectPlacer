#include "Hooks.h"

namespace Hooks
{
	/*struct TESObjectREFR__MarkedAsPickedUp
	{
		static void thunk(RE::TESObjectREFR* a_this)
		{
			logger::info("{:X} is picked up", a_this->GetFormID());

			func(a_this);
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static void Install()
		{
			REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(19378, 19805) };
			stl::hook_function_prologue<TESObjectREFR__MarkedAsPickedUp, 6>(target.address());
		}
	};

	// because TESObjectLoadedEvent only fires for forms with FormRetainsID flag
	struct TESObjectREFR__Set3DSimple
	{
		static void thunk(RE::TESObjectREFR* a_this, RE::NiAVObject* a_root)
		{
			func(a_this, a_root);
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static void Install()
		{
			REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(19303, 19730) };
			stl::hook_function_prologue<TESObjectREFR__Set3DSimple, 5>(target.address());
		}
	};*/

	void Install()
	{
		logger::info("{:*^50}", "HOOKS");

		CheckSaveGame<RE::TESObjectREFR>::Install();
		CheckSaveGame<RE::Hazard>::Install();
		CheckSaveGame<RE::ArrowProjectile>::Install();

		FinishLoadGame<RE::TESObjectREFR>::Install();
		FinishLoadGame<RE::Hazard>::Install();
		FinishLoadGame<RE::ArrowProjectile>::Install();

		InitHavok<RE::TESObjectREFR>::Install();
		InitHavok<RE::Hazard>::Install();
		InitHavok<RE::ArrowProjectile>::Install();

		//TESObjectREFR__Set3DSimple::Install();
		//TESObjectREFR__MarkedAsPickedUp::Install();
	}
}

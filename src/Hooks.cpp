#include "Hooks.h"

#include "Manager.h"

namespace Hooks
{
	struct TESObjectREFR__LoadGame
	{
		static void thunk(RE::TESObjectREFR* a_this, RE::BGSLoadFormBuffer* a_buf)
		{
			func(a_this, a_buf);

			Manager::GetSingleton()->LoadSerializedObject(a_this);
		}
		static inline REL::Relocation<decltype(thunk)> func;
		static constexpr std::size_t                   idx{ 0xF };

		static void Install()
		{
			stl::write_vfunc<RE::TESObjectREFR, TESObjectREFR__LoadGame>();
		}
	};

	struct TESObjectREFR__FinishLoadGame
	{
		static void thunk(RE::TESObjectREFR* a_this, RE::BGSLoadFormBuffer* a_buf)
		{
			func(a_this, a_buf);

			Manager::GetSingleton()->FinishLoadSerializedObject(a_this);
		}
		static inline REL::Relocation<decltype(thunk)> func;
		static constexpr std::size_t                   idx{ 0x11 };

		static void Install()
		{
			stl::write_vfunc<RE::TESObjectREFR, TESObjectREFR__FinishLoadGame>();
		}
	};

	struct TESObjectREFR__InitHavok
	{
		static void thunk(RE::TESObjectREFR* a_this)
		{
			func(a_this);

			if (auto root = a_this->Get3D()) {
				Manager::GetSingleton()->UpdateSerializedObjectHavok(a_this);
			}
		}
		static inline REL::Relocation<decltype(thunk)> func;
		static constexpr std::size_t                   idx{ 0x66 };

		static void Install()
		{
			stl::write_vfunc<RE::TESObjectREFR, TESObjectREFR__InitHavok>();
		}
	};

	struct TESObjectREFR__MarkedAsPickedUp
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
	/*struct TESObjectREFR__Set3DSimple
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
		TESObjectREFR__LoadGame::Install();
		TESObjectREFR__FinishLoadGame::Install();
		TESObjectREFR__InitHavok::Install();
		//TESObjectREFR__Set3DSimple::Install();
		//TESObjectREFR__MarkedAsPickedUp::Install();
	}
}

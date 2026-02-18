#pragma once

#include "Manager.h"

namespace Hooks
{
	template <class T>
	struct CheckSaveGame
	{
		static bool thunk(T* a_this, RE::BGSSaveFormBuffer* a_buf)
		{
			auto result = func(a_this, a_buf);
			if (result) {
				if (Manager::GetSingleton()->IsTempObject(a_this)) {
					return false;
				}
			}
			return result;
		}
		static inline REL::Relocation<decltype(thunk)> func;
		static constexpr std::size_t                   idx{ 0x0D };

		static void Install()
		{
			logger::info("Installing {}::CheckSaveGame hook", typeid(T).name());
			stl::write_vfunc<T, CheckSaveGame>();
		}
	};

	template <class T>
	struct FinishLoadGame
	{
		static void thunk(T* a_this, RE::BGSLoadFormBuffer* a_buf)
		{
			func(a_this, a_buf);

			Manager::GetSingleton()->FinishLoadSerializedObject(a_this);
		}
		static inline REL::Relocation<decltype(thunk)> func;
		static constexpr std::size_t                   idx{ 0x11 };

		static void Install()
		{
			logger::info("Installing {}::FinishLoadGame hook", typeid(T).name());
			stl::write_vfunc<T, FinishLoadGame>();
		}
	};

	template <class T>
	struct InitHavok
	{
		static void thunk(T* a_this)
		{
			func(a_this);

			if (auto root = a_this->Get3D()) {
				Manager::GetSingleton()->UpdateSerializedObjectHavok(a_this, root);
			}
		}
		static inline REL::Relocation<decltype(thunk)> func;
		static constexpr std::size_t                   idx{ 0x66 };

		static void Install()
		{
			logger::info("Installing {}::InitHavok hook", typeid(T).name());
			stl::write_vfunc<T, InitHavok>();
		}
	};

	void Install();
}

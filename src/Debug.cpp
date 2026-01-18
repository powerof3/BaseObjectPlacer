#include "Debug.h"

#include "Manager.h"

namespace Debug
{
	struct ReloadConfig
	{
		constexpr static auto OG_COMMAND = "ToggleSPUCulling"sv;

		constexpr static auto LONG_NAME = "ReloadBOP"sv;
		constexpr static auto SHORT_NAME = "ReloadBOP"sv;
		constexpr static auto HELP = "Reload Base Object Placer configs from disk\n"sv;

		static bool Execute(const RE::SCRIPT_PARAMETER*, RE::SCRIPT_FUNCTION::ScriptData*, RE::TESObjectREFR*, RE::TESObjectREFR*, RE::Script*, RE::ScriptLocals*, double&, std::uint32_t&)
		{
			Manager::GetSingleton()->ReloadConfigs();

			return true;
		}
	};

	void Install()
	{
		logger::info("{:*^50}", "DEBUG");
		ConsoleCommandHandler<ReloadConfig>::Install();
		logger::info("{:*^50}", "SAVES");
	}
}

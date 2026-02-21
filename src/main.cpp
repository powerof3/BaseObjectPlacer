#include "Debug.h"
#include "Hooks.h"
#include "Manager.h"

void MessageHandler(SKSE::MessagingInterface::Message* a_message)
{
	switch (a_message->type) {
	case SKSE::MessagingInterface::kPostLoad:
		{
			if (auto [success, errorFound] = Manager::GetSingleton()->ReadConfigs(); success) {
				Hooks::Install();
			}
		}
		break;
	case SKSE::MessagingInterface::kPostPostLoad:
		{
			logger::info("{:*^50}", "MERGES");
			MergeMapperPluginAPI::GetMergeMapperInterface001();
			if (g_mergeMapperInterface) {
				const auto version = g_mergeMapperInterface->GetBuildNumber();
				logger::info("Got MergeMapper interface buildnumber {}", version);
			} else {
				logger::info("MergeMapper not detected");
			}
		}
		break;
	case SKSE::MessagingInterface::kDataLoaded:
		{
			Manager::GetSingleton()->OnDataLoad();
			Debug::Install();
		}
		break;
	case SKSE::MessagingInterface::kSaveGame:
		{
			std::string_view savePath(static_cast<const char*>(a_message->data), a_message->dataLen);
			Manager::GetSingleton()->SaveFiles(savePath);
		}
		break;
	case SKSE::MessagingInterface::kPreLoadGame:
		{
			std::string_view savePath(static_cast<const char*>(a_message->data), a_message->dataLen);
			if (savePath.ends_with(".ess")) {
				savePath.remove_suffix(4);
			}
			Manager::GetSingleton()->LoadFiles(savePath);
		}
		break;
	case SKSE::MessagingInterface::kDeleteGame:
		{
			std::string_view savePath(static_cast<const char*>(a_message->data), a_message->dataLen);
			Manager::GetSingleton()->DeleteSavedFiles(savePath);
		}
		break;
	case SKSE::MessagingInterface::kNewGame:
		Manager::GetSingleton()->ClearSavedObjects();
		break;
	default:
		break;
	}
}

#ifdef SKYRIM_AE
extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = []() {
	SKSE::PluginVersionData v;
	v.PluginVersion(Version::MAJOR);
	v.PluginName("BaseObjectPlacer");
	v.AuthorName("powerofthree");
	v.UsesAddressLibrary();
	v.UsesUpdatedStructs();
	v.CompatibleVersions({ SKSE::RUNTIME_SSE_LATEST });

	return v;
}();
#else
extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
	a_info->infoVersion = SKSE::PluginInfo::kVersion;
	a_info->name = "BaseObjectPlacer";
	a_info->version = Version::MAJOR;

	if (a_skse->IsEditor()) {
		logger::critical("Loaded in editor, marking as incompatible"sv);
		return false;
	}

	const auto ver = a_skse->RuntimeVersion();
	if (ver <
#	ifdef SKYRIMVR
		SKSE::RUNTIME_VR_1_4_15
#	else
		SKSE::RUNTIME_SSE_1_5_39
#	endif
	) {
		logger::critical(FMT_STRING("Unsupported runtime version {}"), ver.string());
		return false;
	}

#	ifdef SKYRIMVR
	REL::IDDatabase::get().IsVRAddressLibraryAtLeastVersion("LightPlacer VR", "0.162.0", true);
#	endif

	return true;
}
#endif

void InitializeLog()
{
	auto path = logger::log_directory();
	if (!path) {
		stl::report_and_fail("Failed to find standard logging directory"sv);
	}

	*path /= fmt::format(FMT_STRING("{}.log"), Version::PROJECT);
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

	log->set_level(spdlog::level::info);
	log->flush_on(spdlog::level::info);

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("%v"s);

	logger::info(FMT_STRING("{} v{}"), Version::PROJECT, Version::NAME);
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	InitializeLog();

	SKSE::Init(a_skse, false);
	SKSE::AllocTrampoline(512);

	logger::info("Game version : {}", a_skse->RuntimeVersion().string());

	const auto messaging = SKSE::GetMessagingInterface();
	messaging->RegisterListener(MessageHandler);

	return true;
}

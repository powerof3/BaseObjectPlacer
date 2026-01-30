#include "Manager.h"

std::pair<bool, bool> Manager::ReadConfigs(bool a_reload)
{
	logger::info("{:*^50}", a_reload ? "RELOAD" : "CONFIG FILES");

	static std::filesystem::path dir{ R"(Data\BaseObjectPlacer)" };

	std::error_code ec;
	if (!std::filesystem::exists(dir, ec)) {
		logger::info("Data\\BaseObjectPlacer folder not found ({})", ec.message());
		return { false, true };
	}

	std::vector<std::filesystem::path> paths;

	for (auto i = std::filesystem::recursive_directory_iterator(dir); i != std::filesystem::recursive_directory_iterator(); ++i) {
		if (i->is_directory()) {
			if (i->path().filename() == "WordPlacement") {
				i.disable_recursion_pending();
			}
			continue;
		}
		if (i->path().extension() == ".json"sv) {
			paths.push_back(i->path());
		}
	}

	std::ranges::sort(paths);

	bool        has_error = false;
	std::string buffer;

	for (auto& path : paths) {
		logger::info("{} {}...", a_reload ? "Reloading" : "Reading", path.string());
		Config::Format tmpConfig;
		auto           err = glz::read_file_json(tmpConfig, path.string(), buffer);
		if (err) {
			has_error = true;
			logger::error("\terror:{}", glz::format_error(err, buffer));
		} else {
			configs.cells.merge(tmpConfig.cells);
			configs.objects.merge(tmpConfig.objects);
		}
	}

	if (!a_reload) {
		ConfigObjectArray::Word::InitCharMap();
	}

	return { !configs.empty(), has_error };
}

void Manager::ReloadConfigs()
{
	if (auto [success, errorFound] = ReadConfigs(true); errorFound) {
		RE::ConsoleLog::GetSingleton()->Print("\tError when parsing configs. See po3_BaseObjectPlacer.log for more information.\nReload skipped.");
		return;
	}

	game.clear();
	configObjects.clear();

	ProcessConfigs();
	ProcessConfigObjects();

	logger::info("{} objects to be placed", configObjects.size());
	RE::ConsoleLog::GetSingleton()->Print("\t%u objects to be placed", configObjects.size());

	SKSE::GetTaskInterface()->AddTask([this]() {
		ClearSavedObjects(true);
		PlaceInLoadedArea();
	});
}

void Manager::OnDataLoad()
{
	if (configs.empty()) {
		return;
	}

	logger::info("{:*^50}", "RESULTS");

	ProcessConfigs();
	ProcessConfigObjects();

	logger::info("{} objects to be placed", configObjects.size());

	if (!game.cells.empty()) {
		detail::add_event_sink<RE::TESCellFullyLoadedEvent>();
		logger::info("Registered for cell load event");
	}

	detail::add_event_sink<RE::TESCellAttachDetachEvent>();
	logger::info("Registered for cell attach event");

	detail::add_event_sink<RE::TESLoadGameEvent>();
	detail::add_event_sink<RE::TESFormDeleteEvent>();

	logger::info("{:*^50}", "FILE CLEANUP");
	CleanupSavedFiles();
}

void Manager::SpawnInCell(RE::TESObjectCELL* a_cell)
{
	if (auto it = game.cells.find(a_cell->GetFormEditorID()); it != game.cells.end()) {
		const auto dataHandler = RE::TESDataHandler::GetSingleton();
		for (const auto& object : it->second) {
			object.SpawnObject(dataHandler, nullptr, a_cell, a_cell->worldSpace);
		}
	}
}

void Manager::SpawnAtReference(RE::TESObjectREFR* a_ref)
{
	if (auto it = game.objects.find(a_ref->GetFormID()); it != game.objects.end()) {
		const auto dataHandler = RE::TESDataHandler::GetSingleton();
		const auto cell = a_ref->GetParentCell();
		const auto worldSpace = a_ref->GetWorldspace();
		for (const auto& object : it->second) {
			object.SpawnObject(dataHandler, a_ref, cell, worldSpace);
		}
	}
}

void Manager::ProcessConfigs()
{
	for (auto& [attachForm, objectVec] : configs.objects) {
		const auto attachFormID = RE::GetUncheckedFormID(attachForm);
		if (attachFormID == 0) {
			continue;
		}
		std::vector<Game::Object> vec;
		for (auto& object : objectVec) {
			object.GenerateHash();
			object.CreateGameObject(vec, attachFormID);
		}
		if (!vec.empty()) {
			auto& to = game.objects[attachFormID];
			to.insert(to.end(), std::make_move_iterator(vec.begin()),
				std::make_move_iterator(vec.end()));
		}
	}

	for (auto& [attachForm, objectVec] : configs.cells) {
		const auto                attachEDID = RE::GetEditorID(attachForm);
		std::vector<Game::Object> vec;
		for (auto& object : objectVec) {
			object.GenerateHash();
			object.CreateGameObject(vec, attachEDID);
		}
		if (!vec.empty()) {
			auto& to = game.cells[attachEDID];
			to.insert(to.end(), std::make_move_iterator(vec.begin()),
				std::make_move_iterator(vec.end()));
		}
	}

	configs.clear();
}

void Manager::ProcessConfigObjects()
{
	for (const auto& [id, objectVec] : game.cells) {
		for (const auto& object : objectVec) {
			for (auto& instance : object.instances) {
				configObjects.emplace(instance.hash, &object);
			}
		}
	}

	for (const auto& [id, objectVec] : game.objects) {
		for (const auto& object : objectVec) {
			for (auto& instance : object.instances) {
				configObjects.emplace(instance.hash, &object);
			}
		}
	}
}

void Manager::PlaceInLoadedArea()
{
	bool placeAtReferences = !game.objects.empty();
	bool placeInCells = !game.cells.empty();

	RE::TES::GetSingleton()->ForEachCell([this, placeAtReferences, placeInCells](auto* cell) {
		if (placeAtReferences) {
			cell->ForEachReference([this](auto* ref) {
				SpawnAtReference(ref);
				return RE::BSContainer::ForEachResult::kContinue;
			});
		}
		if (placeInCells) {
			SpawnInCell(cell);
		}
	});
}

std::optional<std::filesystem::path> Manager::GetSaveDirectory()
{
	if (!saveDirectory) {
		if (auto dir = logger::log_directory()) {
			saveDirectory = dir;

			saveDirectory->remove_filename();
			*saveDirectory /= "Saves";
			*saveDirectory /= "BaseObjectPlacer"sv;
			std::error_code ec;
			if (!std::filesystem::exists(*saveDirectory, ec)) {
				std::filesystem::create_directory(*saveDirectory, ec);
			}
		}
	}
	return saveDirectory;
}

std::optional<std::filesystem::path> Manager::GetFile(std::string_view a_save)
{
	auto jsonPath = GetSaveDirectory();
	if (!jsonPath) {
		return std::nullopt;
	}

	*jsonPath /= a_save;
	jsonPath->replace_extension(".json");

	return jsonPath;
}

void Manager::SaveFiles(std::string_view a_save)
{
	if (savedObjects.empty()) {
		return;
	}

	auto jsonPath = GetFile(a_save);
	if (!jsonPath) {
		return;
	}

	logger::info("Saving {}", jsonPath->filename().string());
	logger::info("\t{} saved objects", savedObjects.size());

	std::string buffer;
	auto        ec = glz::write_file_json(savedObjects, jsonPath->string(), buffer);

	if (ec) {
		logger::info("\tFailed to save file: (error: {})", glz::format_error(ec, buffer));
	}
}

void Manager::LoadFiles(std::string_view a_save)
{
	loadingSave = true;

	logger::info("Loading save {}", a_save);

	logger::info("\tDeleting {} temp objects", tempObjects.size());
	tempObjects.clear(true);

	const auto& jsonPath = GetFile(a_save);
	if (!jsonPath) {
		return;
	}

	savedObjects.clear();

	std::error_code err;
	if (std::filesystem::exists(*jsonPath, err)) {
		std::string buffer;
		auto        ec = glz::read_file_json<glz::opts{ .minified = true }>(savedObjects, jsonPath->string(), buffer);
		if (ec) {
			logger::info("\tFailed to read json (error: {})", glz::format_error(ec, buffer));
		}
	}

	logger::info("\t{} saved objects", savedObjects.size());

	// delete hashes not present
	if (!savedObjects.empty()) {
		erase_if(savedObjects.map,
			[this](const auto& entry) {
				return !configObjects.contains(entry.first);
			});
		savedObjects.rebuild_inverse_map();
	}
}

void Manager::DeleteSavedFiles(std::string_view a_save)
{
	auto jsonPath = GetFile(a_save);
	if (!jsonPath) {
		return;
	}

	std::filesystem::remove(*jsonPath);
}

void Manager::CleanupSavedFiles()
{
	constexpr auto get_game_save_directory = []() -> std::optional<std::filesystem::path> {
		if (auto path = logger::log_directory()) {
			path->remove_filename();  // remove "/SKSE"
			path->append("sLocalSavePath:General"_ini.value());
			return path;
		}
		return std::nullopt;
	};

	static auto gameSaveDir = get_game_save_directory();
	if (!gameSaveDir) {
		return;
	}

	std::uint32_t count = 0;

	if (auto bopSaveDir = GetSaveDirectory()) {
		std::error_code ec;

		for (const auto& entry : std::filesystem::directory_iterator(*bopSaveDir)) {
			if (entry.exists() && entry.path().extension() == ".json"sv) {
				auto saveFileName = entry.path().stem().string();
				auto savePath = std::format("{}{}.ess", gameSaveDir->string(), saveFileName);
				if (!std::filesystem::exists(savePath, ec)) {
					std::filesystem::remove(entry.path(), ec);
					count++;
				}
			}
		}
	}

	logger::info("Cleaned up {} orphaned saved files.", count);
}

bool Manager::IsTempObject(RE::TESObjectREFR* a_ref) const
{
	return GetSerializedObjectHash(a_ref) != 0 && tempObjects.find(a_ref->GetFormID()) != 0;
}

void Manager::SerializeHash(std::size_t hash, RE::ExtraCachedScale* a_scaleExtra)
{
	RE::SplitValue(hash, a_scaleExtra->scale3D, a_scaleExtra->refScale);
}

std::size_t Manager::DeserializeHash(RE::ExtraCachedScale* a_scaleExtra)
{
	return RE::RecombineValue(a_scaleExtra->scale3D, a_scaleExtra->refScale);
}

void Manager::ClearSavedObjects(bool a_deleteObjects)
{
	savedObjects.clear(a_deleteObjects);
	tempObjects.clear(a_deleteObjects);
}

void Manager::ClearTempObject(RE::TESObjectREFR* a_ref)
{
	if (tempObjects.erase(a_ref->GetFormID())) {
		RE::GarbageCollector::GetSingleton()->Add(a_ref, true);
	}
}

RE::ExtraCachedScale* Manager::GetSerializedObjectData(RE::TESObjectREFR* a_ref)
{
	if (a_ref && a_ref->IsDynamicForm()) {
		if (auto xData = a_ref->extraList.GetByType<RE::ExtraCachedScale>()) {  // actor specific xData
			return xData;
		}
	}

	return nullptr;
}

std::size_t Manager::GetSerializedObjectHash(RE::TESObjectREFR* a_ref)
{
	auto data = GetSerializedObjectData(a_ref);
	return data ? DeserializeHash(data) : 0;
}

void Manager::SerializeObject(std::size_t hash, const RE::TESObjectREFRPtr& a_ref, bool a_temporary)
{
	auto xData = RE::BSExtraData::Create<RE::ExtraCachedScale>();  // only actors have this xData
	SerializeHash(hash, xData);
	a_ref->extraList.Add(xData);
	a_ref->AddChange(RE::TESObjectREFR::ChangeFlags::kGameOnlyExtra);

	if (a_temporary) {
		tempObjects.emplace(hash, a_ref->GetFormID());
	} else {
		savedObjects.emplace(hash, a_ref->GetFormID());
	}
}

void Manager::FinishLoadSerializedObject(RE::TESObjectREFR* a_ref)
{
	if (auto hash = GetSerializedObjectHash(a_ref); hash != 0) {
		RE::FormID curID = a_ref->GetFormID();
		auto       savedID = GetSavedObject(hash);

		bool shouldDeleteRef = false;
		if (!savedID) {
			logger::error("\t\tObject with hash {} did not have a corresponding config entry. Deleting saved object. [FormID: {:X}]", hash, a_ref->GetFormID());
			shouldDeleteRef = true;
		} else if (savedID != curID) {
			logger::error("\t\tObject {:X} - saved ID and current ID mismatch. Deleting saved object. [Expected: ({:X}), Found: ({:X})]",
				a_ref->GetFormID(), curID, savedID);
			shouldDeleteRef = true;
		}

		if (shouldDeleteRef) {
			RE::GarbageCollector::GetSingleton()->Add(a_ref, true);
		} else {
			if (auto object = GetConfigObject(hash)) {
				object->data.SetPropertiesHavok(a_ref, a_ref->Get3D());
			}
		}
	}
}

void Manager::UpdateSerializedObjectHavok(RE::TESObjectREFR* a_ref)
{
	VisitSerializedObject(a_ref, [a_ref](const auto* object, const auto) {
		object->data.SetPropertiesHavok(a_ref, a_ref->Get3D());
	});
}

RE::FormID Manager::GetSavedObject(std::size_t a_hash) const
{
	if (auto id = savedObjects.find(a_hash); id != 0) {
		return id;
	}
	return tempObjects.find(a_hash);
}

const Game::Object* Manager::GetConfigObject(std::size_t a_hash)
{
	auto it = configObjects.find(a_hash);
	return it != configObjects.end() ? it->second : nullptr;
}

RE::BSEventNotifyControl Manager::ProcessEvent(const RE::TESCellFullyLoadedEvent* a_event, RE::BSTEventSource<RE::TESCellFullyLoadedEvent>*)
{
	if (!a_event || !a_event->cell || loadingSave) {
		return RE::BSEventNotifyControl::kContinue;
	}

	SpawnInCell(a_event->cell);

	return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl Manager::ProcessEvent(const RE::TESCellAttachDetachEvent* a_event, RE::BSTEventSource<RE::TESCellAttachDetachEvent>*)
{
	if (!a_event || loadingSave) {
		return RE::BSEventNotifyControl::kContinue;
	}

	auto& ref = a_event->reference;
	if (!ref) {
		return RE::BSEventNotifyControl::kContinue;
	}

	if (a_event->attached && !ref->IsDynamicForm()) {
		SpawnAtReference(ref.get());
	} else if (!a_event->attached && ref->IsDynamicForm()) {
		ClearTempObject(ref.get());
	}

	return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl Manager::ProcessEvent(const RE::TESLoadGameEvent* a_event, RE::BSTEventSource<RE::TESLoadGameEvent>*)
{
	if (!a_event || !loadingSave) {
		return RE::BSEventNotifyControl::kContinue;
	}

	logger::info("Finished loading save game");

	PlaceInLoadedArea();

	loadingSave = false;

	return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl Manager::ProcessEvent(const RE::TESFormDeleteEvent* a_event, RE::BSTEventSource<RE::TESFormDeleteEvent>*)
{
	if (a_event && a_event->formID != 0) {
		savedObjects.erase(a_event->formID);
		tempObjects.erase(a_event->formID);
	}

	return RE::BSEventNotifyControl::kContinue;
}

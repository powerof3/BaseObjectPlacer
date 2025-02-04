#include "Manager.h"

std::pair<bool, bool> Manager::ReadConfigs(bool a_reload)
{
	logger::info("{:*^50}", a_reload ? "RELOAD" : "CONFIG FILES");

	std::filesystem::path dir{ R"(Data\BaseObjectPlacer)" };

	std::error_code ec;
	if (!std::filesystem::exists(dir, ec)) {
		logger::info("Data\\BaseObjectPlacer folder not found ({})", ec.message());
		return { false, true };
	}

	if (a_reload) {
		configs.clear();
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

	bool has_error = false;

	for (auto& path : paths) {
		logger::info("{} {}...", a_reload ? "Reloading" : "Reading", path.string());
		std::string    buffer;
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
	auto [success, errorFound] = ReadConfigs(true);

	if (errorFound) {
		RE::ConsoleLog::GetSingleton()->Print("\tError when parsing configs. See po3_BaseObjectPlacer.log for more information.\nReload skipped.", configObjects.size());
		return;
	}

	game.clear();
	configObjects.clear();
	uuidHashes.clear();

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
	if (!game.objects.empty()) {
		detail::add_event_sink<RE::TESCellAttachDetachEvent>();
		logger::info("Registered for cell attach event");
	}
	detail::add_event_sink<RE::TESLoadGameEvent>();
	detail::add_event_sink<RE::TESFormDeleteEvent>();

	logger::info("{:*^50}", "SAVES");
}

void Manager::ProcessConfigs()
{
	for (const auto& [attachForm, sourceDataVec] : configs.objects) {
		const auto attachFormID = RE::GetFormID(attachForm);
		if (attachFormID == 0) {
			continue;
		}
		std::vector<Game::SourceData> vec;
		for (const auto& sourceData : sourceDataVec) {
			sourceData.CreateGameSourceData(vec, attachFormID, attachForm);
			for (auto& data : vec) {
				uuidHashes[sourceData.uuid].insert(data.hash);
			}
		}
		if (!vec.empty()) {
			game.objects[attachFormID].append_range(vec);
		}
	}

	for (const auto& [attachForm, sourceDataVec] : configs.cells) {
		const auto                    attachEDID = RE::GetEditorID(attachForm);
		std::vector<Game::SourceData> vec;
		for (const auto& sourceData : sourceDataVec) {
			sourceData.CreateGameSourceData(vec, 0, attachEDID);
			for (auto& data : vec) {
				uuidHashes[sourceData.uuid].insert(data.hash);
			}
		}
		if (!vec.empty()) {
			game.cells[attachEDID].append_range(vec);
		}
	}
}

void Manager::ProcessConfigObjects()
{
	for (const auto& [id, srcDataVec] : game.cells) {
		for (const auto& srcData : srcDataVec) {
			configObjects.emplace(srcData.hash, &srcData);
		}
	}

	for (const auto& [id, srcDataVec] : game.objects) {
		for (const auto& srcData : srcDataVec) {
			configObjects.emplace(srcData.hash, &srcData);
		}
	}
}

void Manager::PlaceInLoadedArea()
{
	RE::TES::GetSingleton()->ForEachCell([this](auto* cell) {
		if (auto it = game.cells.find(cell->GetFormEditorID()); it != game.cells.end()) {
			for (const auto& srcData : it->second) {
				srcData.SpawnObject(nullptr, cell, cell->worldSpace);
			}
		}
		cell->ForEachReference([this](auto* ref) {
			if (auto it = game.objects.find(ref->GetFormID()); it != game.objects.end()) {
				for (const auto& srcData : it->second) {
					srcData.SpawnObject(ref, ref->GetParentCell(), ref->GetWorldspace());
				}
			}
			return RE::BSContainer::ForEachResult::kContinue;
		});
	});
}

std::optional<std::filesystem::path> Manager::GetSaveDirectory()
{
	if (!saveDirectory) {
		wchar_t*                                                       buffer{ nullptr };
		const auto                                                     result = REX::W32::SHGetKnownFolderPath(REX::W32::FOLDERID_Documents, REX::W32::KF_FLAG_DEFAULT, nullptr, std::addressof(buffer));
		std::unique_ptr<wchar_t[], decltype(&REX::W32::CoTaskMemFree)> knownPath(buffer, REX::W32::CoTaskMemFree);
		if (!knownPath || result != 0) {
			logger::error("failed to get known folder path"sv);
			saveDirectory = std::nullopt;
		} else {
			saveDirectory = knownPath.get();
			*saveDirectory /= "My Games"sv;
			if (::GetModuleHandle(TEXT("Galaxy64"))) {
				*saveDirectory /= "Skyrim Special Edition GOG"sv;
			} else {
				*saveDirectory /= "Skyrim Special Edition"sv;
			}
			*saveDirectory /= "Saves"sv;
			*saveDirectory /= "BaseObjectPlacer"sv;

			std::error_code ec;
			if (!std::filesystem::exists(*saveDirectory, ec)) {
				std::filesystem::create_directory(*saveDirectory);
			}
		}
	}
	return saveDirectory;
}

std::optional<std::filesystem::path> Manager::GetFile(const std::string& a_save)
{
	auto jsonPath = GetSaveDirectory();
	if (!jsonPath) {
		return std::nullopt;
	}

	*jsonPath /= a_save;
	jsonPath->replace_extension(".json");

	return jsonPath;
}

void Manager::SaveFiles(const std::string& a_save)
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

void Manager::LoadFiles(const std::string& a_save)
{
	loadingSave = true;

	const auto& jsonPath = GetFile(a_save);
	if (!jsonPath) {
		return;
	}

	ClearSavedObjects();

	logger::info("Loading save {}", a_save);

	std::error_code err;
	if (std::filesystem::exists(*jsonPath, err)) {
		std::string buffer;
		auto        ec = glz::read_file_json(savedObjects, jsonPath->string(), buffer);
		if (ec) {
			logger::info("\tFailed to read json (error: {})", glz::format_error(ec, buffer));
		}
	}

	logger::info("\t{} saved objects", savedObjects.size());

	// delete hashes not present i
	if (!savedObjects.empty()) {
		erase_if(savedObjects,
			[this](const auto& entry) {
				if (!configObjects.contains(entry.first)) {
					return true;
				}
				return false;
			});
	}
}

void Manager::DeleteSavedFiles(const std::string& a_save)
{
	auto jsonPath = GetFile(a_save);
	if (!jsonPath) {
		return;
	}

	std::filesystem::remove(*jsonPath);
}

void Manager::ClearSavedObjects(bool a_deleteObjects)
{
	if (a_deleteObjects) {
		for (auto& [hash, id] : savedObjects) {
			if (auto ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(id.GetNumericID())) {
				if (ref->extraList.HasType<RE::ExtraCachedScale>()) {  // make sure we're actually deleting a placed object and not a game ref with an overriden formid
					RE::GarbageCollector::GetSingleton()->Add(ref, true);
				}
			}
		}
	}

	savedObjects.clear();
}

std::size_t Manager::GetSerializedObject(RE::TESObjectREFR* a_ref)
{
	if (a_ref->IsDynamicForm()) {
		if (auto xData = a_ref->extraList.GetByType<RE::ExtraCachedScale>()) {  // actor specific xData
			return RE::RecombineValue(xData->scale3D, xData->refScale);
		}
	}

	return 0;
}

void Manager::SerializeObject(std::size_t hash, const RE::TESObjectREFRPtr& a_ref, bool a_temporary)
{
	auto xData = RE::BSExtraData::Create<RE::ExtraCachedScale>();  // only actors have this xData
	RE::SplitValue(hash, xData->scale3D, xData->refScale);
	a_ref->extraList.Add(xData);
	a_ref->AddChange(RE::TESObjectREFR::ChangeFlags::kGameOnlyExtra);

	if (a_temporary) {
		a_ref->SetTemporary();
	} else {
		RE::BGSNumericIDIndex numericID{};
		numericID.SetNumericID(a_ref->GetFormID());
		savedObjects.emplace(hash, numericID);
	}
}

void Manager::LoadSerializedObject(RE::TESObjectREFR* a_ref)
{
	VisitSerializedObject(a_ref, [a_ref](const auto* sourceData) {
		sourceData->SetProperties(a_ref);
	});
}

void Manager::FinishLoadSerializedObject(RE::TESObjectREFR* a_ref)
{
	if (auto hash = GetSerializedObject(a_ref); hash != 0) {
		RE::BGSNumericIDIndex curID{};
		curID.SetNumericID(a_ref->GetFormID());

		auto savedID = Manager::GetSingleton()->GetSavedObject(hash);

		bool shouldDeleteRef = false;
		if (!savedID) {
			logger::error("\t\tObject with hash {} did not have a corresponding config entry. Deleting saved object. [FormID: {:X}]", hash, a_ref->GetFormID());
			shouldDeleteRef = true;
		} else if (std::tie(savedID->data1, savedID->data2, savedID->data3) != std::tie(curID.data1, curID.data2, curID.data3)) {
			logger::error("\t\tObject {:X} - saved ID and current ID mismatch. Deleting saved object. [Expected: ({}, {}, {}), Found: ({}, {}, {})]",
				a_ref->GetFormID(), curID.data1, curID.data2, curID.data3, savedID->data1, savedID->data2, savedID->data3);
			shouldDeleteRef = true;
		}

		if (shouldDeleteRef) {
			RE::GarbageCollector::GetSingleton()->Add(a_ref, true);
		} else {
			if (auto sourceData = GetConfigObject(hash)) {
				sourceData->SetPropertiesHavok(a_ref, a_ref->Get3D());
			}
		}
	}
}

void Manager::UpdateSerializedObjectHavok(RE::TESObjectREFR* a_ref)
{
	VisitSerializedObject(a_ref, [a_ref](const auto* sourceData) {
		sourceData->SetPropertiesHavok(a_ref, a_ref->Get3D());
	});
}

std::optional<RE::BGSNumericIDIndex> Manager::GetSavedObject(std::size_t a_hash)
{
	auto it = savedObjects.find(a_hash);
	return it != savedObjects.end() ? std::optional(it->second) : std::nullopt;
}

const Game::SourceData* Manager::GetConfigObject(std::size_t a_hash)
{
	auto it = configObjects.find(a_hash);
	return it != configObjects.end() ? it->second : nullptr;
}

RE::BSEventNotifyControl Manager::ProcessEvent(const RE::TESCellFullyLoadedEvent* a_event, RE::BSTEventSource<RE::TESCellFullyLoadedEvent>*)
{
	if (!a_event || !a_event->cell || loadingSave) {
		return RE::BSEventNotifyControl::kContinue;
	}

	auto cell = a_event->cell;

	if (auto it = game.cells.find(cell->GetFormEditorID()); it != game.cells.end()) {
		for (const auto& srcData : it->second) {
			srcData.SpawnObject(nullptr, cell, cell->worldSpace);
		}
	}

	return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl Manager::ProcessEvent(const RE::TESCellAttachDetachEvent* a_event, RE::BSTEventSource<RE::TESCellAttachDetachEvent>*)
{
	if (!a_event || !a_event->attached || loadingSave) {
		return RE::BSEventNotifyControl::kContinue;
	}

	auto ref = a_event->reference;
	if (!ref) {
		return RE::BSEventNotifyControl::kContinue;
	}

	if (auto it = game.objects.find(ref->GetFormID()); it != game.objects.end()) {
		for (const auto& srcData : it->second) {
			srcData.SpawnObject(ref.get(), ref->GetParentCell(), ref->GetWorldspace());
		}
	}

	return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl Manager::ProcessEvent(const RE::TESLoadGameEvent* a_event, RE::BSTEventSource<RE::TESLoadGameEvent>*)
{
	if (!a_event || !loadingSave) {
		return RE::BSEventNotifyControl::kContinue;
	}

	PlaceInLoadedArea();

	loadingSave = false;

	return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl Manager::ProcessEvent(const RE::TESFormDeleteEvent* a_event, RE::BSTEventSource<RE::TESFormDeleteEvent>*)
{
	if (!a_event || a_event->formID == 0) {
		return RE::BSEventNotifyControl::kContinue;
	}

	return RE::BSEventNotifyControl::kContinue;
}

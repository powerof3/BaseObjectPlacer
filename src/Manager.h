#pragma once

#include "Config/Format.h"
#include "GameData.h"

class Manager :
	public REX::Singleton<Manager>,
	public RE::BSTEventSink<RE::TESCellFullyLoadedEvent>,
	public RE::BSTEventSink<RE::TESCellAttachDetachEvent>,
	public RE::BSTEventSink<RE::TESLoadGameEvent>,
	public RE::BSTEventSink<RE::TESFormDeleteEvent>
{
public:
	std::pair<bool,bool> ReadConfigs(bool a_reload = false);
	void                 ReloadConfigs();

	void OnDataLoad();

	std::optional<RE::BGSNumericIDIndex> GetSavedObject(std::size_t a_hash);
	const Game::SourceData*              GetConfigObject(std::size_t a_hash);

	void LoadFiles(const std::string& a_save);
	void SaveFiles(const std::string& a_save);
	void DeleteSavedFiles(const std::string& a_save);
	void ClearSavedObjects(bool a_deleteObjects = false);

	std::size_t GetSerializedObject(RE::TESObjectREFR* a_ref);
	void        UpdateSerializedObjectHavok(RE::TESObjectREFR* a_ref);
	void        SerializeObject(std::size_t hash, const RE::TESObjectREFRPtr& a_ref, bool a_temporary);

	void LoadSerializedObject(RE::TESObjectREFR* a_ref);
	void FinishLoadSerializedObject(RE::TESObjectREFR* a_ref);

private:
	struct detail
	{
		template <class T>
		static void add_event_sink()
		{
			RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<T>(Manager::GetSingleton());
		}
	};

	void ProcessConfigs();
	void ProcessConfigObjects();
	void PlaceInLoadedArea();

	std::optional<std::filesystem::path> GetSaveDirectory();
	std::optional<std::filesystem::path> GetFile(const std::string& a_save);

	template <class F>
	bool VisitSerializedObject(RE::TESObjectREFR* a_ref, F&& func)
	{
		if (auto hash = GetSerializedObject(a_ref); hash != 0) {
			if (auto sourceData = GetConfigObject(hash)) {
				func(sourceData);
				return true;
			}
		}

		return false;
	}

	RE::BSEventNotifyControl ProcessEvent(const RE::TESCellFullyLoadedEvent* a_event, RE::BSTEventSource<RE::TESCellFullyLoadedEvent>*) override;
	RE::BSEventNotifyControl ProcessEvent(const RE::TESCellAttachDetachEvent* a_event, RE::BSTEventSource<RE::TESCellAttachDetachEvent>*) override;
	RE::BSEventNotifyControl ProcessEvent(const RE::TESLoadGameEvent* a_event, RE::BSTEventSource<RE::TESLoadGameEvent>*) override;
	RE::BSEventNotifyControl ProcessEvent(const RE::TESFormDeleteEvent* a_event, RE::BSTEventSource<RE::TESFormDeleteEvent>*) override;

	// members
	Config::Format                                configs;
	Game::Format                                  game;
	StringMap<FlatSet<std::size_t>>               uuidHashes;     // [uuid, entry hashes]
	FlatMap<std::size_t, const Game::SourceData*> configObjects;  // [entry hash, ptr to object inside configs]
	FlatMap<std::size_t, RE::BGSNumericIDIndex>   savedObjects;   // [entry hash, ref]
	std::optional<std::filesystem::path>          saveDirectory;
	std::vector<RE::BGSNumericIDIndex>            createdObjects;
	bool                                          loadingSave{ false };
};

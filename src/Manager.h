#pragma once

#include "Config/Format.h"
#include "Game/Object.h"
#include "Game/CreatedObject.h"

class Manager :
	public REX::Singleton<Manager>,
	public RE::BSTEventSink<RE::TESCellFullyLoadedEvent>,
	public RE::BSTEventSink<RE::TESCellAttachDetachEvent>,
	public RE::BSTEventSink<RE::TESLoadGameEvent>,
	public RE::BSTEventSink<RE::TESFormDeleteEvent>
{
public:
	std::pair<bool, bool> ReadConfigs(bool a_reload = false);
	void                  ReloadConfigs();

	void OnDataLoad();

	RE::FormID          GetSavedObject(std::size_t a_hash);
	const Game::Object* GetConfigObject(std::size_t a_hash);

	void LoadFiles(std::string_view a_save);
	void SaveFiles(std::string_view a_save);
	void DeleteSavedFiles(std::string_view a_save);
	void CleanupSavedFiles();

	bool IsTempObject(RE::TESObjectREFR* a_ref);
	void ClearTempObject(RE::TESObjectREFR* a_ref);

	void ClearSavedObjects(bool a_deleteObjects = false);

	void        SerializeHash(std::size_t hash, RE::ExtraCachedScale* a_scaleExtra);
	std::size_t DeserializeHash(RE::ExtraCachedScale* a_scaleExtra);

	RE::ExtraCachedScale* GetSerializedObjectData(RE::TESObjectREFR* a_ref);
	std::size_t           GetSerializedObjectHash(RE::TESObjectREFR* a_ref);
	void                  UpdateSerializedObjectHavok(RE::TESObjectREFR* a_ref);
	void                  SerializeObject(std::size_t hash, const RE::TESObjectREFRPtr& a_ref, bool a_temporary);

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

	void SpawnInCell(RE::TESObjectCELL* a_cell);
	void SpawnAtReference(RE::TESObjectREFR* a_ref);

	void ProcessConfigs();
	void ProcessConfigObjects();
	void PlaceInLoadedArea();

	std::optional<std::filesystem::path> GetSaveDirectory();
	std::optional<std::filesystem::path> GetFile(std::string_view a_save);

	template <class F>
	bool VisitSerializedObject(RE::TESObjectREFR* a_ref, F&& func)
	{
		if (auto hash = GetSerializedObjectHash(a_ref); hash != 0) {
			if (auto object = GetConfigObject(hash)) {
				func(object, hash);
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
	Config::Format                            configs;
	Game::Format                              game;
	FlatMap<std::size_t, const Game::Object*> configObjects;  // static. [entry hash, ptr to game object inside configs]
	CreatedObjects                            savedObjects;
	CreatedObjects                            tempObjects;
	std::optional<std::filesystem::path>      saveDirectory;
	bool                                      loadingSave{ false };
};

#include "Game/CreatedObject.h"

#include "Manager.h"

void CreatedObjects::clear(bool a_deleteObjects)
{
	if (a_deleteObjects) {
		auto mgr = Manager::GetSingleton();
		
		for (auto& [hash, id] : map) {
			if (auto ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(id); mgr->GetSerializedObjectHash(ref) == hash) {
				RE::GarbageCollector::GetSingleton()->Add(ref, true);
			}
		}
	}

	clear();
}

bool CreatedObjects::erase(RE::FormID a_formID)
{
	if (auto it = inverseMap.find(a_formID); it != inverseMap.end()) {
		map.erase(it->second);
		inverseMap.erase(it);
		return true;
	}
	return false;
}

bool CreatedObjects::erase(std::size_t a_hash)
{
	if (auto it = map.find(a_hash); it != map.end()) {
		inverseMap.erase(it->second);
		map.erase(it);
		return true;
	}
	return false;
}

RE::FormID CreatedObjects::find(std::size_t a_hash) const
{
	auto it = map.find(a_hash);
	return it != map.end() ? it->second : 0;
}

std::size_t CreatedObjects::find(RE::FormID a_formID) const
{
	auto it = inverseMap.find(a_formID);
	return it != inverseMap.end() ? it->second : 0;
}

void CreatedObjects::rebuild_inverse_map()
{
	inverseMap.clear();
	inverseMap.reserve(map.size());
	for (const auto& [hash, id] : map) {
		inverseMap.emplace(id, hash);
	}
}

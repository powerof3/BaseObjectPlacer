#pragma once

#include "Game/Object.h"

struct CreatedObjects
{
	bool        empty() const noexcept { return map.empty(); }
	std::size_t size() const noexcept { return map.size(); }

	void clear(bool a_deleteObjects);

	void clear() noexcept
	{
		map.clear();
		inverseMap.clear();
	}

	void emplace(std::size_t a_hash, RE::FormID a_id)
	{
		map.emplace(a_hash, a_id);
		inverseMap.emplace(a_id, a_hash);
	}

	bool erase(RE::FormID a_formID);
	bool erase(std::size_t a_hash);

	RE::FormID  find(std::size_t a_hash) const;
	std::size_t find(RE::FormID a_formID) const;

	void rebuild_inverse_map();

	REL::Version                     version{ 1, 0, 0, 0 };
	FlatMap<std::size_t, RE::FormID> map;         // [entry hash, ref]
	FlatMap<RE::FormID, std::size_t> inverseMap;  // [ref, entry hash]
};

template <>
struct glz::meta<CreatedObjects>
{
	using T = CreatedObjects;

	static constexpr auto read_map = [](T& s, const FlatMap<std::size_t, RE::BGSNumericIDIndex>& input) {
		s.map.clear();
		s.map.reserve(input.size());
		for (const auto& [hash, numericID] : input) {
			s.map.emplace(hash, numericID.GetNumericID());
		}
		s.rebuild_inverse_map();
	};

	static constexpr auto write_map = [](T& s) {
		FlatMap<std::size_t, RE::BGSNumericIDIndex> output;
		for (const auto& [hash, formID] : s.map) {
			RE::BGSNumericIDIndex id;
			id.SetNumericID(formID);
			output.emplace(hash, id);
		}
		return output;
	};

	static constexpr auto value = object(
		"version", &T::version,
		"objects", glz::custom<read_map, write_map>);
};

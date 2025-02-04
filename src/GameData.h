#pragma once

#include "Common.h"

namespace Config
{
	class ObjectData;
}

namespace Game
{
	class SourceData
	{
	public:
		SourceData() = default;
		SourceData(const Config::ObjectData& a_data, std::uint32_t a_attachID);
		SourceData(const Config::ObjectData& a_data);

		bool IsTemporary(const RE::TESObjectREFRPtr& a_ref) const;

		RE::BSTransform GetTransform(RE::TESObjectREFR* a_ref) const;

		void SetProperties(RE::TESObjectREFR* a_ref) const;
		void SetPropertiesHavok(RE::TESObjectREFR* a_ref, RE::NiAVObject* a_root) const;
		void SetPropertiesFlags(RE::TESObjectREFR* a_ref) const;

		void SpawnObject(RE::TESObjectREFR* a_ref, RE::TESObjectCELL* a_cell, RE::TESWorldSpace* a_worldSpace) const;

		// members
		RE::FormID                                        attachID{ 0 };
		RE::FormID                                        baseID{ 0 };
		RE::BSTransform                                   transform;
		REX::EnumSet<Data::ReferenceFlags, std::uint32_t> flags;
		RE::FormID                                        encounterZone{ 0 };
		Data::MotionType                                  motionType;
		GameExtraData                                     extraData;
		std::size_t                                       hash{ 0 };
	};

	using FormIDObjectMap = FlatMap<RE::FormID, std::vector<SourceData>>;
	using EditorIDObjectMap = StringMap<std::vector<SourceData>>;

	struct Format
	{
		void clear()
		{
			cells.clear();
			objects.clear();
		}
		
		// members
		EditorIDObjectMap cells;
		FormIDObjectMap   objects;
	};
}

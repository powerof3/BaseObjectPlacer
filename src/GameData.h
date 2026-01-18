#pragma once

#include "SharedData.h"

namespace Config
{
	struct SharedData;
	class ObjectData;
}

namespace Game
{
	struct SharedData
	{
		SharedData(const Config::SharedData& a_data);

		bool IsTemporary() const;

		void SetPropertiesHavok(RE::TESObjectREFR* a_ref, RE::NiAVObject* a_root) const;
		void SetPropertiesFlags(RE::TESObjectREFR* a_ref) const;
		void AttachScripts(RE::TESObjectREFR* a_ref) const;

		// members
		GameExtraData                                     extraData;
		BSScript::GameScripts                             scripts;
		std::unique_ptr<RE::TESCondition>                 conditions;
		Data::MotionType                                  motionType;
		REX::EnumSet<Data::ReferenceFlags, std::uint32_t> flags;
		float                                             chance{ 1.0f };

	private:
		template <class U>
		static void CreateScriptArray(std::string_view scriptName, RE::BSScript::Variable* a_variable, const std::vector<U>& a_val)
		{
			auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
			auto* handlePolicy = vm->GetObjectHandlePolicy();
			auto* bindPolicy = vm->GetObjectBindPolicy();

			RE::BSTSmartPointer<RE::BSScript::Array> arr;

			RE::BSScript::TypeInfo typeInfo;
			if constexpr (std::is_same_v<U, RE::TESForm*>) {
				typeInfo = RE::BSScript::TypeInfo::RawType::kObject;
			} else if constexpr (std::is_same_v<U, std::string>) {
				typeInfo = RE::BSScript::TypeInfo::RawType::kString;
			} else if constexpr (std::is_same_v<U, std::int32_t>) {
				typeInfo = RE::BSScript::TypeInfo::RawType::kInt;
			} else if constexpr (std::is_same_v<U, float>) {
				typeInfo = RE::BSScript::TypeInfo::RawType::kFloat;
			} else if constexpr (std::is_same_v<U, bool>) {
				typeInfo = RE::BSScript::TypeInfo::RawType::kBool;
			}

			vm->CreateArray(typeInfo, static_cast<std::uint32_t>(a_val.size()), arr);

			for (std::uint32_t i = 0; i < a_val.size(); i++) {
				if constexpr (std::is_same_v<U, RE::TESForm*>) {
					auto                                      form = a_val[i];
					RE::BSTSmartPointer<RE::BSScript::Object> obj;
					RE::VMHandle                              handle = handlePolicy->GetHandleForObject(form->GetFormType(), form);
					if (handle && vm->CreateObject(scriptName, obj) && obj) {
						if (bindPolicy) {
							bindPolicy->BindObject(obj, handle);
							arr->data()[i].SetObject(obj);
						}
					}
				} else if constexpr (std::is_same_v<U, std::string>) {
					arr->data()[i].SetString(a_val[i]);
				} else if constexpr (std::is_same_v<U, std::int32_t>) {
					arr->data()[i].SetSInt(a_val[i]);
				} else if constexpr (std::is_same_v<U, float>) {
					arr->data()[i].SetFloat(a_val[i]);
				} else if constexpr (std::is_same_v<U, bool>) {
					arr->data()[i].SetBool(a_val[i]);
				}
			}

			a_variable->SetArray(arr);
		}
	};

	class SourceData
	{
	public:
		SourceData() = default;
		SourceData(std::shared_ptr<SharedData> a_data, std::uint32_t a_attachID);
		SourceData(std::shared_ptr<SharedData> a_data);

		bool IsTemporary(const RE::TESObjectREFRPtr& a_ref) const;

		RE::BSTransform GetTransform(RE::TESObjectREFR* a_ref) const;

		void SetProperties(RE::TESObjectREFR* a_ref) const;

		void SpawnObject(RE::TESObjectREFR* a_ref, RE::TESObjectCELL* a_cell, RE::TESWorldSpace* a_worldSpace) const;

		// members
		RE::FormID                  attachID{ 0 };
		RE::FormID                  baseID{ 0 };
		RE::BSTransform             transform;
		std::shared_ptr<SharedData> data;
		std::size_t                 hash{ 0 };
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

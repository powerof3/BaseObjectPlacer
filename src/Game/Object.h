#pragma once

#include "SharedData.h"

namespace Config
{
	struct ObjectArray;
	class Object;
	struct SharedData;
}

namespace Game
{
	struct ObjectFilter
	{
		using FilterEntry = std::variant<RE::FormID, std::string>;

		struct Input
		{
			Input() = default;
			Input(RE::TESObjectREFR* a_ref, RE::TESObjectCELL* a_cell);

			// members
			RE::TESObjectREFR*  ref;
			RE::TESBoundObject* baseObj;
			std::string_view    fileName;
			std::string_view    cellEDID;
		};

		ObjectFilter() = default;
		explicit ObjectFilter(const Config::SharedData& a_data);

		bool IsAllowed(RE::TESObjectREFR* a_ref, RE::TESObjectCELL* a_cell) const;

		// members
		std::vector<FilterEntry> whiteList;
		std::vector<FilterEntry> blackList;

	private:
		static bool CheckList(const std::vector<FilterEntry>& a_list, const Input& input);
		static bool MatchFormID(RE::FormID a_id, const Input& input);
		static bool MatchString(const std::string& a_str, const Input& input);
	};

	struct SharedData
	{
		SharedData() = default;
		explicit SharedData(const Config::SharedData& a_data);

		bool PassesFilters(RE::TESObjectREFR* a_ref, RE::TESObjectCELL* a_cell) const;

		bool IsTemporary() const;

		void SetPropertiesHavok(RE::TESObjectREFR* a_ref, RE::NiAVObject* a_root) const;
		void SetPropertiesFlags(RE::TESObjectREFR* a_ref) const;
		void AttachScripts(RE::TESObjectREFR* a_ref) const;

		// members
		GameExtraData                                     extraData;
		BSScript::GameScripts                             scripts;
		std::shared_ptr<RE::TESCondition>                 conditions;
		Data::MotionType                                  motionType;
		ObjectFilter                                      filter;
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

	class Object
	{
	public:
		struct Instance
		{
			enum class Flags
			{
				kNone = 0,
				kRandomizeRotation = 1 << 0,
				kRandomizeScale = 1 << 1,
				kRelativeTranslate = 1 << 2,
				kRelativeRotate = 1 << 3,
				kRelativeScale = 1 << 4,
			};

			Instance() = default;
			Instance(std::uint32_t a_baseIndex, const RE::BSTransformRange& a_range, const RE::BSTransform& a_transform, Flags a_flags, std::size_t a_hash);
			Instance(std::uint32_t a_baseIndex, const RE::BSTransformRange& a_range, Flags a_flags, std::size_t a_hash);

			static Flags GetInstanceFlags(const RE::BSTransformRange& a_range, const Config::ObjectArray& a_array);

			RE::BSTransform GetWorldTransform(const RE::NiPoint3& a_refPos, const RE::NiPoint3& a_refAngle, std::size_t a_hash) const;

			// members
			std::uint32_t                      baseIndex{ 0 };
			RE::BSTransform                    transform;
			RE::BSTransformRange               transformRange;
			REX::EnumSet<Flags, std::uint32_t> flags;
			std::size_t                        hash;
		};

		Object() = default;
		explicit Object(const Config::SharedData& a_data);

		void SetProperties(RE::TESObjectREFR* a_ref, std::size_t hash) const;
		void SpawnObject(RE::TESDataHandler* a_dataHandler, RE::TESObjectREFR* a_ref, RE::TESObjectCELL* a_cell, RE::TESWorldSpace* a_worldSpace, bool a_doRayCast) const;

		// members
		SharedData              data;
		std::vector<RE::FormID> bases;
		std::vector<Instance>   instances;
	};

	using FormIDObjectMap = FlatMap<std::variant<RE::FormID, std::string>, std::vector<Game::Object>>;
	using EditorIDObjectMap = StringMap<std::vector<Game::Object>>;
	using FormTypeObjectMap = FlatMap<RE::FormType, std::vector<Game::Object>>;

	struct Format
	{
		void clear()
		{
			cells.clear();
			objects.clear();
			objectTypes.clear();
		}

		void SpawnInCell(RE::TESObjectCELL* a_cell);
		void SpawnAtReference(RE::TESObjectREFR* a_ref);

		// members
		EditorIDObjectMap cells;
		FormIDObjectMap   objects;
		FormTypeObjectMap objectTypes;
	};
}

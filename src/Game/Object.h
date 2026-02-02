#pragma once

#include "SharedData.h"

namespace Config
{
	struct SharedData;
	class Object;
}

namespace Game
{
	struct SharedData
	{
		SharedData() = default;
		explicit SharedData(const Config::SharedData& a_data);

		bool IsTemporary() const;

		void SetPropertiesHavok(RE::TESObjectREFR* a_ref, RE::NiAVObject* a_root) const;
		void SetPropertiesFlags(RE::TESObjectREFR* a_ref) const;
		void AttachScripts(RE::TESObjectREFR* a_ref) const;

		// members
		GameExtraData                                     extraData;
		BSScript::GameScripts                             scripts;
		std::shared_ptr<RE::TESCondition>                 conditions;
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

	class Object
	{
	public:
		struct Instance
		{
			Instance() = default;
			Instance(std::uint32_t a_baseIndex, const RE::BSTransform& a_transform, std::size_t a_hash) :
				baseIndex(a_baseIndex),
				transform(a_transform),
				hash(a_hash)
			{}
			Instance(std::uint32_t a_baseIndex, const RE::BSTransformRange& a_range, std::size_t a_hash) :
				baseIndex(a_baseIndex),
				transform(a_range, a_hash),
				hash(a_hash)
			{}

			RE::BSTransform GetWorldTransform(const RE::NiPoint3& a_refPos, const RE::NiPoint3& a_refAngle) const;

			std::uint32_t   baseIndex{ 0 };
			RE::BSTransform transform;
			std::size_t     hash;
		};

		Object() = default;
		explicit Object(const Config::SharedData& a_data);

		void SetProperties(RE::TESObjectREFR* a_ref, std::size_t hash) const;
		void SpawnObject(RE::TESDataHandler* a_dataHandler, RE::TESObjectREFR* a_ref, RE::TESObjectCELL* a_cell, RE::TESWorldSpace* a_worldSpace, bool a_doRayCast) const;

		// members
		SharedData                        data;
		std::vector<RE::FormID>           bases;
		std::vector<Instance>             instances;
		std::vector<RE::BSTransformRange> transforms;
	};

	using FormIDObjectMap = FlatMap<std::variant<RE::FormID, std::string>, std::vector<Game::Object>>;
	using EditorIDObjectMap = StringMap<std::vector<Game::Object>>;

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

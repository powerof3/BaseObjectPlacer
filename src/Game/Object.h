#pragma once

#include "SharedData.h"

namespace Config
{
	struct ObjectArray;
	class Object;
	struct FilterData;
	struct ObjectData;
}

namespace Game
{
	using ReferenceFlags = Base::ReferenceFlags;

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
			RE::TESObjectCELL*  cell;
			std::string_view    fileName;
		};

		ObjectFilter() = default;
		explicit ObjectFilter(const Config::FilterData& a_filter);

		bool IsAllowed(RE::TESObjectREFR* a_ref, RE::TESObjectCELL* a_cell) const;

		// members
		std::vector<FilterEntry> whiteList;
		std::vector<FilterEntry> blackList;

	private:
		static bool CheckList(const std::vector<FilterEntry>& a_list, const Input& input);
		static bool MatchFormID(RE::FormID a_id, const Input& input);
		static bool MatchString(const std::string& a_str, const Input& input);
	};

	struct FilterData
	{
		FilterData() = default;
		explicit FilterData(const Config::FilterData& a_filter);

		bool PassesFilters(RE::TESObjectREFR* a_ref, RE::TESObjectCELL* a_cell) const;

		// members
		std::shared_ptr<RE::TESCondition> conditions;
		ObjectFilter                      filter;
	};

	struct ObjectData
	{
		ObjectData() = default;
		explicit ObjectData(const Config::ObjectData& a_data);

		void Merge(const Game::ObjectData& a_parent);

		bool PreventClipping(const RE::TESBoundObject* a_base) const;

		void SetProperties(RE::TESObjectREFR* a_ref, std::size_t a_hash) const;
		void SetPropertiesHavok(RE::TESObjectREFR* a_ref, RE::NiAVObject* a_root) const;

		// members
		GameExtraData                               extraData;
		BSScript::GameScripts                       scripts;
		Base::MotionType                            motionType;
		REX::EnumSet<ReferenceFlags, std::uint32_t> flags;

	private:
		void SetPropertiesFlags(RE::TESObjectREFR* a_ref) const;
		void AttachScripts(RE::TESObjectREFR* a_ref) const;

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
		struct Params
		{
			struct RefParams
			{
				RefParams() = default;
				RefParams(RE::TESObjectREFR* a_ref);

				// members
				RE::RawFormID   id;
				RE::BoundingBox bb;
				float           scale;
			};

			Params(RE::TESObjectREFR* a_ref);
			Params(RE::TESObjectCELL* a_cell);

			// members
			RefParams          refParams;
			RE::TESObjectREFR* ref;
			RE::TESObjectCELL* cell;
			RE::TESWorldSpace* worldspace;
		};

		struct Instance
		{
			enum class Flags
			{
				kNone = 0,
				kSequentialObjects = 1 << 0,
				kRandomizeRotation = 1 << 1,
				kRandomizeScale = 1 << 2,
				kRelativeTranslate = 1 << 3,
				kRelativeRotate = 1 << 4,
				kRelativeScale = 1 << 5,
			};

			Instance() = default;
			Instance(const RE::BSTransformRange& a_range, const RE::BSTransform& a_transform, Flags a_flags, std::size_t a_hash);
			Instance(const RE::BSTransformRange& a_range, Flags a_flags, std::size_t a_hash);

			static REX::EnumSet<Flags> GetInstanceFlags(const Game::ObjectData& a_data, const RE::BSTransformRange& a_range, const Config::ObjectArray& a_array);

			RE::BSTransform GetWorldTransform(const RE::NiPoint3& a_refPos, const RE::NiPoint3& a_refAngle, std::size_t a_hash) const;

			// members
			RE::BSTransform                    transform;
			RE::BSTransformRange               transformRange;
			REX::EnumSet<Flags, std::uint32_t> flags;
			std::size_t                        hash;
		};

		Object() = default;
		explicit Object(const Config::ObjectData& a_data);

		void SpawnObject(RE::TESDataHandler* a_dataHandler, const Params& a_params, std::uint32_t& a_numHandles, bool a_isTemporary, std::size_t a_parentHash, const std::vector<Object>& a_childObjects = {}) const;

		// members
		ObjectData            data;
		Base::WeightedObjects bases;
		std::vector<Instance> instances;
	};

	class RootObject : public Object
	{
	public:
		RootObject() = default;
		explicit RootObject(const Config::FilterData& a_filter, const Config::ObjectData& a_data);

		bool IsTemporary() const;

		void SpawnObject(RE::TESDataHandler* a_dataHandler, const Params& a_params, std::uint32_t& a_numHandles) const;

		// members
		FilterData          filter;
		std::vector<Object> childObjects;
	};

	using FormIDObjectMap = FlatMap<std::variant<RE::FormID, std::string>, std::vector<Game::RootObject>>;
	using EditorIDObjectMap = StringMap<std::vector<Game::RootObject>>;
	using FormTypeObjectMap = FlatMap<RE::FormType, std::vector<Game::RootObject>>;

	struct Format
	{
		void clear()
		{
			cells.clear();
			objects.clear();
			objectTypes.clear();
		}

		std::vector<Game::RootObject>* FindObjects(const RE::TESObjectREFR* a_ref, const RE::TESBoundObject* a_base);
		std::vector<Game::RootObject>* FindObjects(const RE::TESBoundObject* a_base);

		void SpawnInCell(RE::TESObjectCELL* a_cell);
		void SpawnAtReference(RE::TESObjectREFR* a_ref);

		// members
		EditorIDObjectMap cells;
		FormIDObjectMap   objects;
		FormTypeObjectMap objectTypes;
	};
}

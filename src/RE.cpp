#include "RE.h"

namespace RE
{
	RawFormID::RawFormID(FormID a_formID, std::string_view a_modName) :
		id(a_formID),
		localID(a_formID & 0x00FFFFFF),
		modName(a_modName)
	{}

	RawFormID::RawFormID(FormID a_formID) :
		id(a_formID),
		localID(a_formID & 0x00FFFFFF)
	{
		if (a_formID == 0) {
			return;
		}

		static auto        dataHandler = TESDataHandler::GetSingleton();
		const std::uint8_t modIndex = a_formID >> 24;

		TESFile* file = nullptr;
		if (modIndex == 0xFE) {
			const std::uint16_t lightIndex = (a_formID >> 12) & 0xFFF;
			file = const_cast<TESFile*>(dataHandler->LookupLoadedLightModByIndex(lightIndex));
		} else {
			file = const_cast<TESFile*>(dataHandler->LookupLoadedModByIndex(modIndex));
		}
		if (file) {
			modName = file->fileName;
		}
	}

	TESForm* GetForm(const std::string& a_str)
	{
		TESForm* form = nullptr;
		if (const auto splitID = string::split(a_str, "~"); splitID.size() == 2) {
			const auto  formID = string::to_num<FormID>(splitID[0], true);
			const auto& modName = splitID[1];
			if (g_mergeMapperInterface) {
				const auto [mergedModName, mergedFormID] = g_mergeMapperInterface->GetNewFormID(modName.c_str(), formID);
				form = TESDataHandler::GetSingleton()->LookupForm(mergedFormID, mergedModName);
			} else {
				form = TESDataHandler::GetSingleton()->LookupForm(formID, modName);
			}
		} else if (string::is_only_hex(a_str, true)) {
			form = TESForm::LookupByID(string::to_num<FormID>(a_str, true));
		} else {
			form = TESForm::LookupByEditorID(a_str);
		}
		return form;
	}

	RawFormID GetRawFormID(const std::string& a_str, bool a_checkEDID)
	{
		if (const auto splitID = string::split(a_str, "~"); splitID.size() == 2) {
			const auto  formID = string::to_num<FormID>(splitID[0], true);
			const auto& modName = splitID[1];
			if (g_mergeMapperInterface) {
				const auto [mergedModName, mergedFormID] = g_mergeMapperInterface->GetNewFormID(modName.c_str(), formID);
				return RawFormID(mergedFormID, mergedModName);
			}
			return RawFormID(formID, modName);
		}
		if (string::is_only_hex(a_str, true)) {
			return RawFormID(string::to_num<FormID>(a_str, true));
		}
		if (a_checkEDID) {
			if (const auto form = TESForm::LookupByEditorID(a_str)) {
				return RawFormID(form->GetFormID());
			}
		}
		return RawFormID(0);
	}

	FormID GetFormID(const std::string& a_str)
	{
		const auto form = GetForm(a_str);
		return form ? form->GetFormID() : 0;
	}

	std::string GetEditorID(const std::string& a_str)
	{
		auto form = GetForm(a_str);
		return form ? clib_util::editorID::get_editorID(form) : "";
	}

	bool CanBeMoved(const TESObjectREFRPtr& a_refr)
	{
		auto base = a_refr->GetBaseObject();
		if (!base) {
			return false;
		}

		return CanBeMoved(base);
	}

	bool CanBeMoved(const TESForm* a_base)
	{
		switch (a_base->GetFormType()) {
		case FormType::Static:
			return a_base->IsHeadingMarker();
		case FormType::StaticCollection:
		case FormType::MovableStatic:
		case FormType::Grass:
		case FormType::Tree:
		case FormType::Flora:
		case FormType::Furniture:
			return false;
		default:
			return true;
		}
	}

	std::uint32_t GetNumReferenceHandles()
	{
		const auto refrArray = BSPointerHandleManager<TESObjectREFR*>::GetHandleEntries();

		std::uint32_t activeHandleCount = 0;

		for (const auto& val : refrArray) {
			if ((val.handleEntryBits & (1 << 26)) != 0) {
				activeHandleCount++;
			}
		}

		return activeHandleCount;
	}

	bool GetMaxFormIDReached()
	{
		return (TESDataHandler::GetSingleton()->nextID & 0xFFFFFF) >= 0x3FFFFF;
	}

	bool IsFormIDInUse(FormID a_formID)
	{
		return TESForm::LookupByID(a_formID) || BGSSaveLoadGame::GetSingleton()->IsFormIDInUse(a_formID);
	}

	NiPoint3 ApplyRotation(const NiPoint3& point, const NiPoint3& pivot, const NiMatrix3& rotationMatrix)
	{
		// Translate point to local space
		NiPoint3 localPoint = point - pivot;
		NiPoint3 rotatedLocal = rotationMatrix * localPoint;
		return rotatedLocal + pivot;
	}

	void WrapAngle(NiPoint3& a_angle)
	{
		constexpr auto wrap_angle = [](float& angle) {
			angle = fmod(angle, NI_TWO_PI);
			if (angle < 0) {
				angle += NI_TWO_PI;
			}
		};

		wrap_angle(a_angle.x);
		wrap_angle(a_angle.y);
		wrap_angle(a_angle.z);
	}

	void SplitValue(std::size_t value, float& lowFloat, float& highFloat)
	{
		auto low = static_cast<uint32_t>(value);
		auto high = static_cast<uint32_t>(value >> 32);

		lowFloat = std::bit_cast<float>(low);
		highFloat = std::bit_cast<float>(high);
	}

	std::size_t RecombineValue(float lowFloat, float highFloat)
	{
		auto low = std::bit_cast<std::uint32_t>(lowFloat);
		auto high = std::bit_cast<std::uint32_t>(highFloat);

		return (static_cast<std::size_t>(high) << 32) | low;
	}

	void InitScripts(TESObjectREFR* a_ref)
	{
		using func_t = decltype(&InitScripts);
		static REL::Relocation<func_t> func{ RELOCATION_ID(19245, 19671) };
		func(a_ref);
	}
}

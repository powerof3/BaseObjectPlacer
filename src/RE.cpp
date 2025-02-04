#include "RE.h"

namespace RE
{

	FormID GetFormID(const std::string& a_str)
	{
		if (const auto splitID = string::split(a_str, "~"); splitID.size() == 2) {
			const auto  formID = string::to_num<FormID>(splitID[0], true);
			const auto& modName = splitID[1];
			if (g_mergeMapperInterface) {
				const auto [mergedModName, mergedFormID] = g_mergeMapperInterface->GetNewFormID(modName.c_str(), formID);
				return TESDataHandler::GetSingleton()->LookupFormID(mergedFormID, mergedModName);
			}
			return TESDataHandler::GetSingleton()->LookupFormID(formID, modName);
		}
		if (string::is_only_hex(a_str, true)) {
			return string::to_num<FormID>(a_str, true);
		}
		if (const auto form = TESForm::LookupByEditorID(a_str)) {
			return form->GetFormID();
		}
		return 0;
	}

	std::string GetEditorID(const std::string& a_str)
	{
		RE::TESForm* form = nullptr;
		if (const auto splitID = string::split(a_str, "~"); splitID.size() == 2) {
			const auto  formID = string::to_num<FormID>(splitID[0], true);
			const auto& modName = splitID[1];
			if (g_mergeMapperInterface) {
				const auto [mergedModName, mergedFormID] = g_mergeMapperInterface->GetNewFormID(modName.c_str(), formID);
				form = TESDataHandler::GetSingleton()->LookupForm(mergedFormID, mergedModName);
			} else {
				form = TESDataHandler::GetSingleton()->LookupForm(formID, modName);
			}
		}
		if (string::is_only_hex(a_str, true)) {
			form = RE::TESForm::LookupByID(string::to_num<FormID>(a_str, true));
		}
		if (form) {
			return form->GetFormEditorID();
		}
		return a_str;
	}

	bool CanBeMoved(const TESObjectREFRPtr& a_refr)
	{
		auto base = a_refr->GetBaseObject();
		if (!base) {
			return false;
		}

		switch (base->GetFormType()) {
		case FormType::Static:
			return base->IsHeadingMarker();
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

	bool GetMaxFormIDReached()
	{
		return (RE::TESDataHandler::GetSingleton()->nextID & 0xFFFFFF) >= 0x3FFFFF;
	}

	bool IsFormIDInUse(FormID a_formID)
	{
		return RE::TESForm::LookupByID(a_formID) || RE::BGSSaveLoadGame::GetSingleton()->IsFormIDInUse(a_formID);
	}

	NiPoint3 ApplyRotation(const NiPoint3& point, const NiPoint3& pivot, const NiPoint3& rotationOffset)
	{
		// Translate point to pivot space
		auto translatedPoint = point - pivot;

		float theta = rotationOffset.Length();

		if (theta == 0) {
			return point;  // No rotation
		}

		auto k = rotationOffset / theta;  // Rotation axis

		// Rodrigues' rotation formula
		auto rotatedPoint =
			translatedPoint * std::cos(theta) +
			k.Cross(translatedPoint) * std::sin(theta) +
			k * (k.Dot(translatedPoint) * (1 - std::cos(theta)));

		// Translate back to original space
		return rotatedPoint + pivot;
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

		std::memcpy(&lowFloat, &low, sizeof(float));
		std::memcpy(&highFloat, &high, sizeof(float));
	}

	std::size_t RecombineValue(float lowFloat, float highFloat)
	{
		std::uint32_t low;
		std::uint32_t high;

		std::memcpy(&low, &lowFloat, sizeof(float));
		std::memcpy(&high, &highFloat, sizeof(float));

		return (static_cast<std::size_t>(high) << 32) | low;
	}
}

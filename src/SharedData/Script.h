#pragma once

namespace BSScript
{
	using ConfigValue = std::variant<
		std::monostate,             // kNone
		std::string,                // kObject/String
		std::int32_t,               // kInt
		float,                      // kFloat
		bool,                       // kBool
		std::vector<std::string>,   // kObjectArray/StringArray
		std::vector<std::int32_t>,  // kIntArray
		std::vector<float>,         // kFloatArray
		std::vector<bool>           // kBoolArray
		>;

	using GameValue = std::variant<
		std::monostate,             // kNone
		RE::BSFixedString,          // kString
		RE::TESForm*,               // kObject
		std::int32_t,               // kInt
		float,                      // kFloat
		bool,                       // kBool
		std::vector<RE::TESForm*>,  // kObjectArray
		std::vector<std::string>,   // StringArray
		std::vector<std::int32_t>,  // kIntArray
		std::vector<float>,         // kFloatArray
		std::vector<bool>           // kBoolArray
		>;

	template <class T>
	struct Script
	{
		Script() = default;
		Script(const Script<T>& other) :
			script(other.script),
			properties(other.properties)
		{}

		explicit Script(const Script<ConfigValue>& a_config)
			requires std::is_same_v<T, GameValue>
			:
			script(a_config.script),
			autoFillProperties(a_config.autoFillProperties)
		{
			properties.reserve(a_config.properties.size());
			for (const auto& [propName, prop] : a_config.properties) {
				GameValue gameValue;
				std::visit(overload{
							   [&](std::monostate) {},
							   [&](const std::string& a_val) {
								   if (auto form = RE::GetForm(a_val); form) {
									   gameValue = form;
								   } else {
									   gameValue = a_val;
								   }
							   },
							   [&](const std::vector<std::string>& a_val) {
								   if (auto form = RE::GetForm(a_val[0]); form) {
									   std::vector<RE::TESForm*> forms;
									   forms.push_back(form);
									   for (auto& str : std::ranges::drop_view{ a_val, 1 }) {
										   if (form = RE::GetForm(str); form) {
											   forms.push_back(form);
										   }
									   }
									   gameValue = forms;
								   } else {
									   gameValue = a_val;
								   }
							   },
							   [&](const auto& a_val) {
								   gameValue = a_val;
							   } },
					prop);
				properties.emplace_back(propName, gameValue);
			}
		}

		std::string                            script;
		std::vector<std::pair<std::string, T>> properties;
		bool                                   autoFillProperties{ true };

	private:
		GENERATE_HASH(Script<T>, a_val.script, a_val.properties, a_val.autoFillProperties)
	};

	using ConfigScripts = std::vector<Script<ConfigValue>>;
	using GameScripts = std::vector<Script<GameValue>>;
}

template <>
struct glz::meta<BSScript::Script<BSScript::ConfigValue>>
{
	using T = BSScript::Script<BSScript::ConfigValue>;
	static constexpr bool requires_key(std::string_view a_key, bool)
	{
		return a_key == "script";
	}
	static constexpr auto value = object(
		"script", &T::script,
		"properties", &T::properties,
		"autoFillProperties", &T::autoFillProperties);
};

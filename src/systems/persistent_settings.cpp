#include "persistent_settings.hpp"

#include <format>
#include <print>

namespace pop::systems {

/* SETTING */
Setting::~Setting() = default;

auto Setting::key() const -> const std::string& {
    return m_key;
}

auto Setting::to_string() const -> std::string {
    return std::format("{}={}", m_key, value());
}

/* SETTING NUMBER */
SettingNumber::SettingNumber(std::string key, float value) : m_value{ value } { 
    m_key = key;
}

auto SettingNumber::value() const -> std::string {
    return std::to_string(m_value);
}

void SettingNumber::set_value(float v) { 
    m_value = v;
}


/* SETTING STRING */
SettingString::SettingString(std::string key, std::string value) : m_value{ value } { 
    m_key = key;
}

auto SettingString::value() const -> std::string {
    return m_value;
}

void SettingString::set_value(std::string v) { 
    m_value = v;
}

/* PersistentSettings */ 
void PersistentSettings::save(Setting& setting) {
    std::println("{}", setting.to_string());
}

}


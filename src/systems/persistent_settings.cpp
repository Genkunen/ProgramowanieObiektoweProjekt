#include "persistent_settings.hpp"

#include <format>
#include <print>
#include <fstream>
#include <algorithm>

namespace pop::systems {

/* SETTING */
Setting::Setting(std::string key) : m_key{ std::move(key) } {}
Setting::~Setting() = default;

auto Setting::key() const -> const std::string& {
    return m_key;
}

auto Setting::to_string() const -> std::string {
    return std::format("{}={}", m_key, value());
}

/* SETTING NUMBER */
SettingNumber::SettingNumber(std::string key, float value) 
: Setting(std::move(key)), m_value{ value } {}

auto SettingNumber::value() const -> std::string {
    return std::to_string(m_value);
}

void SettingNumber::set_value(float v) { 
    m_value = v;
}

auto SettingNumber::clone() const -> std::unique_ptr<Setting> {
    return std::make_unique<SettingNumber>(*this);
}

/* SETTING STRING */
SettingString::SettingString(std::string key, std::string value) 
: Setting(std::move(key)), m_value{ value } {}

auto SettingString::value() const -> std::string {
    return m_value;
}

void SettingString::set_value(std::string v) { 
    m_value = v;
}

auto SettingString::clone() const -> std::unique_ptr<Setting> {
    return std::make_unique<SettingString>(*this);
}

/* PersistentSettings */ 
void PersistentSettings::amend(std::string key, float value) {
    auto proj = [] (std::unique_ptr<Setting>& ptr) -> const std::string& {
        return ptr->key();
    };

    if (auto it = std::ranges::find(settings, key, proj); it != settings.end()) {
        if (auto number = dynamic_cast<SettingNumber*>(it->get())) {
            number->set_value(value);
        }
    }
    else {
        settings.emplace_back(std::make_unique<SettingNumber>(key, value));
    }
}

void PersistentSettings::amend(std::string key, std::string value) {
    auto proj = [] (std::unique_ptr<Setting>& ptr) -> const std::string& {
        return ptr->key();
    };

    if (auto it = std::ranges::find(settings, key, proj); it != settings.end()) {
        if (auto number = dynamic_cast<SettingString*>(it->get())) {
            number->set_value(std::move(value));
        }
    }
    else {
        settings.emplace_back(std::make_unique<SettingString>(key, value));
    }
}

void PersistentSettings::amend(const Setting& setting) {
    auto it = std::ranges::find_if(settings, [&setting](const std::unique_ptr<Setting>& ptr) {
        return ptr->key() == setting.key();
    });

    if (it != settings.end()) {
        *it = setting.clone();
    } else {
        settings.push_back(setting.clone());
    }
}

void PersistentSettings::save() {
    for (auto& pair : settings) {
        std::println("{}", pair->to_string());
    }
}

void PersistentSettings::load() {

}

std::vector<std::unique_ptr<Setting>> PersistentSettings::settings;

}


#include "persistent_settings.hpp"

#include <format>
#include <print>
#include <fstream>
#include <algorithm>
#include <ranges>
#include <filesystem>
#include <charconv>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <unistd.h>
#include <limits.h>
#endif

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
    if (auto s = get(key); s != nullptr) {
        if (auto number = dynamic_cast<SettingNumber*>(s)) {
            number->set_value(value);
        }
    }
    else {
        settings.emplace_back(std::make_unique<SettingNumber>(key, value));
    }
}

void PersistentSettings::amend(std::string key, std::string value) {
    if (auto s = get(key); s != nullptr) {
        if (auto number = dynamic_cast<SettingString*>(s)) {
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
    for (auto& p : settings) {
        std::println("{}", p->to_string());
    }
}

void PersistentSettings::load() {
    std::filesystem::path path = file_path;

    std::ifstream file(path / file_name, std::ios::in | std::ios::binary);

    if (!file.is_open()) {
        return;
    }

    file.seekg(0, std::ios::end);
    buffer.reserve(file.tellg());
    file.seekg(0, std::ios::beg);

    buffer.assign(std::istreambuf_iterator<char>(file), 
                  std::istreambuf_iterator<char>());
    parse_buffer();
}

auto PersistentSettings::get(const std::string& key) -> Setting* {
    auto proj = [] (std::unique_ptr<Setting>& ptr) -> const std::string& {
        return ptr->key();
    };
    if (auto it = std::ranges::find(settings, key, proj); it != settings.end()) {
        return it->get();
    }
    return nullptr;
}

void PersistentSettings::parse_buffer() {
    auto lines = buffer 
        | std::views::split('\n')
        | std::views::transform([](const auto& line) { return std::string_view{ line }; });

    for (std::string_view line : lines) {
        if (line.empty()) {
            continue;
        }

        auto start = line.find_first_not_of(" \n\t\r\v");
        if (start == std::string_view::npos) {
            continue;
        }
        line.remove_prefix(start);
       
        bool is_const{};
        if (line[start] == '%') {
            line.remove_prefix(1);
            is_const = true;
        }

        auto eq = line.find_first_of("=");
        if (eq == std::string_view::npos) {
            continue;
        }
        
        auto key = std::string{ line.substr(start, eq - start) };
        line.remove_prefix(eq + 1);
        if (line.empty()) {
            continue;
        }

        if (line[0] == '"') {
            size_t end = line.size();

            while (end > 0 && std::ranges::contains(" \n\t\r\v", line[end - 1])) {
                --end;
            }

            auto value = std::string{ line.substr(0, end) };
            settings.emplace_back(std::make_unique<SettingString>(key, value));
        }
        else {
            float value;
            auto [ptr, ec] = std::from_chars(
                line.data(), std::next(line.data(), std::ssize(line)), value);
            if (ec != std::errc()) {
                continue;
            }
            settings.emplace_back(std::make_unique<SettingNumber>(key, value));
        }
    }
}

std::string PersistentSettings::file_path = [] -> std::string {
#if defined(_WIN32)
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    return std::filesystem::path(buffer).parent_path();
#elif defined(__linux__)
    char buffer[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", buffer, PATH_MAX);
    if (count > 0) {
        return std::filesystem::path(std::string(buffer, count)).parent_path();
    }
    return std::filesystem::current_path();
#else
    return std::filesystem::current_path();
#endif
}();

std::vector<std::unique_ptr<Setting>> PersistentSettings::settings;
std::string PersistentSettings::buffer;

}


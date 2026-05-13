#include "persistent_settings.hpp"
#include "systems.hpp"

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <format>
#include <fstream>
#include <print>
#include <ranges>

namespace pop::systems {

/* SETTING */
Option::Option(std::string key) : m_key{ std::move(key) } {}
Option::~Option() = default;

auto Option::key() const -> const std::string& {
    return m_key;
}

auto Option::to_string() const -> std::string {
    return std::format("{}={}", m_key, value());
}

/* SETTING NUMBER */
OptionNumber::OptionNumber(std::string key, float value) 
: Option(std::move(key)), m_value{ value } {}

auto OptionNumber::value() const -> std::string {
    return std::to_string(m_value);
}

auto OptionNumber::real_value() const -> float {
    return m_value;
}

void OptionNumber::set_value(float v) { 
    m_value = v;
}

auto OptionNumber::clone() const -> std::unique_ptr<Option> {
    return std::make_unique<OptionNumber>(*this);
}

/* SETTING STRING */
OptionString::OptionString(std::string key, std::string value) 
: Option(std::move(key)), m_value{ value } {}

auto OptionString::value() const -> std::string {
    return m_value;
}

void OptionString::set_value(std::string v) { 
    m_value = std::move(v);
}

auto OptionString::clone() const -> std::unique_ptr<Option> {
    return std::make_unique<OptionString>(*this);
}

/* PersistentSettings */ 
void PersistentSettings::amend(std::string key, float value) {
    m_dirty = true;
    if (auto s = get(key); s != nullptr) {
        if (auto number = dynamic_cast<OptionNumber*>(s)) {
            number->set_value(value);
        }
    }
    else {
        settings.emplace_back(std::make_unique<OptionNumber>(key, value));
    }
}

void PersistentSettings::amend(std::string key, std::string value) {
    m_dirty = true;
    if (auto s = get(key); s != nullptr) {
        if (auto number = dynamic_cast<OptionString*>(s)) {
            number->set_value(std::move(value));
        }
    }
    else {
        settings.emplace_back(std::make_unique<OptionString>(key, value));
    }
}

void PersistentSettings::amend(const Option& setting) {
    m_dirty = true;
    auto pred = [&setting](const std::unique_ptr<Option>& ptr) {
        return ptr->key() == setting.key();
    };
    auto it = std::ranges::find_if(settings, std::move(pred));

    if (it != settings.end()) {
        *it = setting.clone();
    } 
    else {
        settings.push_back(setting.clone());
    }
}

void PersistentSettings::save_all() {
    m_dirty = false;
    auto new_buffer = [] static {
        auto lines = buffer 
            | std::views::split('\n')
            | std::views::take_while([] (auto&& range) static noexcept {
                std::string_view line{ range.begin(), range.end() };
                auto i = line.find_first_not_of(whitespace);
                return i == std::string_view::npos || line[i] == '%';
            })
            | std::views::join_with('\n');

        auto result = std::ranges::to<std::string>(lines);

        if (!result.empty() && result.back() != '\n') {
            result += "\n";
        }

        return result;
    }();

    for (auto& p : settings) {
        new_buffer += p->to_string();
        new_buffer += '\n';
    }

    std::ofstream file(file_path);
    if (!file.is_open()) {
        return;
    }
    file << new_buffer;
}

void PersistentSettings::load_all() {
    std::ifstream file(file_path, std::ios::binary);
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

void PersistentSettings::reload_all() {
    settings.clear();
    save_all();
    load_all();
}

auto PersistentSettings::get(const std::string& key) -> Option* {
    auto proj = [] (const auto& ptr) static -> const std::string& {
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
        | std::views::transform(
            [](const auto& line) static noexcept { return std::string_view{ line }; 
          });

    for (std::string_view line : lines) {
        if (line.empty()) {
            continue;
        }

        auto start = line.find_first_not_of(whitespace);
        if (start == std::string_view::npos) {
            continue;
        }
        line.remove_prefix(start);
       
        if (line[start] == '%') {
            line.remove_prefix(1);
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

            while (end > 0 && std::ranges::contains(whitespace, line[end - 1])) {
                --end;
            }

            auto value = std::string{ line.substr(0, end) };
            amend(key, value);
        }
        else {
            float value;
            auto [ptr, ec] = std::from_chars(
                line.data(), std::next(line.data(), std::ssize(line)), value);
            if (ec != std::errc()) {
                continue;
            }
            amend(key, value);
        }
    }
}

std::filesystem::path PersistentSettings::file_path = 
    pop::systems::relative_path() / "persistent.settings";
}


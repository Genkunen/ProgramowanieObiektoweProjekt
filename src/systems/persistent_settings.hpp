#pragma once

#include <string>
#include <vector>
#include <memory>
#include <filesystem>

namespace pop::systems {


class Setting {
public:
    explicit Setting(std::string key);
    virtual ~Setting();

    [[nodiscard]] virtual auto value() const -> std::string = 0;
    [[nodiscard]] virtual auto clone() const -> std::unique_ptr<Setting> = 0;

    [[nodiscard]] auto key() const -> const std::string&;
    [[nodiscard]] auto to_string() const -> std::string;

protected:
    std::string m_key;
    bool is_const{};
};

class SettingNumber final : public Setting {
public:
    explicit SettingNumber(std::string key, float value = 0);

    [[nodiscard]] 
    auto value() const -> std::string override final;
    void set_value(float v);

    [[nodiscard]]
    auto clone() const -> std::unique_ptr<Setting> override final;

private:
    float m_value;
};

class SettingString final : public Setting {
public:
    explicit SettingString(std::string key, std::string value = "");

    [[nodiscard]]
    auto value() const -> std::string override final;
    void set_value(std::string v);

    [[nodiscard]]
    auto clone() const -> std::unique_ptr<Setting> override final;

private:
    std::string m_value;
};

class PersistentSettings {
    PersistentSettings() = delete;

public:
    static std::filesystem::path file_path;
    
    static void amend(std::string key, float value);
    static void amend(std::string key, std::string value);
    static void amend(const Setting& setting);

    static void save();
    static void load();
    static void reload();

    [[nodiscard]]
    static auto get(const std::string& key) -> Setting*;

private:
    static void parse_buffer();

    static constexpr auto whitespace{ std::string_view{ " \t\r\v" } };

    inline static std::vector<std::unique_ptr<Setting>> settings;
    inline static std::string buffer;
};

}



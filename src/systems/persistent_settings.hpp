#pragma once

#include <string>
#include <vector>
#include <memory>

namespace pop::systems {


class Setting {
public:
    explicit Setting(std::string key);
    virtual ~Setting();

    virtual auto value() const -> std::string = 0;
    virtual auto clone() const -> std::unique_ptr<Setting> = 0;

    auto key() const -> const std::string&;
    auto to_string() const -> std::string;

protected:
    std::string m_key;
    bool is_const{};
};

class SettingNumber : public Setting {
public:
    explicit SettingNumber(std::string key, float value = 0);

    auto value() const -> std::string override;
    void set_value(float v);

    auto clone() const -> std::unique_ptr<Setting> override;

private:
    float m_value;
};

class SettingString : public Setting {
public:
    explicit SettingString(std::string key, std::string value = "");

    auto value() const -> std::string override;
    void set_value(std::string v);

    auto clone() const -> std::unique_ptr<Setting> override;

private:
    std::string m_value;
};

class PersistentSettings {
    PersistentSettings() = delete;

public:
    static constexpr auto file_name = "persistent.settings";
    static std::string file_path;
    
    static void amend(std::string key, float value);
    static void amend(std::string key, std::string value);
    static void amend(const Setting& setting);

    static void save();
    static void load();

    static auto get(const std::string& key) -> Setting*;

private:
    static void parse_buffer();
    static constexpr auto whitespace = std::string_view{ " \t\r\v" };
    
    inline static std::vector<std::unique_ptr<Setting>> settings;
    inline static std::string buffer;
};

}



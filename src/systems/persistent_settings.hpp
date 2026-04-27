#pragma once

#include <string>

namespace pop::systems {


class Setting {
public:
    virtual ~Setting();
    virtual auto value() const -> std::string = 0;

    auto key() const -> const std::string&;
    auto to_string() const -> std::string;

protected:
    std::string m_key;
};

class SettingNumber : public Setting {
public:
    explicit SettingNumber(std::string key, float value = 0);
    auto value() const -> std::string override;
    void set_value(float v);

private:
    float m_value;
};

class SettingString : public Setting {
public:
    explicit SettingString(std::string key, std::string value = "");
    auto value() const -> std::string override;
    void set_value(std::string v);

private:
    std::string m_value;
};

class PersistentSettings {

public:
    static void save(Setting& setting);
};

}



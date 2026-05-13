#pragma once

#include <string>
#include <algorithm>
#include <vector>
#include <memory>
#include <filesystem>

namespace pop::systems {

class Option {
public:
    explicit Option(std::string key);
    virtual ~Option();

    [[nodiscard]] virtual auto value() const -> std::string = 0;
    [[nodiscard]] virtual auto clone() const -> std::unique_ptr<Option> = 0;

    [[nodiscard]] auto key() const -> const std::string&;
    [[nodiscard]] auto to_string() const -> std::string;
protected:
    std::string m_key;
};

class OptionNumber final : public Option {
public:
    explicit OptionNumber(std::string key, float value = 0);

    [[nodiscard]] 
    auto value() const -> std::string override final;
    auto real_value() const -> float;
    void set_value(float v);

    [[nodiscard]]
    auto clone() const -> std::unique_ptr<Option> override final;

private:
    float m_value;
};

class OptionString final : public Option {
public:
    explicit OptionString(std::string key, std::string value = "");

    [[nodiscard]]
    auto value() const -> std::string override final;
    void set_value(std::string v);

    [[nodiscard]]
    auto clone() const -> std::unique_ptr<Option> override final;

private:
    std::string m_value;
};

class PersistentSettings {
    PersistentSettings() = delete;

    template <typename T>
    class OptionWrapper {
    public:
        OptionWrapper(T&&);

        auto value() -> T& {
            return m_value;
        }

        void save() {
        }

    private:
        T m_value;
    };

public:
    static std::filesystem::path file_path;
    
    static void amend(std::string key, float value);
    static void amend(std::string key, std::string value);
    static void amend(const Option& setting);

    static void save_all();
    static void load_all();
    static void reload_all();

    [[nodiscard]]
    static auto get(const std::string& key) -> Option*;

    [[nodiscard]]
    inline static auto is_dirty() -> bool { return m_dirty; }

    // global setting variables
    [[nodiscard]]
    inline static auto clear_color() -> std::array<float, 4> {
        std::array<OptionNumber*, 4> opts = {
            dynamic_cast<OptionNumber*>(get("clear_color_r")),
            dynamic_cast<OptionNumber*>(get("clear_color_g")),
            dynamic_cast<OptionNumber*>(get("clear_color_b")),
            dynamic_cast<OptionNumber*>(get("clear_color_a")),
        };

        if (std::ranges::any_of(opts, [](auto* opt) { return opt == nullptr; })) {
            amend("clear_color_r", 0);
            amend("clear_color_g", 0);
            amend("clear_color_b", 0);
            amend("clear_color_a", 1.);
            return { 0, 0, 0, 1 };
        }
        return { 
            opts[0]->real_value(),
            opts[1]->real_value(),
            opts[2]->real_value(),
            opts[3]->real_value(),
        };
    }
    inline static void set_clear_color(std::array<float, 4> value) {
        m_dirty = true;
        amend("clear_color_r", value[0]);
        amend("clear_color_g", value[1]);
        amend("clear_color_b", value[2]);
        amend("clear_color_a", value[3]);
    }
    
private:
    inline static bool m_dirty{};
    static void parse_buffer();

    static constexpr auto whitespace{ std::string_view{ " \t\r\v" } };

    inline static std::vector<std::unique_ptr<Option>> settings;
    inline static std::string buffer;
};

}



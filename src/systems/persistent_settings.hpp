#pragma once

#include <string>
#include <array>
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

    [[nodiscard]]
    inline static auto background_scale() -> float {
        auto opt = dynamic_cast<OptionNumber*>(get("background_scale"));
        if (!opt) {
            amend("background_scale", 1.0f);
            return 1.0f;
        }
        return opt->real_value();
    }
    inline static void set_background_scale(float v) {
        m_dirty = true;
        amend("background_scale", v);
    }

    [[nodiscard]]
    inline static auto background_iterations() -> uint32_t {
        auto opt = dynamic_cast<OptionNumber*>(get("background_iterations"));
        if (!opt) {
            amend("background_iterations", 1);
            return 1;
        }
        return static_cast<uint32_t>(opt->real_value());
    }
    inline static void set_background_iterations(uint32_t v) {
        m_dirty = true;
        amend("background_iterations", v);
    }

    [[nodiscard]]
    inline static auto caustic_intensity() -> float {
        auto opt = dynamic_cast<OptionNumber*>(get("caustic_intensity"));
        if (!opt) {
            amend("caustic_intensity", 1);
            return 1;
        }
        return opt->real_value();
    }
    inline static void set_caustic_intensity(float v) {
        m_dirty = true;
        amend("caustic_intensity", v);
    }

    [[nodiscard]]
    inline static auto ray_intensity() -> float {
        auto opt = dynamic_cast<OptionNumber*>(get("ray_intensity"));
        if (!opt) {
            amend("ray_intensity", 1);
            return 1;
        }
        return opt->real_value();
    }
    inline static void set_ray_intensity(float v) {
        m_dirty = true;
        amend("ray_intensity", v);
    }
    
    [[nodiscard]]
    inline static auto surface_y() -> float {
        auto opt = dynamic_cast<OptionNumber*>(get("surface_y"));
        if (!opt) {
            amend("surface_y", 1);
            return 1;
        }
        return opt->real_value();
    }
    inline static void set_surface_y(float v) {
        m_dirty = true;
        amend("surface_y", v);
    }


    [[nodiscard]]
    inline static auto depth_range() -> float {
        auto opt = dynamic_cast<OptionNumber*>(get("depth_range"));
        if (!opt) {
            amend("depth_range", 1);
            return 1;
        }
        return opt->real_value();
    }
    inline static void set_depth_range(float v) {
        m_dirty = true;
        amend("depth_range", v);
    }
    

    [[nodiscard]]
    inline static auto vignette_size() -> float {
        auto opt = dynamic_cast<OptionNumber*>(get("vignette_size"));
        if (!opt) {
            amend("vignette_size", 1);
            return 1;
        }
        return opt->real_value();
    }
    inline static void set_vignette_size(float v) {
        m_dirty = true;
        amend("vignette_size", v);
    }

private:
    inline static bool m_dirty{};
    static void parse_buffer();

    static constexpr auto whitespace{ std::string_view{ " \t\r\v" } };

    inline static std::vector<std::unique_ptr<Option>> settings;
    inline static std::string buffer;
};

}



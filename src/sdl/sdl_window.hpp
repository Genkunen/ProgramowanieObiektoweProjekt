#pragma once
#include <SDL3/SDL_video.h>
#include <string>
#include "vulkan/vk_prelude.hpp"

namespace pop::sdl {

class SdlWindow {
public:
    SdlWindow(const std::string& title, uint32_t width, uint32_t height);
    ~SdlWindow();

    SdlWindow(const SdlWindow&) = delete;
    SdlWindow(SdlWindow&&) = default;
    SdlWindow& operator=(const SdlWindow&) = delete;
    SdlWindow& operator=(SdlWindow&&) = default;

    [[nodiscard]] auto vulkan_create_raw_surface(vk::Instance instance) const -> vk::SurfaceKHR;
    [[nodiscard]] auto vulkan_window_drawable_extent() const -> vk::Extent2D;
    [[nodiscard]] auto get() const -> SDL_Window*;
private:
    SDL_Window* m_window;
};

}

#pragma once
#include <SDL3/SDL_video.h>
#include <string>
#include "vulkan/vk_prelude.hpp"

namespace pop::sdl {

/// @class SdlWindow
/// @brief Wrapper around an SDL @c SDL_Window.
class SdlWindow {
public:
    /// @brief Creates a window with the given title, width and height.
    /// @param title The title of the window.
    /// @param width The width of the window.
    /// @param height The height of the window.
    SdlWindow(const std::string& title, uint32_t width, uint32_t height);
    ~SdlWindow();

    SdlWindow(const SdlWindow&) = delete;
    SdlWindow(SdlWindow&&) = default;
    SdlWindow& operator=(const SdlWindow&) = delete;
    SdlWindow& operator=(SdlWindow&&) = default;

    /// @brief Creates a Vulkan surface for the window.
    /// @param instance The Vulkan instance to create the surface for.
    /// @return The created Vulkan surface.
    [[nodiscard]] auto vulkan_create_raw_surface(vk::Instance instance) const -> vk::SurfaceKHR;

    /// @brief Returns the drawable extent of the window in pixels.
    /// @return The drawable extent of the window.
    [[nodiscard]] auto vulkan_window_drawable_extent() const -> vk::Extent2D;

    /// @brief Returns the SDL window handle.
    /// @return The SDL window handle.
    [[nodiscard]] auto get_handle()                    const -> SDL_Window*;

private:
    SDL_Window* m_window;
};

} // namespace pop::sdl
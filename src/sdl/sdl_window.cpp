#include "sdl_window.hpp"

#include <SDL3/SDL_vulkan.h>
#include <print>
#include <string>

namespace pop::sdl {

SdlWindow::SdlWindow(const std::string& title, uint32_t width, uint32_t height) {
    m_window = SDL_CreateWindow(title.c_str(), width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

}

SdlWindow::~SdlWindow() {
    SDL_DestroyWindow(m_window);
}

auto SdlWindow::vulkan_create_raw_surface(vk::Instance instance) const -> vk::SurfaceKHR {
    VkSurfaceKHR surface;
    if (!SDL_Vulkan_CreateSurface(m_window, instance, nullptr, &surface)) {
        auto msg = SDL_GetError();
        std::println("SDL Error when creating vk::SurfaceKHR: {}", msg);
        throw std::logic_error(msg);
    }

    return {surface};
}

auto SdlWindow::vulkan_window_drawable_extent() const -> vk::Extent2D {
    vk::Extent2D extent;
    SDL_GetWindowSizeInPixels(m_window, reinterpret_cast<int*>(&extent.width), reinterpret_cast<int*>(&extent.height));
    return extent;
}

auto SdlWindow::get() const -> SDL_Window* const {
    return m_window;
}

} // namespace pop::sdl

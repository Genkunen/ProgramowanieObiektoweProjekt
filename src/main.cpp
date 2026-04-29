#include "sdl/sdl_lib.hpp"
#include "vulkan/renderer/vk_renderer.hpp"
#include "vulkan/vk_context.hpp"
#include "vulkan/vk_swapchain.hpp"

#include <SDL3/SDL.h>

auto sdl_entry_main() -> void {
    auto window = pop::sdl::SdlWindow("ProgramowanieObiektoweProjekt", 1920, 1080);
    auto vulkan_context = pop::vulkan::VulkanContext::create(window);

    auto swapchain = pop::vulkan::VulkanSwapchain::create(window.vulkan_window_drawable_extent(), std::nullopt, true);
    auto renderer = pop::vulkan::renderer::VulkanRenderer::create(std::move(swapchain));

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        renderer.render_frame();
    }

}


auto main() -> int {

    pop::sdl::initializeSdl();
    sdl_entry_main();
    pop::sdl::terminateSdl();
}

#include "sdl/sdl_lib.hpp"
#include "vulkan/renderer/vk_renderer.hpp"
#include "vulkan/vk_context.hpp"
#include "vulkan/vk_swapchain.hpp"
#include <imgui/imgui_layer.hpp>
#include <backends/imgui_impl_sdl3.h>

#include <SDL3/SDL.h>

auto sdl_entry_main() -> void {
    auto window = pop::sdl::SdlWindow("ProgramowanieObiektoweProjekt", 1920, 1080);
    auto vulkan_context = pop::vulkan::VulkanContext::create(window);

    auto swapchain = pop::vulkan::VulkanSwapchain::create(window.vulkan_window_drawable_extent(), std::nullopt, true);
    auto renderer = pop::vulkan::renderer::VulkanRenderer::create(std::move(swapchain));

    auto imgui = pop::imgui::ImGuiLayer(window, renderer.swapchain());

    bool running = true;
    while (running) {
        SDL_Event event;
        bool should_recreate_swapchain = false;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            switch (event.type) {
            case SDL_EVENT_QUIT:
                running = false;
                break;
            case SDL_EVENT_WINDOW_RESIZED:
                should_recreate_swapchain = true;
                break;
                // TODO: Handle minimization events to avoid creating zero-sized swapchain images
            }
        }

        if (should_recreate_swapchain) {
            renderer.handle_surface_invalidation(window.vulkan_window_drawable_extent());
        }

        imgui.begin_frame();

        ImGui::Begin("ImGui");
        ImGui::Text("Frame time: %.3f ms (%.1f FPS)", 
                    1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::End();

        auto render_result = renderer.render_frame(imgui.draw_data());

        if (render_result == pop::vulkan::renderer::RenderResult::SwapchainSuboptimal) {
            renderer.handle_surface_invalidation(window.vulkan_window_drawable_extent());
        }
    }
}

auto main() -> int {
    pop::sdl::initializeSdl();
    sdl_entry_main();
    pop::sdl::terminateSdl();
}

#include "sdl/sdl_lib.hpp"
#include "sphere_geometry_gen.hpp"
#include "systems/ktx2_loader.hpp"
#include "vulkan/renderer/mesh_pool.hpp"
#include "vulkan/renderer/vk_renderer.hpp"
#include "vulkan/vk_context.hpp"
#include "vulkan/vk_swapchain.hpp"

#include <backends/imgui_impl_sdl3.h>
#include <imgui/imgui_layer.hpp>

#include <SDL3/SDL.h>

#include <print>

auto sdl_entry_main() -> void {
    auto window = pop::sdl::SdlWindow("ProgramowanieObiektoweProjekt", 1920, 1080);
    auto vulkan_context = pop::vulkan::VulkanContext::create(window);

    auto swapchain = pop::vulkan::VulkanSwapchain::create(window.vulkan_window_drawable_extent(), std::nullopt, true);
    auto renderer = pop::vulkan::renderer::VulkanRenderer::create(std::move(swapchain));
    auto mesh_pool = pop::vulkan::renderer::MeshPool::create(1048576, 1048576);
    auto ktx2_loader = pop::systems::Ktx2Loader::create();

    std::vector<pop::vulkan::renderer::Mesh> meshes;

    for (int i = 0; i < 10; i++) {
        auto mesh = mesh_pool.load_mesh("../fih.glb");

        meshes.push_back(mesh);
    }

    auto imgui = pop::imgui::ImGuiLayer(window, renderer.swapchain());

    bool running = true;
    static int simulation_object_count = pop::vulkan::renderer::DEFAULT_GPU_DRIVEN_SIM_OBJECT_COUNT;
    glm::vec3 camera_position = {4000.0f, 2000.0f, -20.0f};

    bool is_mouse_dragging = false;
    glm::vec2 mouse_drag_start_pos = {0.0f, 0.0f};

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
            case SDL_EVENT_MOUSE_WHEEL:
                camera_position.z = std::clamp(camera_position.z * std::pow(1.1f, -event.wheel.y), -5000.0f, -5.0f);
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    is_mouse_dragging = true;
                    mouse_drag_start_pos = {event.button.x, event.button.y};
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (event.button.button == SDL_BUTTON_LEFT) is_mouse_dragging = false;
                break;
            case SDL_EVENT_MOUSE_MOTION:
                if (is_mouse_dragging) {
                    float pan_speed = 2.0f * camera_position.z * std::tan(glm::radians(100.0f) / 2.0f) / static_cast<float>(window.vulkan_window_drawable_extent().height);
                    camera_position.x += (event.motion.xrel * pan_speed);
                    camera_position.y += (event.motion.yrel * -pan_speed);

                    camera_position.x = std::clamp(camera_position.x, -1000.0f, 9000.0f);
                    camera_position.y = std::clamp(camera_position.y, -1000.0f, 5000.0f);
                }
                break;
                // TODO: Handle minimization events to avoid creating zero-sized swapchain images
            default: break;
            }
        }

        if (should_recreate_swapchain) {
            renderer.handle_surface_invalidation(window.vulkan_window_drawable_extent());
        }

        imgui.begin_frame();

        ImGui::Begin("ImGui");
        ImGui::Text("Frame time: %.3f ms (%.1f FPS)", 
                    1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

        ImGui::NewLine();

        ImGui::Text("Use mouse wheel to zoom, drag to move camera");
        ImGui::Text("Looking at: (%.1f, %.1f)", camera_position.x, camera_position.y);

        ImGui::NewLine();

        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::InputInt("Object Count", &simulation_object_count)) {
            if (simulation_object_count < 0) simulation_object_count = 0;
            if (simulation_object_count > 10000000) simulation_object_count = 10000000;
        }

        ImGui::SameLine();

        if (ImGui::Button("Apply")) {
            renderer.reset_simulation_object_count(simulation_object_count);
        }

        if (simulation_object_count >= 50000) {
            ImGui::TextColored(ImVec4{1.0f, 1.0f, 0.0f, 1.0f}, "Warning: Using high-poly meshes with a high object count can degrade performance");
        }
        ImGui::End();

        float delta_time = 1.0f / ImGui::GetIO().Framerate;

        auto render_result = renderer.render_frame(mesh_pool, meshes, imgui.draw_data(), delta_time, camera_position);

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

#include "sdl/sdl_lib.hpp"
#include "sphere_geometry_gen.hpp"
#include "systems/ktx2_loader.hpp"
#include "systems/persistent_settings.hpp"
#include "systems/simulation_data_csv_writer.hpp"
#include "vulkan/renderer/mesh_pool.hpp"
#include "vulkan/renderer/vk_renderer.hpp"
#include "vulkan/vk_context.hpp"
#include "vulkan/vk_device_fault_dump.hpp"
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

    for (int i = 0; i < 3; i++) {
        auto mesh = mesh_pool.load_mesh("../fih.glb");

        meshes.push_back(mesh);
    }

    auto imgui = pop::imgui::ImGuiLayer(window, renderer.swapchain());

    bool running = true;
    static int simulation_object_count = pop::vulkan::renderer::DEFAULT_GPU_DRIVEN_SIM_OBJECT_COUNT;
    static float simulation_water_current_strength = pop::vulkan::renderer::DEFAULT_GPU_DRIVEN_SIM_WATER_CURRENT_STRENGTH;
    glm::vec3 camera_position = {4000.0f, 2000.0f, -20.0f};

    bool is_mouse_dragging = false;

    pop::systems::PersistentSettings::load_all();
    
    struct {
        struct {
            float r, g, b, a;
        } clr_colors;
        float background_scale;
        int background_iterations;
        float caustic_intensity;
        float ray_intensity;
        float surface_y;
        float depth_range;
        float vignette_size;
    } imgui_variables{};

    auto populate_imgui_variables = [&imgui_variables] {
        auto clrs = pop::systems::PersistentSettings::clear_color();
        imgui_variables.clr_colors = { clrs[0], clrs[1], clrs[2], clrs[3] };
        imgui_variables.background_iterations = pop::systems::PersistentSettings::background_iterations();
        imgui_variables.background_scale = pop::systems::PersistentSettings::background_scale();
        imgui_variables.caustic_intensity = pop::systems::PersistentSettings::caustic_intensity();
        imgui_variables.ray_intensity = pop::systems::PersistentSettings::ray_intensity();
        imgui_variables.surface_y = pop::systems::PersistentSettings::surface_y();
        imgui_variables.depth_range = pop::systems::PersistentSettings::depth_range();
        imgui_variables.vignette_size = pop::systems::PersistentSettings::vignette_size();
    };
    populate_imgui_variables();

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
                if (ImGui::GetIO().WantCaptureMouse) break;

                camera_position.z = std::clamp(camera_position.z * std::pow(1.1f, -event.wheel.y), -2000.0f, -5.0f);
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (ImGui::GetIO().WantCaptureMouse) break;

                if (event.button.button == SDL_BUTTON_LEFT) {
                    is_mouse_dragging = true;
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (ImGui::GetIO().WantCaptureMouse) break;

                if (event.button.button == SDL_BUTTON_LEFT) is_mouse_dragging = false;
                break;
            case SDL_EVENT_MOUSE_MOTION:
                if (ImGui::GetIO().WantCaptureMouse) break;

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

        if (ImGui::Button("Apply##1")) {
            renderer.reset_simulation_object_count(simulation_object_count);
        }

        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputFloat("Water Current Strength", &simulation_water_current_strength);

        ImGui::SameLine();

        if (ImGui::Button("Apply##2")) {
            renderer.set_water_current_strength(simulation_water_current_strength);
        }

        {
            ImGuiColorEditFlags colorEdiFlags =
                ImGuiColorEditFlags_NoSmallPreview |
                ImGuiColorEditFlags_NoSidePreview |
                ImGuiColorEditFlags_PickerHueBar |
                ImGuiColorEditFlags_NoTooltip |
                ImGuiColorEditFlags_NoAlpha;
            auto& clrs = imgui_variables.clr_colors;
            if (ImGui::ColorPicker4("Background Color", (float*)&imgui_variables.clr_colors, colorEdiFlags)) {
                pop::systems::PersistentSettings::set_clear_color({ clrs.r, clrs.g, clrs.b, 1.f });
            }
        }

        {
            auto& scale = imgui_variables.background_scale;
            if (ImGui::SliderFloat("Background Scale", &scale, 0.1f, 1.5)) {
                pop::systems::PersistentSettings::set_background_scale(scale);
            }
        }
        {
            auto& caustic_int = imgui_variables.caustic_intensity;
            if (ImGui::SliderFloat("Caustic Intensity", &caustic_int, 0.1f, 1000.0f)) {
                pop::systems::PersistentSettings::set_caustic_intensity(caustic_int);
            }
        }
        {
            auto& ray_int = imgui_variables.ray_intensity;
            if (ImGui::SliderFloat("Ray Intensity", &ray_int, 0.01f, 1.f)) {
                pop::systems::PersistentSettings::set_ray_intensity(ray_int);
            }
        }
        {
            auto& srfc = imgui_variables.surface_y;
            if (ImGui::SliderFloat("Surface Y", &srfc, -5000.f, 20000.f)) {
                pop::systems::PersistentSettings::set_surface_y(srfc);
            }
        }
        {
            auto& depth_range = imgui_variables.depth_range;
            if (ImGui::SliderFloat("Depth Range", &depth_range, 0.f, 15000.f)) {
                pop::systems::PersistentSettings::set_depth_range(depth_range);
            }
        }
        {
            auto& vignette_size = imgui_variables.vignette_size;
            if (ImGui::SliderFloat("Vignette Size", &vignette_size, 0.1f, 30000.f)) {
                pop::systems::PersistentSettings::set_vignette_size(vignette_size);
            }
        }
        {
            auto& val = imgui_variables.background_iterations;
            if (ImGui::InputInt("Background Iterations Count", &val)) {
                pop::systems::PersistentSettings::set_background_iterations(val);
            }
        }

        if (ImGui::Button("Export Simulation Data to CSV File")) {
            auto data = renderer.export_simulation_data();
            pop::systems::SimulationDataCsvWriter::write_to_file(data, "./simulation_data.csv");
        }
        ImGui::End();

        float delta_time = 1.0f / ImGui::GetIO().Framerate;

        try {
            auto render_result = renderer.render_frame(mesh_pool, meshes, imgui.draw_data(), delta_time, camera_position);

            if (render_result == pop::vulkan::renderer::RenderResult::SwapchainSuboptimal) {
                renderer.handle_surface_invalidation(window.vulkan_window_drawable_extent());
            }
        } catch (const vk::SystemError& e) {
            std::string error_message = "A Vulkan system error has been thrown during rendering:\n    " + std::string{ e.what() } + "\n\n";

            if (e.code() == vk::Result::eErrorDeviceLost) {
                if (pop::vulkan::VulkanDeviceFaultDump::is_dumping_supported()) {
                    auto fault_dump = pop::vulkan::VulkanDeviceFaultDump::dump_device_fault_info();
                    error_message += fault_dump.format_as_fault_message();
                } else {
                    error_message += "Debugging isn't enabled or VK_EXT_device_fault is not supported, no extra debug information available.";
                }
            }

            std::println("{}", error_message);
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan System Error", error_message.c_str(), window.get());
            running = false;
        }

        if (pop::systems::PersistentSettings::is_dirty()) {
            pop::systems::PersistentSettings::save_all();
        }
    }
}

auto main() -> int {
    pop::sdl::initializeSdl();
    try {
        sdl_entry_main();
    } catch (const std::exception& e) {
        std::string error_message = "An error occurred during program execution:\n    " + std::string{ e.what() };
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", error_message.c_str(), nullptr);
    }
    pop::sdl::terminateSdl();
}

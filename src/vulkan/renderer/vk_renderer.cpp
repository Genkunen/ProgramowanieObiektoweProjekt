#include "vk_renderer.hpp"

#include "shaders/shared_consts.hpp"
#include "systems/systems.hpp"
#include "vulkan/vk_context.hpp"
#include "vulkan/renderer/render_graph/render_graph.hpp"

#include "simulation_render_graph_passes.hpp"
#include <imgui.h>
#include <print>
#include <random>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace pop::vulkan::renderer {

VulkanRenderer::VulkanRenderer(
        VulkanSwapchain&& swapchain, render_graph::RenderGraph<SimulationRenderState>&& render_graph, render_graph::PassIndex mesh_upload_pass_index,
        VulkanBuffer&& simulation_objects_buffer_pp1, VulkanBuffer&& simulation_objects_buffer_pp2,
        VulkanBuffer&& indirect_draw_commands_buffer, VulkanBuffer&& drawlocal_instance_ids_buffer, VulkanBuffer&& instance_data_buffer,
        VulkanImage&& main_render_target, VulkanImage&& depth_buffer,
        std::vector<FrameInFlight>&& frames_in_flight)
    : m_swapchain(std::move(swapchain)), m_render_graph(std::move(render_graph)), m_mesh_upload_pass_index(mesh_upload_pass_index),
        m_simulation_objects_buffer_pp1(std::move(simulation_objects_buffer_pp1)), m_simulation_objects_buffer_pp2(std::move(simulation_objects_buffer_pp2)),
        m_indirect_draw_commands_buffer(std::move(indirect_draw_commands_buffer)), m_drawlocal_instance_ids_buffer(std::move(drawlocal_instance_ids_buffer)),
        m_instance_data_buffer(std::move(instance_data_buffer)), m_main_render_target(std::move(main_render_target)), m_depth_buffer(std::move(depth_buffer)),
        m_frames_in_flight(std::move(frames_in_flight)) {}

VulkanRenderer::~VulkanRenderer() {
    VulkanContext::get().vk_device().waitIdle();
}

// TODO: temporary view
inline glm::mat4 build_projview(glm::vec3 pos, float aspect_ratio) {
    // near and far swapped for reverse Z
    glm::mat4 proj = glm::perspectiveLH_ZO(glm::radians(100.0f), aspect_ratio, 1000.0f,  0.01f);

    // must flip Y for Vulkan
    proj[1][1] *= -1.0f;

    glm::mat4 view = glm::lookAt(pos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    return proj * view;
}

auto VulkanRenderer::create(VulkanSwapchain&& swapchain) -> VulkanRenderer {
    // ---- Per-pending-frame Data -----------------------------------------------------------------------------------------------------------------------------

    std::vector<FrameInFlight> frames_in_flight;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        auto command_pool_create_info = vk::CommandPoolCreateInfo()
            .setQueueFamilyIndex(VulkanContext::get().vk_graphics_queue_family());
        auto signaled_fence_create_info = vk::FenceCreateInfo()
            .setFlags(vk::FenceCreateFlagBits::eSignaled);
        auto default_semaphore_create_info = vk::SemaphoreCreateInfo();


        auto command_pool = VulkanContext::get().vk_device().createCommandPool(command_pool_create_info);
        auto frame_command_buffer = std::move(VulkanContext::get().vk_device().allocateCommandBuffers({*command_pool, vk::CommandBufferLevel::ePrimary, 1})[0]);
        auto frame_finished_fence = VulkanContext::get().vk_device().createFence(signaled_fence_create_info);
        auto image_acquired_semaphore = VulkanContext::get().vk_device().createSemaphore(default_semaphore_create_info);

        auto simulation_data_buffer = VulkanBuffer::builder()
            .set_size(sizeof(shaders::SimulationData))
            .set_usage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress)
            .set_memory_usage(vma::MemoryUsage::eAutoPreferDevice)
            .map_for_sequential_write()
            .build();

        auto mesh_allocations_table_staging_buffer = VulkanBuffer::builder()
            .set_size(64 * sizeof(shaders::MeshAllocationData))
            .set_usage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress)
            .set_memory_usage(vma::MemoryUsage::eAutoPreferDevice)
            .map_for_sequential_write()
            .build();

        frames_in_flight.emplace_back(std::move(frame_finished_fence), std::move(image_acquired_semaphore), std::move(command_pool),
            std::move(frame_command_buffer), std::move(simulation_data_buffer), std::move(mesh_allocations_table_staging_buffer));
    }

    // ---- Render Graph Build ---------------------------------------------------------------------------------------------------------------------------------

    render_graph::RenderGraph<SimulationRenderState> render_graph;
    render_graph::PassIndex mesh_upload_pass_index = render_graph.add_pass(std::make_unique<UploadMeshInfoPass>(UploadMeshInfoPass::create()));
    render_graph.add_pass(std::make_unique<IndirectDrawCommandsClearPass>(IndirectDrawCommandsClearPass::create()));
    render_graph.add_pass(std::make_unique<SimulationStepPass>(SimulationStepPass::create()));
    render_graph.add_pass(std::make_unique<IndirectDrawCommandsInstanceCountBuildPass>(IndirectDrawCommandsInstanceCountBuildPass::create()));
    render_graph.add_pass(std::make_unique<IndirectDrawCommandsFirstInstanceBuildPass>(IndirectDrawCommandsFirstInstanceBuildPass::create()));
    render_graph.add_pass(std::make_unique<InstanceBufferBuildPass>(InstanceBufferBuildPass::create()));
    render_graph.add_pass(std::make_unique<FishTankRenderPass>(FishTankRenderPass::create()));
    render_graph.add_pass(std::make_unique<ImGuiRenderPass>(ImGuiRenderPass::create()));
    render_graph.add_pass(std::make_unique<BlitMainImageToSwapchainPass>(BlitMainImageToSwapchainPass::create()));


    // ---- Render Targets -------------------------------------------------------------------------------------------------------------------------------------

    auto main_render_target = VulkanImage::builder()
        .set_extent(vk::Extent3D(swapchain.image_extent(), 1))
        .set_format(vk::Format::eR16G16B16A16Sfloat)
        .set_initial_layout(vk::ImageLayout::eUndefined)
        .set_memory_usage(vma::MemoryUsage::eAutoPreferDevice)
        .set_mip_levels(1)
        .set_tiling(vk::ImageTiling::eOptimal)
        .set_type(vk::ImageType::e2D)
        .set_usage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc)
        .build();

    auto depth_buffer = VulkanImage::builder()
        .set_extent(vk::Extent3D(swapchain.image_extent(), 1))
        .set_format(vk::Format::eD32Sfloat)
        .set_initial_layout(vk::ImageLayout::eUndefined)
        .set_memory_usage(vma::MemoryUsage::eAutoPreferDevice)
        .set_mip_levels(1)
        .set_tiling(vk::ImageTiling::eOptimal)
        .set_type(vk::ImageType::e2D)
        .set_usage(vk::ImageUsageFlagBits::eDepthStencilAttachment)
        .build();

    // ---- Simulation Data Buffers ----------------------------------------------------------------------------------------------------------------------------

    auto simulation_objects_buffer_pp1 = VulkanBuffer::builder()
        .set_size(DEFAULT_GPU_DRIVEN_SIM_OBJECT_COUNT * sizeof(shaders::SimulationObject))
        .set_usage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .set_memory_usage(vma::MemoryUsage::eAutoPreferDevice)
        .map_for_sequential_write()
        .build();

    auto simulation_objects_buffer_pp2 = VulkanBuffer::builder()
        .set_size(DEFAULT_GPU_DRIVEN_SIM_OBJECT_COUNT * sizeof(shaders::SimulationObject))
        .set_usage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .set_memory_usage(vma::MemoryUsage::eAutoPreferDevice)
        .map_for_sequential_write()
        .build();

    // ---- Indirect Draw Buffers ------------------------------------------------------------------------------------------------------------------------------

    auto indirect_draw_commands_buffer = VulkanBuffer::builder()
        .set_size(shader_consts::MAX_DRAW_COMMANDS * sizeof(vk::DrawIndexedIndirectCommand))
        .set_usage(vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .set_memory_usage(vma::MemoryUsage::eAutoPreferDevice)
        .build();

    auto drawlocal_instance_ids_buffer = VulkanBuffer::builder()
        .set_size(DEFAULT_GPU_DRIVEN_SIM_OBJECT_COUNT * sizeof(uint32_t))
        .set_usage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .set_memory_usage(vma::MemoryUsage::eAutoPreferDevice)
        .build();

    auto instance_data_buffer = VulkanBuffer::builder()
        .set_size(DEFAULT_GPU_DRIVEN_SIM_OBJECT_COUNT * sizeof(shaders::PreparedSimulationObject))
        .set_usage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .set_memory_usage(vma::MemoryUsage::eAutoPreferDevice)
        .build();

    return VulkanRenderer{ std::move(swapchain), std::move(render_graph), mesh_upload_pass_index,
        std::move(simulation_objects_buffer_pp1), std::move(simulation_objects_buffer_pp2), std::move(indirect_draw_commands_buffer), std::move(drawlocal_instance_ids_buffer), std::move(instance_data_buffer),
        std::move(main_render_target), std::move(depth_buffer), std::move(frames_in_flight) };
}

auto VulkanRenderer::render_frame(MeshPool& mesh_pool, const std::span<const Mesh>& meshes, ImDrawData* draw_data, float delta_time) -> RenderResult {
    auto& frame = m_frames_in_flight[m_current_frame];
    auto& device = VulkanContext::get().vk_device();
    std::ignore = device.waitForFences(*frame.frame_finished_fence, true, std::numeric_limits<uint64_t>::max());

    auto swapchain_acquire_result = m_swapchain.vk_swapchain().acquireNextImage(std::numeric_limits<uint64_t>::max(), frame.image_acquired_semaphore, {});

    if (swapchain_acquire_result.result == vk::Result::eSuboptimalKHR || swapchain_acquire_result.result == vk::Result::eErrorOutOfDateKHR) {
        return RenderResult::SwapchainSuboptimal;
    }

    if (swapchain_acquire_result.result != vk::Result::eSuccess) {
        std::println("ERROR: Failed to acquire swapchain image: {}", vk::to_string(swapchain_acquire_result.result));
        throw std::runtime_error("Failed to acquire swapchain image!");
    }

    auto& swapchain_image = m_swapchain.images()[*swapchain_acquire_result];

    device.resetFences(*frame.frame_finished_fence);

    frame.command_pool.reset();

    // ---- Fill simulation data buffer ------------------------------------------------------------------------------------------------------------------------

    float aspect_ratio = static_cast<float>(m_main_render_target.extent().width) / static_cast<float>(m_main_render_target.extent().height);
    glm::mat4 projview = build_projview({0.0f, 0.0f, 20.0f}, aspect_ratio);
    float time_since_start = std::chrono::duration<float>(std::chrono::high_resolution_clock::now() - m_start_timepoint).count();

    shaders::SimulationData simulation_data = shaders::SimulationData{projview, delta_time, time_since_start};

    memcpy(frame.simulation_data_buffer.memory_host_ptr(), &simulation_data, sizeof(shaders::SimulationData));

    // ---- Preinitialize and/or refit simulation if needeed ---------------------------------------------------------------------------------------------------

    if (m_gpu_driven_sim_needs_refit || m_gpu_driven_sim_needs_preinit) device.waitIdle(); // to avoid hazard with CS reading the buffer, this is rare anyway so it's fine

    if (m_gpu_driven_sim_needs_refit) {
        m_gpu_driven_sim_needs_refit = false;
        refit_simulation_buffers();
    }

    if (m_gpu_driven_sim_needs_preinit) {
        m_gpu_driven_sim_needs_preinit = false;
        preinitialize_simulation(meshes);
    }

    // ---- Fill draw commands mesh parameters if needed -------------------------------------------------------------------------------------------------------

    if (mesh_pool.mesh_allocations_table_generation() > m_indirect_draw_commands_mesh_params_generation) {
        memcpy(frame.mesh_allocations_table_staging_buffer.memory_host_ptr(), mesh_pool.mesh_allocations().data(), mesh_pool.mesh_allocations().size() * sizeof(shaders::MeshAllocationData));
        m_indirect_draw_commands_mesh_params_generation = mesh_pool.mesh_allocations_table_generation();
        m_render_graph.get_pass_by_id(m_mesh_upload_pass_index).enable();
    } else {
        m_render_graph.get_pass_by_id(m_mesh_upload_pass_index).disable();
    }

    // ---- Command recording begin ----------------------------------------------------------------------------------------------------------------------------

    vk::CommandBufferBeginInfo command_buffer_begin_info = vk::CommandBufferBeginInfo()
        .setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    auto& command_buffer = frame.frame_command_buffer;

    command_buffer.begin(command_buffer_begin_info);

    render_graph::PassResources pass_resources{};
    pass_resources.inject_buffer(render_graph::BufferResourceIdentifier::FrameLocalMeshInfoStagingBuffer, frame.mesh_allocations_table_staging_buffer);
    pass_resources.inject_buffer(render_graph::BufferResourceIdentifier::SimulationDrawIndirectCommands, m_indirect_draw_commands_buffer);
    pass_resources.inject_buffer(render_graph::BufferResourceIdentifier::DrawCommandsObjectInstanceOffsets, m_drawlocal_instance_ids_buffer);
    pass_resources.inject_buffer(render_graph::BufferResourceIdentifier::FrameLocalSimulationData, frame.simulation_data_buffer);
    pass_resources.inject_buffer(render_graph::BufferResourceIdentifier::SimulationObjects, get_simulation_objects_src_buffer());
    pass_resources.inject_buffer(render_graph::BufferResourceIdentifier::SimulationNextObjects, get_simulation_objects_dst_buffer());
    pass_resources.inject_buffer(render_graph::BufferResourceIdentifier::ObjectsInstanceBuffer, m_instance_data_buffer);

    pass_resources.inject_image(render_graph::ImageResourceIdentifier::MainRenderTarget, m_main_render_target);
    pass_resources.inject_image(render_graph::ImageResourceIdentifier::DepthBuffer, m_depth_buffer);

    SimulationRenderState simulation_render_state = {
        .mesh_pool = mesh_pool,
        .current_swapchain_image = swapchain_image,
        .object_count = m_gpu_driven_sim_object_count,
        .imgui_draw_data = draw_data
    };

    m_render_graph.execute(command_buffer, simulation_render_state, pass_resources);

    frame.frame_command_buffer.end();

    auto wait_semaphore_submit_info = vk::SemaphoreSubmitInfo()
        .setSemaphore(*frame.image_acquired_semaphore)
        .setStageMask(vk::PipelineStageFlagBits2::eTransfer);

    auto signal_semaphore_submit_info = vk::SemaphoreSubmitInfo()
        .setSemaphore(*swapchain_image.image_present_semaphore())
        .setStageMask(vk::PipelineStageFlagBits2::eAllCommands);

    auto command_buffer_info = vk::CommandBufferSubmitInfo{}
        .setCommandBuffer(frame.frame_command_buffer);

    auto submit_info = vk::SubmitInfo2()
        .setCommandBufferInfos(command_buffer_info)
        .setWaitSemaphoreInfos(wait_semaphore_submit_info)
        .setSignalSemaphoreInfos(signal_semaphore_submit_info);

    VulkanContext::get().vk_graphics_queue().submit2(submit_info, *frame.frame_finished_fence);

    auto present_info = vk::PresentInfoKHR()
        .setWaitSemaphores(*swapchain_image.image_present_semaphore())
        .setSwapchains(*m_swapchain.vk_swapchain())
        .setImageIndices(swapchain_acquire_result.value);

    m_current_frame = (m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
    swap_simulation_objects_buffer_rw_direction();

    auto present_result = VulkanContext::get().vk_present_queue().presentKHR(present_info);

    if (present_result == vk::Result::eErrorOutOfDateKHR || present_result == vk::Result::eSuboptimalKHR) {
        return RenderResult::SwapchainSuboptimal;
    }

    if (present_result != vk::Result::eSuccess) {
        std::println("ERROR: Failed to submit present: {}", vk::to_string(swapchain_acquire_result.result));
        throw std::runtime_error("Failed to submit present!");
    }

    return RenderResult::Ok;
}

auto VulkanRenderer::handle_surface_invalidation(vk::Extent2D new_window_extent) -> void {
    VulkanContext::get().vk_device().waitIdle();

    m_main_render_target = VulkanImage::builder()
        .set_extent(vk::Extent3D(new_window_extent, 1))
        .set_format(vk::Format::eR16G16B16A16Sfloat)
        .set_initial_layout(vk::ImageLayout::eUndefined)
        .set_memory_usage(vma::MemoryUsage::eAutoPreferDevice)
        .set_mip_levels(1)
        .set_tiling(vk::ImageTiling::eOptimal)
        .set_type(vk::ImageType::e2D)
        .set_usage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc)
        .build();

    m_depth_buffer = VulkanImage::builder()
        .set_extent(vk::Extent3D(new_window_extent, 1))
        .set_format(vk::Format::eD32Sfloat)
        .set_initial_layout(vk::ImageLayout::eUndefined)
        .set_memory_usage(vma::MemoryUsage::eAutoPreferDevice)
        .set_mip_levels(1)
        .set_tiling(vk::ImageTiling::eOptimal)
        .set_type(vk::ImageType::e2D)
        .set_usage(vk::ImageUsageFlagBits::eDepthStencilAttachment)
        .build();

    m_swapchain = VulkanSwapchain::create(new_window_extent, std::move(m_swapchain), true);

    m_render_graph.reset_tracking_for_image(render_graph::ImageResourceIdentifier::MainRenderTarget);
    m_render_graph.reset_tracking_for_image(render_graph::ImageResourceIdentifier::DepthBuffer);
}

auto VulkanRenderer::swapchain() const -> const VulkanSwapchain& {
    return m_swapchain;
}
auto VulkanRenderer::reset_simulation_object_count(uint32_t new_count) -> void {
    m_gpu_driven_sim_object_count = new_count;
    m_gpu_driven_sim_needs_preinit = true;
    m_gpu_driven_sim_needs_refit = true;
}

// TODO: remove or move somewhere else later
inline glm::vec2 random_ndc() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    return glm::vec2(dist(gen), dist(gen));
}

inline uint32_t object_random_seed() {
    return static_cast<uint32_t>(rand());
}

auto VulkanRenderer::refit_simulation_buffers() -> void {
    m_simulation_objects_buffer_pp1 = VulkanBuffer::builder()
        .set_size(m_gpu_driven_sim_object_count * sizeof(shaders::SimulationObject))
        .set_usage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .set_memory_usage(vma::MemoryUsage::eAutoPreferDevice)
        .map_for_sequential_write()
        .build();

    m_simulation_objects_buffer_pp2 = VulkanBuffer::builder()
        .set_size(m_gpu_driven_sim_object_count * sizeof(shaders::SimulationObject))
        .set_usage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .set_memory_usage(vma::MemoryUsage::eAutoPreferDevice)
        .map_for_sequential_write()
        .build();

    m_drawlocal_instance_ids_buffer = VulkanBuffer::builder()
        .set_size(m_gpu_driven_sim_object_count * sizeof(uint32_t))
        .set_usage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .set_memory_usage(vma::MemoryUsage::eAutoPreferDevice)
        .build();

    m_instance_data_buffer = VulkanBuffer::builder()
        .set_size(m_gpu_driven_sim_object_count * sizeof(shaders::PreparedSimulationObject))
        .set_usage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .set_memory_usage(vma::MemoryUsage::eAutoPreferDevice)
        .build();
}

auto VulkanRenderer::preinitialize_simulation(const std::span<const Mesh>& meshes) -> void {
    auto dst_ptr = reinterpret_cast<shaders::SimulationObject*>(get_simulation_objects_src_buffer().memory_host_ptr());
    for (uint32_t i = 0; i < m_gpu_driven_sim_object_count; ++i) {
        dst_ptr[i].mesh_index = meshes[i % meshes.size()].allocation_index;
        dst_ptr[i].position = random_ndc() * glm::vec2(40.0f, 20.0f);
        dst_ptr[i].velocity = random_ndc() * 5.0f;
        dst_ptr[i].randseed = object_random_seed();
    }
}

auto VulkanRenderer::swap_simulation_objects_buffer_rw_direction() -> void {
    if (m_simulation_objects_buffer_rw_direction == SimulationObjectBufferReadWriteDirection::e1to2) {
        m_simulation_objects_buffer_rw_direction = SimulationObjectBufferReadWriteDirection::e2to1;
    } else {
        m_simulation_objects_buffer_rw_direction = SimulationObjectBufferReadWriteDirection::e1to2;
    }
}
auto VulkanRenderer::get_simulation_objects_src_buffer() -> VulkanBuffer& {
    if (m_simulation_objects_buffer_rw_direction == SimulationObjectBufferReadWriteDirection::e1to2) {
        return m_simulation_objects_buffer_pp1;
    } else {
        return m_simulation_objects_buffer_pp2;
    }
}
auto VulkanRenderer::get_simulation_objects_dst_buffer() -> VulkanBuffer& {
    if (m_simulation_objects_buffer_rw_direction == SimulationObjectBufferReadWriteDirection::e1to2) {
        return m_simulation_objects_buffer_pp2;
    } else {
        return m_simulation_objects_buffer_pp1;
    }
}
} // namespace pop::vulkan::renderer

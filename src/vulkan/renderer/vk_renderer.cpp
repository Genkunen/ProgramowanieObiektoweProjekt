#include "vk_renderer.hpp"

#include "pipelines_push_constants.hpp"
#include "shaders/shared_consts.hpp"
#include "systems/filesystem.hpp"
#include "vulkan/spirv_code.hpp"
#include "vulkan/vk_context.hpp"
#include "vulkan/vk_pipeline_barriers.hpp"

#include <backends/imgui_impl_vulkan.h>
#include <bits/os_defines.h>
#include <imgui.h>
#include <print>
#include <random>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace pop::vulkan::renderer {

VulkanRenderer::VulkanRenderer(
        VulkanSwapchain&& swapchain, VulkanPipelineLayout&& triangle_pipeline_layout, VulkanGraphicsPipeline&& triangle_pipeline,
        VulkanPipelineLayout&& upload_meshes_cs_layout, VulkanComputePipeline&& upload_meshes_cs,
        VulkanPipelineLayout&& clear_instance_count_cs_layout, VulkanComputePipeline&& clear_instance_count_cs,
        VulkanPipelineLayout&& simulation_step_cs_layout, VulkanComputePipeline&& simulation_step_cs,
        VulkanPipelineLayout&& build_indirect_instance_count_cs_layout, VulkanComputePipeline&& build_indirect_instance_count_cs,
        VulkanPipelineLayout&& build_indirect_first_instance_cs_layout, VulkanComputePipeline&& build_indirect_first_instance_cs,
        VulkanPipelineLayout&& build_instance_buffer_cs_layout, VulkanComputePipeline&& build_instance_buffer_cs,
        VulkanBuffer&& simulation_objects_buffer_pp1, VulkanBuffer&& simulation_objects_buffer_pp2,
        VulkanBuffer&& indirect_draw_commands_buffer, VulkanBuffer&& drawlocal_instance_ids_buffer, VulkanBuffer&& instance_data_buffer,
        VulkanImage&& main_render_target, VulkanImage&& depth_buffer,
        std::vector<FrameInFlight>&& frames_in_flight)
    : m_swapchain(std::move(swapchain)), m_triangle_pipeline_layout(std::move(triangle_pipeline_layout)), m_triangle_pipeline(std::move(triangle_pipeline)),
        m_upload_meshes_cs_layout(std::move(upload_meshes_cs_layout)), m_upload_meshes_cs(std::move(upload_meshes_cs)),
        m_clear_instance_count_cs_layout(std::move(clear_instance_count_cs_layout)), m_clear_instance_count_cs(std::move(clear_instance_count_cs)),
        m_simulation_step_cs_layout(std::move(simulation_step_cs_layout)), m_simulation_step_cs(std::move(simulation_step_cs)),
        m_build_indirect_instance_count_cs_layout(std::move(build_indirect_instance_count_cs_layout)), m_build_indirect_instance_count_cs(std::move(build_indirect_instance_count_cs)),
        m_build_indirect_first_instance_cs_layout(std::move(build_indirect_first_instance_cs_layout)), m_build_indirect_first_instance_cs(std::move(build_indirect_first_instance_cs)),
        m_build_instance_buffer_cs_layout(std::move(build_instance_buffer_cs_layout)), m_build_instance_buffer_cs(std::move(build_instance_buffer_cs)),
        m_simulation_objects_buffer_pp1(std::move(simulation_objects_buffer_pp1)), m_simulation_objects_buffer_pp2(std::move(simulation_objects_buffer_pp2)),
        m_indirect_draw_commands_buffer(std::move(indirect_draw_commands_buffer)), m_drawlocal_instance_ids_buffer(std::move(drawlocal_instance_ids_buffer)),
        m_instance_data_buffer(std::move(instance_data_buffer)), m_main_render_target(std::move(main_render_target)), m_depth_buffer(std::move(depth_buffer)),
        m_frames_in_flight(std::move(frames_in_flight)) {}

VulkanRenderer::~VulkanRenderer() {
    VulkanContext::get().vk_device().waitIdle();
}

inline vk::Offset3D to_offset3d(const vk::Extent3D& extent) {
    return vk::Offset3D{
        static_cast<int32_t>(extent.width),
        static_cast<int32_t>(extent.height),
        static_cast<int32_t>(extent.depth)
    };
}

inline uint32_t div_ceil(uint32_t a, uint32_t b) {
    return (a + b - 1) / b;
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

    // ---- Pipelines ------------------------------------------------------------------------------------------------------------------------------------------

    auto triangle_pipeline_layout = VulkanPipelineLayout::builder()
        .add_push_constant_range(0, 24, vk::ShaderStageFlagBits::eVertex)
        .build();

    auto triangle_pipeline_shader_code = SpirvCode::load_from_file(filesystem::relative_path() / "spirv/simulation_entity.spv");

    auto triangle_pipeline = VulkanGraphicsPipeline::builder()
        .set_pipeline_layout(triangle_pipeline_layout)
        .add_shader(triangle_pipeline_shader_code, vk::ShaderStageFlagBits::eVertex)
        .add_shader(triangle_pipeline_shader_code, vk::ShaderStageFlagBits::eFragment)
        .set_input_topology(vk::PrimitiveTopology::eTriangleList)
        .set_rasterizer_polygon_mode(vk::PolygonMode::eFill)
        .set_rasterizer_cull_mode(vk::CullModeFlagBits::eFront, vk::FrontFace::eCounterClockwise)
        .set_rasterizer_line_width(1.0f)
        .disable_multisampling()
        .enable_depth_test(true)
        .add_rendering_attachment(
            vk::PipelineColorBlendAttachmentState()
                .setBlendEnable(false)
                .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA),
            vk::Format::eR16G16B16A16Sfloat
        )
        .set_depth_attachment_format(vk::Format::eD32Sfloat)
        .build();

    // ---- Compute Pipelines ----------------------------------------------------------------------------------------------------------------------------------

    auto upload_meshes_cs_layout = VulkanPipelineLayout::builder()
        .add_push_constant_range(0, sizeof(UploadMeshesCSPushConstants), vk::ShaderStageFlagBits::eCompute)
        .build();

    auto clear_instance_count_cs_layout = VulkanPipelineLayout::builder()
        .add_push_constant_range(0, sizeof(ClearInstanceCountCSPushConstants), vk::ShaderStageFlagBits::eCompute)
        .build();

    auto simulation_step_cs_layout = VulkanPipelineLayout::builder()
        .add_push_constant_range(0, sizeof(SimulationStepCSPushConstants), vk::ShaderStageFlagBits::eCompute)
        .build();

    auto build_indirect_instance_count_cs_layout = VulkanPipelineLayout::builder()
        .add_push_constant_range(0, sizeof(BuildIndirectInstanceCountCSPushConstants), vk::ShaderStageFlagBits::eCompute)
        .build();

    auto build_indirect_first_instance_cs_layout = VulkanPipelineLayout::builder()
        .add_push_constant_range(0, sizeof(BuildIndirectFirstInstanceCSPushConstants), vk::ShaderStageFlagBits::eCompute)
        .build();

    auto build_instance_buffer_cs_layout = VulkanPipelineLayout::builder()
        .add_push_constant_range(0, sizeof(BuildInstanceBufferCSPushConstants), vk::ShaderStageFlagBits::eCompute)
        .build();

    // clang-format off
    auto upload_meshes_cs_code                 = SpirvCode::load_from_file(filesystem::relative_path() / "spirv/simulation_st1_1_upload_meshes.spv");
    auto clear_instance_count_cs_code          = SpirvCode::load_from_file(filesystem::relative_path() / "spirv/simulation_st2_clear_instance_count.spv");
    auto simulation_step_cs_code               = SpirvCode::load_from_file(filesystem::relative_path() / "spirv/simulation_st3_step.spv");
    auto build_indirect_instance_count_cs_code = SpirvCode::load_from_file(filesystem::relative_path() / "spirv/simulation_st4_build_indirect_instance_count.spv");
    auto build_indirect_first_instance_cs_code = SpirvCode::load_from_file(filesystem::relative_path() / "spirv/simulation_st5_build_indirect_first_instance.spv");
    auto build_instance_buffer_cs_code         = SpirvCode::load_from_file(filesystem::relative_path() / "spirv/simulation_st6_build_instance_buffer.spv");
    // clang-format on

    auto upload_meshes_cs = VulkanComputePipeline::builder()
        .set_pipeline_layout(upload_meshes_cs_layout)
        .set_shader(upload_meshes_cs_code)
        .build();

    auto clear_instance_count_cs = VulkanComputePipeline::builder()
        .set_pipeline_layout(clear_instance_count_cs_layout)
        .set_shader(clear_instance_count_cs_code)
        .build();

    auto simulation_step_cs = VulkanComputePipeline::builder()
        .set_pipeline_layout(simulation_step_cs_layout)
        .set_shader(simulation_step_cs_code)
        .build();

    auto build_indirect_instance_count_cs = VulkanComputePipeline::builder()
        .set_pipeline_layout(build_indirect_instance_count_cs_layout)
        .set_shader(build_indirect_instance_count_cs_code)
        .build();

    auto build_indirect_first_instance_cs = VulkanComputePipeline::builder()
        .set_pipeline_layout(build_indirect_first_instance_cs_layout)
        .set_shader(build_indirect_first_instance_cs_code)
        .build();

    auto build_instance_buffer_cs = VulkanComputePipeline::builder()
        .set_pipeline_layout(build_instance_buffer_cs_layout)
        .set_shader(build_instance_buffer_cs_code)
        .build();

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

    return VulkanRenderer{ std::move(swapchain), std::move(triangle_pipeline_layout), std::move(triangle_pipeline),
        std::move(upload_meshes_cs_layout), std::move(upload_meshes_cs), std::move(clear_instance_count_cs_layout), std::move(clear_instance_count_cs),
        std::move(simulation_step_cs_layout), std::move(simulation_step_cs), std::move(build_indirect_instance_count_cs_layout), std::move(build_indirect_instance_count_cs),
        std::move(build_indirect_first_instance_cs_layout), std::move(build_indirect_first_instance_cs), std::move(build_instance_buffer_cs_layout), std::move(build_instance_buffer_cs),
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

    // ---- Command recording begin ----------------------------------------------------------------------------------------------------------------------------

    vk::CommandBufferBeginInfo command_buffer_begin_info = vk::CommandBufferBeginInfo()
        .setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    auto& command_buffer = frame.frame_command_buffer;

    command_buffer.begin(command_buffer_begin_info);

    VulkanPipelineBarriers::builder()
        .insert_buffer_memory_barrier(m_indirect_draw_commands_buffer.vk_buffer(), vk::WholeSize, 0,
            vk::PipelineStageFlagBits2::eDrawIndirect, vk::AccessFlagBits2::eIndirectCommandRead,
            vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite
        )
        .flush(command_buffer);

    // ---- Fill draw commands mesh parameters if needed -------------------------------------------------------------------------------------------------------

    if (mesh_pool.mesh_allocations_table_generation() > m_indirect_draw_commands_mesh_params_generation) {
        prepare_indirect_draw_mesh_params(command_buffer, frame, mesh_pool);
        m_indirect_draw_commands_mesh_params_generation = mesh_pool.mesh_allocations_table_generation();
    }

    // ---- Clear instance counts ------------------------------------------------------------------------------------------------------------------------------

    // No need for a buffer barrier here since last step and this step overwrite different parts of the indirect draws buffer.
    clear_indirect_draw_instance_counts(command_buffer, mesh_pool);

    // ---- Transition before simulation pass ------------------------------------------------------------------------------------------------------------------

    VulkanPipelineBarriers::builder()
        .insert_buffer_memory_barrier(get_simulation_objects_src_buffer().vk_buffer(), vk::WholeSize, 0,
            vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite,
            vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageRead
        )
        .insert_buffer_memory_barrier(get_simulation_objects_dst_buffer().vk_buffer(), vk::WholeSize, 0,
            vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite,
            vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite
        )
        .flush(command_buffer);

    run_gpgpu_simulation_step(command_buffer, frame);

    // ---- Build indirect draw buffers ------------------------------------------------------------------------------------------------------------------------

    VulkanPipelineBarriers::builder()
        .insert_buffer_memory_barrier(m_indirect_draw_commands_buffer.vk_buffer(), vk::WholeSize, 0,
            vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite,
            vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite
        )
        .insert_buffer_memory_barrier(get_simulation_objects_dst_buffer().vk_buffer(), vk::WholeSize, 0,
            vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite,
            vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageRead
        )
        .flush(command_buffer);

    build_indirect_draw_buffers(command_buffer, mesh_pool, frame);

    // ---- Transition before main renderpass ------------------------------------------------------------------------------------------------------------------
    
    VulkanPipelineBarriers::builder()
        .insert_image_memory_barrier(m_main_render_target.vk_image(),
            vk::ImageLayout::eUndefined, vk::PipelineStageFlagBits2::eBlit, vk::AccessFlagBits2::eTransferRead,
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
            m_main_render_target.full_subresource_range()
        )
        .insert_image_memory_barrier(m_depth_buffer.vk_image(),
            vk::ImageLayout::eUndefined, vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
            vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite,

            vk::ImageLayout::eDepthStencilAttachmentOptimal, vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
            vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
            m_depth_buffer.full_subresource_range()
        )
        .insert_buffer_memory_barrier(m_indirect_draw_commands_buffer.vk_buffer(), vk::WholeSize, 0,
            vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite,
            vk::PipelineStageFlagBits2::eDrawIndirect, vk::AccessFlagBits2::eIndirectCommandRead
        )
        .insert_buffer_memory_barrier(m_instance_data_buffer.vk_buffer(), vk::WholeSize, 0,
            vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite,
            vk::PipelineStageFlagBits2::eVertexShader, vk::AccessFlagBits2::eShaderStorageRead
        )
        .flush(command_buffer);
    
    run_main_renderpass(command_buffer, mesh_pool, frame);

    // ---- Transition after main renderpass before ImGui renderpass -------------------------------------------------------------------------------------------

    VulkanPipelineBarriers::builder()
        .insert_image_memory_barrier(m_main_render_target.vk_image(),
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite,
            m_main_render_target.full_subresource_range()
        )
        .flush(command_buffer);

    run_imgui_renderpass(command_buffer, draw_data);

    // ---- Transition after ImGui renderpass before copy to swapchain image ------------------------------------------------------------------------------------
    VulkanPipelineBarriers::builder()
        .insert_image_memory_barrier(m_main_render_target.vk_image(),
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::ImageLayout::eTransferSrcOptimal, vk::PipelineStageFlagBits2::eBlit, vk::AccessFlagBits2::eTransferRead,
            m_main_render_target.full_subresource_range()
        )
        .insert_image_memory_barrier(swapchain_image.vk_image(),
            vk::ImageLayout::eUndefined, vk::PipelineStageFlagBits2::eAllCommands, vk::AccessFlagBits2::eNone,
            vk::ImageLayout::eTransferDstOptimal, vk::PipelineStageFlagBits2::eBlit, vk::AccessFlagBits2::eTransferWrite,
            swapchain_image.full_subresource_range()
        )
        .flush(command_buffer);

    copy_main_render_target_to_swapchain_image(command_buffer, swapchain_image);

    // ---- Transition after copy to swapchain image -----------------------------------------------------------------------------------------------------------

    VulkanPipelineBarriers::builder()
        .insert_image_memory_barrier(swapchain_image.vk_image(),
            vk::ImageLayout::eTransferDstOptimal, vk::PipelineStageFlagBits2::eBlit, vk::AccessFlagBits2::eTransferWrite,
            vk::ImageLayout::ePresentSrcKHR, vk::PipelineStageFlagBits2::eNone, vk::AccessFlagBits2::eNone,
            swapchain_image.full_subresource_range()
        )
        .flush(command_buffer);

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

auto VulkanRenderer::prepare_indirect_draw_mesh_params(vk::raii::CommandBuffer& cmd, FrameInFlight& frame, MeshPool& mesh_pool) -> void {
    memcpy(frame.mesh_allocations_table_staging_buffer.memory_host_ptr(), mesh_pool.mesh_allocations().data(), mesh_pool.mesh_allocations().size() * sizeof(shaders::MeshAllocationData));

    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_upload_meshes_cs.vk_pipeline());

    uint32_t mesh_count = static_cast<uint32_t>(mesh_pool.mesh_allocations().size());
    UploadMeshesCSPushConstants consts = {
        m_indirect_draw_commands_buffer.memory_device_ptr(),
        frame.mesh_allocations_table_staging_buffer.memory_device_ptr(),
        mesh_count
    };

    cmd.pushConstants<UploadMeshesCSPushConstants>(m_upload_meshes_cs_layout.vk_pipeline_layout(), vk::ShaderStageFlagBits::eCompute, 0, consts);
    cmd.dispatch(div_ceil(mesh_count, shader_consts::CS_UPLOAD_MESHES_GROUP_SIZE_X), 1, 1);
}

auto VulkanRenderer::clear_indirect_draw_instance_counts(vk::raii::CommandBuffer& cmd, MeshPool& mesh_pool) -> void {
    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_clear_instance_count_cs.vk_pipeline());

    uint32_t mesh_count = static_cast<uint32_t>(mesh_pool.mesh_allocations().size());

    ClearInstanceCountCSPushConstants consts = {
        m_indirect_draw_commands_buffer.memory_device_ptr(),
        mesh_count
    };

    cmd.pushConstants<ClearInstanceCountCSPushConstants>(m_clear_instance_count_cs_layout.vk_pipeline_layout(), vk::ShaderStageFlagBits::eCompute, 0, consts);
    cmd.dispatch(div_ceil(mesh_count, shader_consts::CS_CLEAR_INSTANCE_COUNT_GROUP_SIZE_X), 1, 1);
}

auto VulkanRenderer::run_gpgpu_simulation_step(vk::raii::CommandBuffer& cmd, FrameInFlight& frame) -> void {
    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_simulation_step_cs.vk_pipeline());

    SimulationStepCSPushConstants consts = {
        frame.simulation_data_buffer.memory_device_ptr(),
        get_simulation_objects_src_buffer().memory_device_ptr(),
        get_simulation_objects_dst_buffer().memory_device_ptr(),
        m_gpu_driven_sim_object_count
    };
    cmd.pushConstants<SimulationStepCSPushConstants>(m_simulation_step_cs_layout.vk_pipeline_layout(), vk::ShaderStageFlagBits::eCompute, 0, consts);

    cmd.dispatch(
        div_ceil(m_gpu_driven_sim_object_count, shader_consts::CS_SIMULATION_STEP_GROUP_SIZE_X),
        1,
        1
    );
}

auto VulkanRenderer::build_indirect_draw_buffers(vk::raii::CommandBuffer& cmd, MeshPool& mesh_pool, FrameInFlight& frame) -> void {
    // Build instance counts first.
    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_build_indirect_instance_count_cs.vk_pipeline());

    BuildIndirectInstanceCountCSPushConstants instance_count_cs_consts = {
        m_indirect_draw_commands_buffer.memory_device_ptr(),
        m_drawlocal_instance_ids_buffer.memory_device_ptr(),
        get_simulation_objects_dst_buffer().memory_device_ptr(),
        m_gpu_driven_sim_object_count
    };
    cmd.pushConstants<BuildIndirectInstanceCountCSPushConstants>(m_build_indirect_instance_count_cs_layout.vk_pipeline_layout(), vk::ShaderStageFlagBits::eCompute, 0, instance_count_cs_consts);
    cmd.dispatch(div_ceil(m_gpu_driven_sim_object_count, shader_consts::CS_BUILD_INDIRECT_INSTANCE_COUNT_GROUP_SIZE_X), 1, 1);

    // Make sure that writes to the indirect draw commands buffer are visible to next dispatch.
    VulkanPipelineBarriers::builder()
        .insert_buffer_memory_barrier(m_indirect_draw_commands_buffer.vk_buffer(), vk::WholeSize, 0,
            vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite,
            vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite
        )
        .flush(cmd);

    // After previous dispatch is done, build first instance values.
    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_build_indirect_first_instance_cs.vk_pipeline());

    uint32_t draw_count = static_cast<uint32_t>(mesh_pool.mesh_allocations().size());
    assert(draw_count <= shader_consts::GPU_WAVE_SIZE && "draw count must be smaller or equal to the GPU wave size");
    BuildIndirectFirstInstanceCSPushConstants first_instance_cs_consts = {
        m_indirect_draw_commands_buffer.memory_device_ptr(),
        draw_count
    };
    cmd.pushConstants<BuildIndirectFirstInstanceCSPushConstants>(m_build_indirect_first_instance_cs_layout.vk_pipeline_layout(), vk::ShaderStageFlagBits::eCompute, 0, first_instance_cs_consts);
    // 1 group for 1 wave that handles all draw instance prefix sums
    cmd.dispatch(1, 1, 1);

    // Make sure that instance ID writes from the earlier dispatch and the draw command writes from the previous dispatch are visible to the next.
    VulkanPipelineBarriers::builder()
        .insert_buffer_memory_barrier(m_drawlocal_instance_ids_buffer.vk_buffer(), vk::WholeSize, 0,
            vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite,
            vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageRead
        )
        .insert_buffer_memory_barrier(m_indirect_draw_commands_buffer.vk_buffer(), vk::WholeSize, 0,
            vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite,
            vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageRead
        )
        .flush(cmd);

    // Once both dispatches are done we can build the instance buffer.
    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_build_instance_buffer_cs.vk_pipeline());

    BuildInstanceBufferCSPushConstants instance_buffer_cs_consts = {
        m_indirect_draw_commands_buffer.memory_device_ptr(),
        m_drawlocal_instance_ids_buffer.memory_device_ptr(),
        frame.simulation_data_buffer.memory_device_ptr(),
        get_simulation_objects_dst_buffer().memory_device_ptr(),
        m_instance_data_buffer.memory_device_ptr(),
        m_gpu_driven_sim_object_count
    };
    cmd.pushConstants<BuildInstanceBufferCSPushConstants>(m_build_instance_buffer_cs_layout.vk_pipeline_layout(), vk::ShaderStageFlagBits::eCompute, 0, instance_buffer_cs_consts);
    cmd.dispatch(div_ceil(m_gpu_driven_sim_object_count, shader_consts::CS_BUILD_INSTANCE_BUFFER_GROUP_SIZE_X), 1, 1);
}

auto VulkanRenderer::run_main_renderpass(vk::raii::CommandBuffer& cmd, MeshPool& mesh_pool, FrameInFlight& frame) -> void {
    auto main_render_target_attachment_info = vk::RenderingAttachmentInfo()
        .setImageView(m_main_render_target.vk_full_image_view())
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue(vk::ClearColorValue{0.0f, 0.0f, 0.0f, 1.0f});

    auto depth_buffer_attachment_info = vk::RenderingAttachmentInfo()
        .setImageView(m_depth_buffer.vk_full_image_view())
        .setImageLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue(vk::ClearDepthStencilValue{0.0f, 0});

    auto rendering_area = vk::Extent2D(m_main_render_target.extent().width, m_main_render_target.extent().height);

    auto viewport = vk::Viewport()
        .setWidth(rendering_area.width)
        .setHeight(rendering_area.height)
        .setMinDepth(0.0f)
        .setMaxDepth(1.0f);

    auto scissor = vk::Rect2D()
        .setExtent(rendering_area);

    auto rendering_info = vk::RenderingInfo()
        .setColorAttachments(main_render_target_attachment_info)
        .setPDepthAttachment(&depth_buffer_attachment_info)
        .setLayerCount(1)
        .setRenderArea(scissor);

    cmd.beginRendering(rendering_info);
    cmd.setViewport(0, viewport);
    cmd.setScissor(0, scissor);

    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_triangle_pipeline.vk_pipeline());

    cmd.bindIndexBuffer(mesh_pool.index_buffer().vk_buffer(), 0, vk::IndexType::eUint32);

    struct PushConstants {
        vk::DeviceAddress vertex_buffer;
        vk::DeviceAddress prepared_simulation_objects;
        vk::DeviceAddress simulation_data;
    };

    PushConstants consts = {
        mesh_pool.vertex_buffer().memory_device_ptr(),
        m_instance_data_buffer.memory_device_ptr(),
        frame.simulation_data_buffer.memory_device_ptr()
    };

    cmd.pushConstants<PushConstants>(m_triangle_pipeline_layout.vk_pipeline_layout(), vk::ShaderStageFlagBits::eVertex, 0, consts);
    cmd.drawIndexedIndirect(m_indirect_draw_commands_buffer.vk_buffer(), 0, static_cast<uint32_t>(mesh_pool.mesh_allocations().size()), sizeof(vk::DrawIndexedIndirectCommand));

    cmd.endRendering();
}
auto VulkanRenderer::run_imgui_renderpass(vk::raii::CommandBuffer& cmd, ImDrawData* draw_data) -> void {
    auto main_render_target_attachment_info = vk::RenderingAttachmentInfo()
        .setImageView(m_main_render_target.vk_full_image_view())
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eLoad)
        .setStoreOp(vk::AttachmentStoreOp::eStore);

    auto rendering_area = vk::Extent2D(m_main_render_target.extent().width, m_main_render_target.extent().height);

    auto viewport = vk::Viewport()
        .setWidth(rendering_area.width)
        .setHeight(rendering_area.height);

    auto scissor = vk::Rect2D()
        .setExtent(rendering_area);

    auto rendering_info = vk::RenderingInfo()
        .setColorAttachments(main_render_target_attachment_info)
        .setLayerCount(1)
        .setRenderArea(scissor);

    cmd.beginRendering(rendering_info);
    cmd.setViewport(0, viewport);
    cmd.setScissor(0, scissor);

    if (draw_data) {
        ImGui_ImplVulkan_RenderDrawData(draw_data, *cmd);
    }

    cmd.endRendering();
}
auto VulkanRenderer::copy_main_render_target_to_swapchain_image(vk::raii::CommandBuffer& cmd, const VulkanSwapchainImage& swapchain_image) -> void {
    auto blit_offsets = std::array<vk::Offset3D, 2>{{{0, 0, 0}, {to_offset3d(m_main_render_target.extent())}}};

    auto blit_subresources = vk::ImageSubresourceLayers()
        .setAspectMask(vk::ImageAspectFlagBits::eColor)
        .setBaseArrayLayer(0)
        .setLayerCount(1)
        .setMipLevel(0);

    auto blit = vk::ImageBlit2()
        .setSrcOffsets(blit_offsets)
        .setDstOffsets(blit_offsets)
        .setSrcSubresource(blit_subresources)
        .setDstSubresource(blit_subresources);

    auto blit_info = vk::BlitImageInfo2()
        .setSrcImage(m_main_render_target.vk_image())
        .setSrcImageLayout(vk::ImageLayout::eTransferSrcOptimal)
        .setDstImage(swapchain_image.vk_image())
        .setDstImageLayout(vk::ImageLayout::eTransferDstOptimal)
        .setFilter(vk::Filter::eNearest)
        .setRegions(blit);

    cmd.blitImage2(blit_info);
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

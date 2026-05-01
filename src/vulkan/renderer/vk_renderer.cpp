#include "vk_renderer.hpp"

#include "systems/filesystem.hpp"
#include "vulkan/spirv_code.hpp"
#include "vulkan/vk_context.hpp"
#include "vulkan/vk_pipeline_barriers.hpp"

#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>
#include <print>

namespace pop::vulkan::renderer {

VulkanRenderer::VulkanRenderer(VulkanSwapchain&& swapchain, VulkanPipelineLayout&& triangle_pipeline_layout, VulkanGraphicsPipeline&& triangle_pipeline,
        VulkanPipelineLayout&& simulation_pipeline_layout, VulkanComputePipeline&& simulation_pipeline, VulkanPipelineLayout&& simulation_clear_pipeline_layout,
        VulkanComputePipeline&& simulation_clear_pipeline, VulkanBuffer&& simulation_draw_commands_buffer, VulkanBuffer&& simulation_draw_commands_count_buffer,
        VulkanImage&& main_render_target, std::vector<FrameInFlight>&& frames_in_flight)
    : m_swapchain(std::move(swapchain)), m_triangle_pipeline_layout(std::move(triangle_pipeline_layout)), m_triangle_pipeline(std::move(triangle_pipeline)),
        m_simulation_pipeline_layout(std::move(simulation_pipeline_layout)), m_simulation_pipeline(std::move(simulation_pipeline)),
        m_simulation_clear_pipeline_layout(std::move(simulation_clear_pipeline_layout)), m_simulation_clear_pipeline(std::move(simulation_clear_pipeline)),
        m_simulation_draw_commands_buffer(std::move(simulation_draw_commands_buffer)),
        m_simulation_draw_commands_count_buffer(std::move(simulation_draw_commands_count_buffer)), m_main_render_target(std::move(main_render_target)),
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

// TODO: Temporary limits, add dynamic resizing later
constexpr uint64_t MAX_DRAW_COMMANDS = 1;

auto VulkanRenderer::create(VulkanSwapchain&& swapchain) -> VulkanRenderer {
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

        auto simulation_object_buffer = VulkanBuffer::builder()
            .set_size(1 * sizeof(shaders::SimulationObject))
            .set_usage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress)
            .set_memory_usage(vma::MemoryUsage::eAutoPreferDevice)
            .map_for_sequential_write()
            .build();

        auto mesh_index_to_buffer_params_table = VulkanBuffer::builder()
            .set_size(64 * sizeof(shaders::MeshAllocationData))
            .set_usage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress)
            .set_memory_usage(vma::MemoryUsage::eAutoPreferDevice)
            .map_for_sequential_write()
            .build();

        frames_in_flight.emplace_back(std::move(frame_finished_fence), std::move(image_acquired_semaphore), std::move(command_pool),
            std::move(frame_command_buffer), std::move(simulation_object_buffer), std::move(mesh_index_to_buffer_params_table));
    }

    auto triangle_pipeline_layout = VulkanPipelineLayout::builder()
        .add_push_constant_range(0, 8, vk::ShaderStageFlagBits::eVertex)
        .build();

    auto triangle_pipeline_shader_code = SpirvCode::load_from_file(filesystem::relative_path() / "spirv/triangle.spv");

    auto triangle_pipeline = VulkanGraphicsPipeline::builder()
        .set_pipeline_layout(triangle_pipeline_layout)
        .add_shader(triangle_pipeline_shader_code, vk::ShaderStageFlagBits::eVertex)
        .add_shader(triangle_pipeline_shader_code, vk::ShaderStageFlagBits::eFragment)
        .set_input_topology(vk::PrimitiveTopology::eTriangleList)
        .set_rasterizer_polygon_mode(vk::PolygonMode::eFill)
        .set_rasterizer_cull_mode(vk::CullModeFlagBits::eNone, vk::FrontFace::eCounterClockwise)
        .set_rasterizer_line_width(1.0f)
        .disable_multisampling()
        .disable_depth_test()
        .add_rendering_attachment(
            vk::PipelineColorBlendAttachmentState()
                .setBlendEnable(false)
                .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA),
            vk::Format::eR16G16B16A16Sfloat
        )
        .build();

    auto simulation_pipeline_layout = VulkanPipelineLayout::builder()
        .add_push_constant_range(0, 40, vk::ShaderStageFlagBits::eCompute) // 4 device ptrs + 1 uint32_t
        .build();

    auto simulation_pipeline_shader_code = SpirvCode::load_from_file(filesystem::relative_path() / "spirv/simulation.spv");
    auto simulation_pipeline = VulkanComputePipeline::builder()
        .set_pipeline_layout(simulation_pipeline_layout)
        .set_shader(simulation_pipeline_shader_code)
        .build();

    auto simulation_clear_pipeline_layout = VulkanPipelineLayout::builder()
        .add_push_constant_range(0, 40, vk::ShaderStageFlagBits::eCompute) // 4 device ptrs + 1 uint32_t
        .build();

    auto simulation_clear_pipeline_shader_code = SpirvCode::load_from_file(filesystem::relative_path() / "spirv/simulation_drawcount_clear.spv");
    auto simulation_clear_pipeline = VulkanComputePipeline::builder()
        .set_pipeline_layout(simulation_clear_pipeline_layout)
        .set_shader(simulation_clear_pipeline_shader_code)
        .build();

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

    auto simulation_draw_commands_buffer = VulkanBuffer::builder()
        .set_size(MAX_DRAW_COMMANDS * sizeof(vk::DrawIndexedIndirectCommand))
        .set_usage(vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .set_memory_usage(vma::MemoryUsage::eAutoPreferDevice)
        .map_for_sequential_write()
        .build();

    auto simulation_draw_commands_count_buffer = VulkanBuffer::builder()
        .set_size(sizeof(uint32_t))
        .set_usage(vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eTransferDst)
        .set_memory_usage(vma::MemoryUsage::eAutoPreferDevice)
        .map_for_sequential_write()
        .build();

    // TODO: Temporary, add proper dispatch filling the buffers later

    auto triangle_draw_indirect_command = vk::DrawIndirectCommand()
        .setVertexCount(3)
        .setInstanceCount(1)
        .setFirstVertex(0)
        .setFirstInstance(0);

    uint32_t draw_commands_count = 1;

    memcpy(simulation_draw_commands_buffer.memory_host_ptr(), &triangle_draw_indirect_command, sizeof(vk::DrawIndirectCommand));
    memcpy(simulation_draw_commands_count_buffer.memory_host_ptr(), &draw_commands_count, sizeof(uint32_t));

    return VulkanRenderer{ std::move(swapchain), std::move(triangle_pipeline_layout), std::move(triangle_pipeline), std::move(simulation_pipeline_layout),
            std::move(simulation_pipeline), std::move(simulation_clear_pipeline_layout), std::move(simulation_clear_pipeline),
            std::move(simulation_draw_commands_buffer), std::move(simulation_draw_commands_count_buffer), std::move(main_render_target),
        std::move(frames_in_flight) };
}

auto VulkanRenderer::render_frame(MeshPool& mesh_pool, Mesh& sample_mesh, ImDrawData* draw_data) -> RenderResult {
    auto& frame = m_frames_in_flight[m_current_frame];
    auto& device = VulkanContext::get().vk_device();
    device.waitForFences(*frame.frame_finished_fence, true, std::numeric_limits<uint64_t>::max());

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

    // ---- Fill simulation object buffer and mesh index to parameter table ------------------------------------------------------------------------------------

    shaders::SimulationObject simulation_object_data = shaders::SimulationObject{ .mesh_index = sample_mesh.allocation_index };

    memcpy(frame.simulation_object_buffer.memory_host_ptr(), &simulation_object_data, sizeof(shaders::SimulationObject));

    if (mesh_pool.mesh_allocations_table_generation() > frame.mesh_index_to_buffer_params_table_generation) {
        // Make sure the destination buffer has enough size
        uint64_t new_table_byte_size = mesh_pool.mesh_allocations().size() * sizeof(shaders::MeshAllocationData);
        if (frame.mesh_index_to_buffer_params_table.size() < new_table_byte_size) {
            frame.mesh_index_to_buffer_params_table = VulkanBuffer::builder()
                .set_size(new_table_byte_size * 2)
                .set_usage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress)
                .set_memory_usage(vma::MemoryUsage::eAutoPreferDevice)
                .map_for_sequential_write()
                .build();
        }

        std::ranges::copy(mesh_pool.mesh_allocations(), reinterpret_cast<shaders::MeshAllocationData*>(frame.mesh_index_to_buffer_params_table.memory_host_ptr()));
        frame.mesh_index_to_buffer_params_table_generation = mesh_pool.mesh_allocations_table_generation();
    }

    // ---- Command recording begin ----------------------------------------------------------------------------------------------------------------------------

    vk::CommandBufferBeginInfo command_buffer_begin_info = vk::CommandBufferBeginInfo()
        .setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    auto& command_buffer = frame.frame_command_buffer;

    command_buffer.begin(command_buffer_begin_info);

    // ---- Transition before simulation pass ------------------------------------------------------------------------------------------------------------------

    VulkanPipelineBarriers::builder()
        .insert_buffer_memory_barrier(m_simulation_draw_commands_buffer.vk_buffer(), vk::WholeSize, 0,
            vk::PipelineStageFlagBits2::eDrawIndirect, vk::AccessFlagBits2::eIndirectCommandRead,
            vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite
        )
        .insert_buffer_memory_barrier(m_simulation_draw_commands_count_buffer.vk_buffer(), vk::WholeSize, 0,
            vk::PipelineStageFlagBits2::eDrawIndirect, vk::AccessFlagBits2::eIndirectCommandRead,
            vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite
        )
        .flush(command_buffer);

    run_gpgpu_simulation_step(command_buffer, frame, 1);

    // ---- Transition before main renderpass ------------------------------------------------------------------------------------------------------------------
    
    VulkanPipelineBarriers::builder()
        .insert_image_memory_barrier(m_main_render_target.vk_image(),
            vk::ImageLayout::eUndefined, vk::PipelineStageFlagBits2::eBlit, vk::AccessFlagBits2::eTransferRead,
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
            m_main_render_target.full_subresource_range()
        )
        .insert_buffer_memory_barrier(m_simulation_draw_commands_buffer.vk_buffer(), vk::WholeSize, 0,
            vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite,
            vk::PipelineStageFlagBits2::eDrawIndirect, vk::AccessFlagBits2::eIndirectCommandRead
        )
        .insert_buffer_memory_barrier(m_simulation_draw_commands_count_buffer.vk_buffer(), vk::WholeSize, 0,
            vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite,
            vk::PipelineStageFlagBits2::eDrawIndirect, vk::AccessFlagBits2::eIndirectCommandRead
        )
        .flush(command_buffer);
    
    run_main_renderpass(command_buffer, mesh_pool, draw_data);

    // ---- Transition after main renderpass before copy to swapchain image ------------------------------------------------------------------------------------

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

    m_swapchain = VulkanSwapchain::create(new_window_extent, std::move(m_swapchain), true);
}

auto VulkanRenderer::swapchain() const -> const VulkanSwapchain& {
    return m_swapchain;
}

auto VulkanRenderer::run_gpgpu_simulation_step(vk::raii::CommandBuffer& cmd, FrameInFlight& frame, uint32_t object_count) -> void {
    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_simulation_clear_pipeline.vk_pipeline());
    cmd.pushConstants<vk::DeviceAddress>(m_simulation_clear_pipeline_layout.vk_pipeline_layout(), vk::ShaderStageFlagBits::eCompute, 0, m_simulation_draw_commands_count_buffer.memory_device_ptr());
    cmd.dispatch(1, 1, 1);

    VulkanPipelineBarriers::builder()
        .insert_buffer_memory_barrier(m_simulation_draw_commands_count_buffer.vk_buffer(), vk::WholeSize, 0,
            vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite,
            vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite
        )
        .flush(cmd);

    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_simulation_pipeline.vk_pipeline());

    struct PushConstants {
        vk::DeviceSize simulation_objects;
        vk::DeviceSize mesh_allocations;
        vk::DeviceSize draw_indirect_commands;
        vk::DeviceSize draw_indirect_commands_count;
        uint32_t object_count;
        uint32_t _pad;
    };

    PushConstants consts = {
        frame.simulation_object_buffer.memory_device_ptr(),
        frame.mesh_index_to_buffer_params_table.memory_device_ptr(),
        m_simulation_draw_commands_buffer.memory_device_ptr(),
        m_simulation_draw_commands_count_buffer.memory_device_ptr(),
        object_count
    };

    cmd.pushConstants<PushConstants>(m_simulation_pipeline_layout.vk_pipeline_layout(), vk::ShaderStageFlagBits::eCompute, 0, consts);
    // TODO: DRY it with the shader params later
    constexpr uint32_t SIMULATION_OBJECTS_PER_GROUP = 64;

    cmd.dispatch(
        (object_count + SIMULATION_OBJECTS_PER_GROUP - 1) / SIMULATION_OBJECTS_PER_GROUP,
        1,
        1
    );
}

auto VulkanRenderer::run_main_renderpass(vk::raii::CommandBuffer& cmd, MeshPool& mesh_pool, ImDrawData* draw_data) -> void {
    auto main_render_target_attachment_info = vk::RenderingAttachmentInfo()
        .setImageView(m_main_render_target.vk_full_image_view())
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue(vk::ClearColorValue{0.0f, 0.0f, 0.0f, 1.0f});

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
        .setLayerCount(1)
        .setRenderArea(scissor);

    cmd.beginRendering(rendering_info);
    cmd.setViewport(0, viewport);
    cmd.setScissor(0, scissor);

    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_triangle_pipeline.vk_pipeline());

    cmd.bindIndexBuffer(mesh_pool.index_buffer().vk_buffer(), 0, vk::IndexType::eUint32);

    cmd.pushConstants<vk::DeviceAddress>(m_triangle_pipeline_layout.vk_pipeline_layout(), vk::ShaderStageFlagBits::eVertex, 0, mesh_pool.vertex_buffer().memory_device_ptr());
    cmd.drawIndexedIndirectCount(m_simulation_draw_commands_buffer.vk_buffer(), 0, m_simulation_draw_commands_count_buffer.vk_buffer(), 0, 1, sizeof(vk::DrawIndexedIndirectCommand));

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

} // namespace pop::vulkan::renderer

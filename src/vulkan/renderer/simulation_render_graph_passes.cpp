#include "simulation_render_graph_passes.hpp"

#include "pipelines_push_constants.hpp"
#include "shaders/shared_consts.hpp"
#include "systems/systems.hpp"

#include <backends/imgui_impl_vulkan.h>

namespace pop::vulkan::renderer {

// ---- SimulationUploadMeshInfoPass ---------------------------------------------------------------------------------------------------------------------------

UploadMeshInfoPass::UploadMeshInfoPass(render_graph::PassDependencies&& deps, VulkanPipelineLayout&& pipeline_layout,
    VulkanComputePipeline&& compute_pipeline)
        : render_graph::PassBase<SimulationRenderState>(std::move(deps)), m_pipeline_layout(std::move(pipeline_layout)), m_compute_pipeline(std::move(compute_pipeline)) {}

auto UploadMeshInfoPass::create() -> UploadMeshInfoPass {
    auto dependencies = render_graph::PassDependencies::builder()
        .add_buffer_dependency(render_graph::BufferResourceIdentifier::FrameLocalMeshInfoStagingBuffer, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderRead)
        .add_buffer_dependency(render_graph::BufferResourceIdentifier::SimulationDrawIndirectCommands, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderWrite)
        .build();

    auto cs_layout = VulkanPipelineLayout::builder()
        .add_push_constant_range(0, sizeof(UploadMeshesCSPushConstants), vk::ShaderStageFlagBits::eCompute)
        .build();

    auto cs_code = SpirvCode::load_from_file(systems::relative_path() / "spirv/simulation_st1_1_upload_meshes.spv");

    auto cs = VulkanComputePipeline::builder()
        .set_pipeline_layout(cs_layout)
        .set_shader(cs_code)
        .build();

    return UploadMeshInfoPass(std::move(dependencies), std::move(cs_layout), std::move(cs));
}

auto UploadMeshInfoPass::invoke(vk::raii::CommandBuffer& cmd, const SimulationRenderState& state, const render_graph::PassResources& resources)
    -> void {
    auto& frame_local_mesh_info_staging_buffer = resources.get_buffer_by_identifier(render_graph::BufferResourceIdentifier::FrameLocalMeshInfoStagingBuffer);
    auto& indirect_draw_commands_buffer = resources.get_buffer_by_identifier(render_graph::BufferResourceIdentifier::SimulationDrawIndirectCommands);
    uint32_t mesh_count = static_cast<uint32_t>(state.mesh_pool.get().mesh_allocations().size());

    memcpy(frame_local_mesh_info_staging_buffer.memory_host_ptr(), state.mesh_pool.get().mesh_allocations().data(), mesh_count * sizeof(shaders::MeshAllocationData));

    UploadMeshesCSPushConstants consts = {
        indirect_draw_commands_buffer.memory_device_ptr(),
        frame_local_mesh_info_staging_buffer.memory_device_ptr(),
        mesh_count
    };

    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_compute_pipeline.vk_pipeline());
    cmd.pushConstants<UploadMeshesCSPushConstants>(m_pipeline_layout.vk_pipeline_layout(), vk::ShaderStageFlagBits::eCompute, 0, consts);
    cmd.dispatch(div_ceil(mesh_count, shader_consts::CS_UPLOAD_MESHES_GROUP_SIZE_X), 1, 1);
}

// ---- SimulationIndirectDrawCommandsResetPass ----------------------------------------------------------------------------------------------------------------

IndirectDrawCommandsClearPass::IndirectDrawCommandsClearPass(render_graph::PassDependencies&& deps, VulkanPipelineLayout&& pipeline_layout,
    VulkanComputePipeline&& compute_pipeline)
        : render_graph::PassBase<SimulationRenderState>(std::move(deps)), m_pipeline_layout(std::move(pipeline_layout)), m_compute_pipeline(std::move(compute_pipeline)) {}

auto IndirectDrawCommandsClearPass::create() -> IndirectDrawCommandsClearPass {
    auto dependencies = render_graph::PassDependencies::builder()
        .add_buffer_dependency(render_graph::BufferResourceIdentifier::SimulationDrawIndirectCommands, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderWrite)
        .build();

    auto cs_layout = VulkanPipelineLayout::builder()
        .add_push_constant_range(0, sizeof(ClearInstanceCountCSPushConstants), vk::ShaderStageFlagBits::eCompute)
        .build();

    auto cs_code = SpirvCode::load_from_file(systems::relative_path() / "spirv/simulation_st2_clear_instance_count.spv");

    auto cs = VulkanComputePipeline::builder()
        .set_pipeline_layout(cs_layout)
        .set_shader(cs_code)
        .build();

    return IndirectDrawCommandsClearPass(std::move(dependencies), std::move(cs_layout), std::move(cs));
}

auto IndirectDrawCommandsClearPass::invoke(vk::raii::CommandBuffer& cmd, const SimulationRenderState& state, const render_graph::PassResources& resources) -> void {
    auto& indirect_draw_commands_buffer = resources.get_buffer_by_identifier(render_graph::BufferResourceIdentifier::SimulationDrawIndirectCommands);
    uint32_t mesh_count = static_cast<uint32_t>(state.mesh_pool.get().mesh_allocations().size());

    ClearInstanceCountCSPushConstants consts = {
        indirect_draw_commands_buffer.memory_device_ptr(),
        mesh_count
    };

    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_compute_pipeline.vk_pipeline());
    cmd.pushConstants<ClearInstanceCountCSPushConstants>(m_pipeline_layout.vk_pipeline_layout(), vk::ShaderStageFlagBits::eCompute, 0, consts);
    cmd.dispatch(div_ceil(mesh_count, shader_consts::CS_CLEAR_INSTANCE_COUNT_GROUP_SIZE_X), 1, 1);
}

// ---- SimulationStepPass -------------------------------------------------------------------------------------------------------------------------------------

SimulationStepPass::SimulationStepPass(render_graph::PassDependencies&& deps, VulkanPipelineLayout&& pipeline_layout,
    VulkanComputePipeline&& compute_pipeline)
        : render_graph::PassBase<SimulationRenderState>(std::move(deps)), m_pipeline_layout(std::move(pipeline_layout)), m_compute_pipeline(std::move(compute_pipeline)) {}

auto SimulationStepPass::create() -> SimulationStepPass {
    auto dependencies = render_graph::PassDependencies::builder()
        .add_buffer_dependency(render_graph::BufferResourceIdentifier::FrameLocalSimulationData, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderRead)
        .add_buffer_dependency(render_graph::BufferResourceIdentifier::SimulationObjects, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderRead)
        .add_buffer_dependency(render_graph::BufferResourceIdentifier::SimulationNextObjects, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderWrite)
        .build();

    auto cs_layout = VulkanPipelineLayout::builder()
        .add_push_constant_range(0, sizeof(SimulationStepCSPushConstants), vk::ShaderStageFlagBits::eCompute)
        .build();

    auto cs_code = SpirvCode::load_from_file(systems::relative_path() / "spirv/simulation_st3_step.spv");

    auto cs = VulkanComputePipeline::builder()
        .set_pipeline_layout(cs_layout)
        .set_shader(cs_code)
        .build();

    return SimulationStepPass(std::move(dependencies), std::move(cs_layout), std::move(cs));
}

auto SimulationStepPass::invoke(vk::raii::CommandBuffer& cmd, const SimulationRenderState& state, const render_graph::PassResources& resources) -> void {
    auto& frame_local_simulation_data_buffer = resources.get_buffer_by_identifier(render_graph::BufferResourceIdentifier::FrameLocalSimulationData);
    auto& simulation_objects_buffer = resources.get_buffer_by_identifier(render_graph::BufferResourceIdentifier::SimulationObjects);
    auto& simulation_next_objects_buffer = resources.get_buffer_by_identifier(render_graph::BufferResourceIdentifier::SimulationNextObjects);

    SimulationStepCSPushConstants consts = {
        frame_local_simulation_data_buffer.memory_device_ptr(),
        simulation_objects_buffer.memory_device_ptr(),
        simulation_next_objects_buffer.memory_device_ptr(),
        state.object_count
    };

    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_compute_pipeline.vk_pipeline());
    cmd.pushConstants<SimulationStepCSPushConstants>(m_pipeline_layout.vk_pipeline_layout(), vk::ShaderStageFlagBits::eCompute, 0, consts);
    cmd.dispatch(
        div_ceil(state.object_count, shader_consts::CS_SIMULATION_STEP_GROUP_SIZE_X),
        1,
        1
    );
}

// ---- IndirectDrawCommandsInstanceCountBuildPass -------------------------------------------------------------------------------------------------------------

IndirectDrawCommandsInstanceCountBuildPass::IndirectDrawCommandsInstanceCountBuildPass(render_graph::PassDependencies&& deps,
    VulkanPipelineLayout&& pipeline_layout, VulkanComputePipeline&& compute_pipeline)
        : render_graph::PassBase<SimulationRenderState>(std::move(deps)), m_pipeline_layout(std::move(pipeline_layout)), m_compute_pipeline(std::move(compute_pipeline)) {}

auto IndirectDrawCommandsInstanceCountBuildPass::create() -> IndirectDrawCommandsInstanceCountBuildPass {
    auto dependencies = render_graph::PassDependencies::builder()
        .add_buffer_dependency(render_graph::BufferResourceIdentifier::DrawCommandsObjectInstanceOffsets, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderWrite)
        .add_buffer_dependency(render_graph::BufferResourceIdentifier::SimulationNextObjects, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderRead)
        .add_buffer_dependency(render_graph::BufferResourceIdentifier::SimulationDrawIndirectCommands, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite)
        .build();

    auto cs_layout = VulkanPipelineLayout::builder()
        .add_push_constant_range(0, sizeof(BuildIndirectInstanceCountCSPushConstants), vk::ShaderStageFlagBits::eCompute)
        .build();

    auto cs_code = SpirvCode::load_from_file(systems::relative_path() / "spirv/simulation_st4_build_indirect_instance_count.spv");

    auto cs = VulkanComputePipeline::builder()
        .set_pipeline_layout(cs_layout)
        .set_shader(cs_code)
        .build();

    return IndirectDrawCommandsInstanceCountBuildPass(std::move(dependencies), std::move(cs_layout), std::move(cs));
}

auto IndirectDrawCommandsInstanceCountBuildPass::invoke(vk::raii::CommandBuffer& cmd, const SimulationRenderState& state, const render_graph::PassResources& resources)
    -> void {
    auto& draw_commands_object_instance_offsets_buffer = resources.get_buffer_by_identifier(render_graph::BufferResourceIdentifier::DrawCommandsObjectInstanceOffsets);
    auto& simulation_next_objects_buffer = resources.get_buffer_by_identifier(render_graph::BufferResourceIdentifier::SimulationNextObjects);
    auto& simulation_draw_indirect_commands_buffer = resources.get_buffer_by_identifier(render_graph::BufferResourceIdentifier::SimulationDrawIndirectCommands);

    BuildIndirectInstanceCountCSPushConstants instance_count_cs_consts = {
        simulation_draw_indirect_commands_buffer.memory_device_ptr(),
        draw_commands_object_instance_offsets_buffer.memory_device_ptr(),
        simulation_next_objects_buffer.memory_device_ptr(),
        state.object_count
    };

    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_compute_pipeline.vk_pipeline());
    cmd.pushConstants<BuildIndirectInstanceCountCSPushConstants>(m_pipeline_layout.vk_pipeline_layout(), vk::ShaderStageFlagBits::eCompute, 0, instance_count_cs_consts);
    cmd.dispatch(div_ceil(state.object_count, shader_consts::CS_BUILD_INDIRECT_INSTANCE_COUNT_GROUP_SIZE_X), 1, 1);
}

// ---- IndirectDrawCommandsFirstInstanceBuildPass -------------------------------------------------------------------------------------------------------------

IndirectDrawCommandsFirstInstanceBuildPass::IndirectDrawCommandsFirstInstanceBuildPass(render_graph::PassDependencies&& deps,
    VulkanPipelineLayout&& pipeline_layout, VulkanComputePipeline&& compute_pipeline)
        : render_graph::PassBase<SimulationRenderState>(std::move(deps)), m_pipeline_layout(std::move(pipeline_layout)), m_compute_pipeline(std::move(compute_pipeline)) {}

auto IndirectDrawCommandsFirstInstanceBuildPass::create() -> IndirectDrawCommandsFirstInstanceBuildPass {
    auto dependencies = render_graph::PassDependencies::builder()
        .add_buffer_dependency(render_graph::BufferResourceIdentifier::SimulationDrawIndirectCommands, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite)
        .build();

    auto cs_layout = VulkanPipelineLayout::builder()
        .add_push_constant_range(0, sizeof(BuildIndirectFirstInstanceCSPushConstants), vk::ShaderStageFlagBits::eCompute)
        .build();

    auto cs_code = SpirvCode::load_from_file(systems::relative_path() / "spirv/simulation_st5_build_indirect_first_instance.spv");

    auto cs = VulkanComputePipeline::builder()
        .set_pipeline_layout(cs_layout)
        .set_shader(cs_code)
        .build();

    return IndirectDrawCommandsFirstInstanceBuildPass(std::move(dependencies), std::move(cs_layout), std::move(cs));
}

auto IndirectDrawCommandsFirstInstanceBuildPass::invoke(vk::raii::CommandBuffer& cmd, const SimulationRenderState& state, const render_graph::PassResources& resources)
    -> void {
    auto& simulation_draw_indirect_commands_buffer = resources.get_buffer_by_identifier(render_graph::BufferResourceIdentifier::SimulationDrawIndirectCommands);
    uint32_t mesh_count = static_cast<uint32_t>(state.mesh_pool.get().mesh_allocations().size());

    assert(mesh_count < shader_consts::CS_BUILD_INDIRECT_FIRST_INSTANCE_GROUP_SIZE_X && "mesh count must be less than CS_BUILD_INDIRECT_FIRST_INSTANCE_GROUP_SIZE_X");

    BuildIndirectFirstInstanceCSPushConstants first_instance_cs_consts = {
        simulation_draw_indirect_commands_buffer.memory_device_ptr(),
        mesh_count
    };

    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_compute_pipeline.vk_pipeline());
    cmd.pushConstants<BuildIndirectFirstInstanceCSPushConstants>(m_pipeline_layout.vk_pipeline_layout(), vk::ShaderStageFlagBits::eCompute, 0, first_instance_cs_consts);
    // 1 group for 1 wave that handles all draw instance prefix sums
    cmd.dispatch(1, 1, 1);
}

// ---- InstanceBufferBuildPass --------------------------------------------------------------------------------------------------------------------------------

InstanceBufferBuildPass::InstanceBufferBuildPass(render_graph::PassDependencies&& deps, VulkanPipelineLayout&& pipeline_layout,
    VulkanComputePipeline&& compute_pipeline)
        : render_graph::PassBase<SimulationRenderState>(std::move(deps)), m_pipeline_layout(std::move(pipeline_layout)), m_compute_pipeline(std::move(compute_pipeline)) {}

auto InstanceBufferBuildPass::create() -> InstanceBufferBuildPass {
    auto dependencies = render_graph::PassDependencies::builder()
        .add_buffer_dependency(render_graph::BufferResourceIdentifier::SimulationDrawIndirectCommands, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderRead)
        .add_buffer_dependency(render_graph::BufferResourceIdentifier::DrawCommandsObjectInstanceOffsets, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderRead)
        .add_buffer_dependency(render_graph::BufferResourceIdentifier::FrameLocalSimulationData, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderRead)
        .add_buffer_dependency(render_graph::BufferResourceIdentifier::SimulationNextObjects, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderRead)
        .add_buffer_dependency(render_graph::BufferResourceIdentifier::ObjectsInstanceBuffer, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderWrite)
        .build();

    auto cs_layout = VulkanPipelineLayout::builder()
        .add_push_constant_range(0, sizeof(BuildInstanceBufferCSPushConstants), vk::ShaderStageFlagBits::eCompute)
        .build();

    auto cs_code = SpirvCode::load_from_file(systems::relative_path() / "spirv/simulation_st6_build_instance_buffer.spv");

    auto cs = VulkanComputePipeline::builder()
        .set_pipeline_layout(cs_layout)
        .set_shader(cs_code)
        .build();

    return InstanceBufferBuildPass(std::move(dependencies), std::move(cs_layout), std::move(cs));
}

auto InstanceBufferBuildPass::invoke(vk::raii::CommandBuffer& cmd, const SimulationRenderState& state, const render_graph::PassResources& resources)
    -> void {
    auto& simulation_draw_indirect_commands_buffer = resources.get_buffer_by_identifier(render_graph::BufferResourceIdentifier::SimulationDrawIndirectCommands);
    auto& draw_commands_object_instance_offsets_buffer = resources.get_buffer_by_identifier(render_graph::BufferResourceIdentifier::DrawCommandsObjectInstanceOffsets);
    auto& frame_local_simulation_data_buffer = resources.get_buffer_by_identifier(render_graph::BufferResourceIdentifier::FrameLocalSimulationData);
    auto& simulation_next_objects_buffer = resources.get_buffer_by_identifier(render_graph::BufferResourceIdentifier::SimulationNextObjects);
    auto& objects_instance_buffer = resources.get_buffer_by_identifier(render_graph::BufferResourceIdentifier::ObjectsInstanceBuffer);

    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_compute_pipeline.vk_pipeline());

    BuildInstanceBufferCSPushConstants instance_buffer_cs_consts = {
        simulation_draw_indirect_commands_buffer.memory_device_ptr(),
        draw_commands_object_instance_offsets_buffer.memory_device_ptr(),
        frame_local_simulation_data_buffer.memory_device_ptr(),
        simulation_next_objects_buffer.memory_device_ptr(),
        objects_instance_buffer.memory_device_ptr(),
        state.object_count
    };
    cmd.pushConstants<BuildInstanceBufferCSPushConstants>(m_pipeline_layout.vk_pipeline_layout(), vk::ShaderStageFlagBits::eCompute, 0, instance_buffer_cs_consts);
    cmd.dispatch(div_ceil(state.object_count, shader_consts::CS_BUILD_INSTANCE_BUFFER_GROUP_SIZE_X), 1, 1);
}

// ---- FishTankRenderPass -------------------------------------------------------------------------------------------------------------------------------------

FishTankRenderPass::FishTankRenderPass(render_graph::PassDependencies&& deps, VulkanPipelineLayout&& pipeline_layout,
    VulkanGraphicsPipeline&& graphics_pipeline)
        : render_graph::PassBase<SimulationRenderState>(std::move(deps)), m_pipeline_layout(std::move(pipeline_layout)), m_graphics_pipeline(std::move(graphics_pipeline)) {}

auto FishTankRenderPass::create() -> FishTankRenderPass {
    auto dependencies = render_graph::PassDependencies::builder()
        .add_buffer_dependency(render_graph::BufferResourceIdentifier::ObjectsInstanceBuffer, vk::PipelineStageFlagBits2::eVertexShader, vk::AccessFlagBits2::eShaderRead)
        .add_buffer_dependency(render_graph::BufferResourceIdentifier::FrameLocalSimulationData, vk::PipelineStageFlagBits2::eVertexShader, vk::AccessFlagBits2::eShaderRead)
        .add_buffer_dependency(render_graph::BufferResourceIdentifier::SimulationDrawIndirectCommands, vk::PipelineStageFlagBits2::eDrawIndirect, vk::AccessFlagBits2::eIndirectCommandRead)
        .add_image_dependency(render_graph::ImageResourceIdentifier::MainRenderTarget, vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite)
        .add_image_dependency(render_graph::ImageResourceIdentifier::DepthBuffer, vk::ImageLayout::eDepthStencilAttachmentOptimal,
            vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
            vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite)
        .build();

    auto pipeline_layout = VulkanPipelineLayout::builder()
        .add_push_constant_range(0, 24, vk::ShaderStageFlagBits::eVertex)
        .build();

    auto pipeline_shader_code = SpirvCode::load_from_file(systems::relative_path() / "spirv/simulation_entity.spv");

    auto pipeline = VulkanGraphicsPipeline::builder()
        .set_pipeline_layout(pipeline_layout)
        .add_shader(pipeline_shader_code, vk::ShaderStageFlagBits::eVertex)
        .add_shader(pipeline_shader_code, vk::ShaderStageFlagBits::eFragment)
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

    return FishTankRenderPass(std::move(dependencies), std::move(pipeline_layout), std::move(pipeline));
}

auto FishTankRenderPass::invoke(vk::raii::CommandBuffer& cmd, const SimulationRenderState& state, const render_graph::PassResources& resources) -> void {
    auto& objects_instance_buffer = resources.get_buffer_by_identifier(render_graph::BufferResourceIdentifier::ObjectsInstanceBuffer);
    auto& frame_local_simulation_data_buffer = resources.get_buffer_by_identifier(render_graph::BufferResourceIdentifier::FrameLocalSimulationData);
    auto& simulation_draw_indirect_commands_buffer = resources.get_buffer_by_identifier(render_graph::BufferResourceIdentifier::SimulationDrawIndirectCommands);
    auto& main_render_target = resources.get_image_by_identifier(render_graph::ImageResourceIdentifier::MainRenderTarget);
    auto& depth_buffer = resources.get_image_by_identifier(render_graph::ImageResourceIdentifier::DepthBuffer);

    auto main_render_target_attachment_info = vk::RenderingAttachmentInfo()
        .setImageView(main_render_target.vk_full_image_view())
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue(vk::ClearColorValue{0.0f, 0.0f, 0.0f, 1.0f});

    auto depth_buffer_attachment_info = vk::RenderingAttachmentInfo()
        .setImageView(depth_buffer.vk_full_image_view())
        .setImageLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue(vk::ClearDepthStencilValue{0.0f, 0});

    auto rendering_area = vk::Extent2D(main_render_target.extent().width, main_render_target.extent().height);

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

    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphics_pipeline.vk_pipeline());

    cmd.bindIndexBuffer(state.mesh_pool.get().index_buffer().vk_buffer(), 0, vk::IndexType::eUint32);

    struct PushConstants {
        vk::DeviceAddress vertex_buffer;
        vk::DeviceAddress prepared_simulation_objects;
        vk::DeviceAddress simulation_data;
    };

    PushConstants consts = {
        state.mesh_pool.get().vertex_buffer().memory_device_ptr(),
        objects_instance_buffer.memory_device_ptr(),
        frame_local_simulation_data_buffer.memory_device_ptr()
    };

    cmd.pushConstants<PushConstants>(m_pipeline_layout.vk_pipeline_layout(), vk::ShaderStageFlagBits::eVertex, 0, consts);
    cmd.drawIndexedIndirect(simulation_draw_indirect_commands_buffer.vk_buffer(), 0, static_cast<uint32_t>(state.mesh_pool.get().mesh_allocations().size()), sizeof(vk::DrawIndexedIndirectCommand));

    cmd.endRendering();
}

// ---- ImGuiRenderPass ----------------------------------------------------------------------------------------------------------------------------------------

ImGuiRenderPass::ImGuiRenderPass(render_graph::PassDependencies&& deps)
    : render_graph::PassBase<SimulationRenderState>(std::move(deps)) {}

auto ImGuiRenderPass::create() -> ImGuiRenderPass {
    auto dependencies = render_graph::PassDependencies::builder()
        .add_image_dependency(render_graph::ImageResourceIdentifier::MainRenderTarget, vk::ImageLayout::eColorAttachmentOptimal,
            vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite)
        .build();

    return ImGuiRenderPass(std::move(dependencies));
}

auto ImGuiRenderPass::invoke(vk::raii::CommandBuffer& cmd, const SimulationRenderState& state, const render_graph::PassResources& resources) -> void {
    auto& main_render_target = resources.get_image_by_identifier(render_graph::ImageResourceIdentifier::MainRenderTarget);

    auto main_render_target_attachment_info = vk::RenderingAttachmentInfo()
        .setImageView(main_render_target.vk_full_image_view())
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eLoad)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue(vk::ClearColorValue{0.0f, 0.0f, 0.0f, 1.0f});

    auto rendering_area = vk::Extent2D(main_render_target.extent().width, main_render_target.extent().height);

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

    if (state.imgui_draw_data) {
        ImGui_ImplVulkan_RenderDrawData(state.imgui_draw_data, *cmd);
    }

    cmd.endRendering();
}

// ---- BlitMainImageToSwapchainPass ---------------------------------------------------------------------------------------------------------------------------

BlitMainImageToSwapchainPass::BlitMainImageToSwapchainPass(render_graph::PassDependencies&& deps)
    : render_graph::PassBase<SimulationRenderState>(std::move(deps)) {}

auto BlitMainImageToSwapchainPass::create() -> BlitMainImageToSwapchainPass {
    auto dependencies = render_graph::PassDependencies::builder()
        .add_image_dependency(render_graph::ImageResourceIdentifier::MainRenderTarget, vk::ImageLayout::eTransferSrcOptimal,
            vk::PipelineStageFlagBits2::eBlit, vk::AccessFlagBits2::eTransferRead)
        .build();

    return BlitMainImageToSwapchainPass(std::move(dependencies));
}

auto BlitMainImageToSwapchainPass::invoke(vk::raii::CommandBuffer& cmd, const SimulationRenderState& state, const render_graph::PassResources& resources) -> void {
    auto& main_render_target = resources.get_image_by_identifier(render_graph::ImageResourceIdentifier::MainRenderTarget);

    // Swapchain images are managed by the driver, which doesn't play too well with the current abstraction over images, so we synchronize access to it manually.

    VulkanPipelineBarriers::builder()
        .insert_image_memory_barrier(state.current_swapchain_image.get().vk_image(),
            vk::ImageLayout::eUndefined, vk::PipelineStageFlagBits2::eAllCommands, vk::AccessFlagBits2::eNone,
            vk::ImageLayout::eTransferDstOptimal, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
            state.current_swapchain_image.get().full_subresource_range()
        )
        .flush(cmd);

    auto blit_offsets = std::array<vk::Offset3D, 2>{{{0, 0, 0}, {to_offset3d(main_render_target.extent())}}};

    auto blit_subresources = vk::ImageSubresourceLayers()
        .setAspectMask(vk::ImageAspectFlagBits::eColor)
        .setBaseArrayLayer(0)
        .setLayerCount(1)
        .setMipLevel(0);

    auto blit_region = vk::ImageBlit2()
        .setSrcOffsets(blit_offsets)
        .setDstOffsets(blit_offsets)
        .setSrcSubresource(blit_subresources)
        .setDstSubresource(blit_subresources);

    auto blit_info = vk::BlitImageInfo2()
        .setSrcImage(main_render_target.vk_image())
        .setSrcImageLayout(vk::ImageLayout::eTransferSrcOptimal)
        .setDstImage(state.current_swapchain_image.get().vk_image())
        .setDstImageLayout(vk::ImageLayout::eTransferDstOptimal)
        .setFilter(vk::Filter::eNearest)
        .setRegions(blit_region);

    cmd.blitImage2(blit_info);

    VulkanPipelineBarriers::builder()
        .insert_image_memory_barrier(state.current_swapchain_image.get().vk_image(),
            vk::ImageLayout::eTransferDstOptimal, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
            vk::ImageLayout::ePresentSrcKHR, vk::PipelineStageFlagBits2::eAllCommands, vk::AccessFlagBits2::eNone,
            state.current_swapchain_image.get().full_subresource_range()
        )
        .flush(cmd);
}

} // namespace pop::vulkan::renderer
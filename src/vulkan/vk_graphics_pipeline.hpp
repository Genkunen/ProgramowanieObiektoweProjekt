#pragma once
#include "vk_graphics_pipeline.hpp"
#include "vk_pipeline_barriers.hpp"
#include "vk_pipeline_layout.hpp"

#include <ranges>

namespace pop::vulkan {

class VulkanGraphicsPipelineBuilder;
class VulkanGraphicsPipeline {
public:
    VulkanGraphicsPipeline(vk::raii::Pipeline&& pipeline);

    constexpr static auto builder() -> VulkanGraphicsPipelineBuilder;

    [[nodiscard]] auto vk_pipeline() const -> const vk::raii::Pipeline& { return m_pipeline; }

private:
    vk::raii::Pipeline m_pipeline;
};

class VulkanGraphicsPipelineBuilder {
public:
    constexpr VulkanGraphicsPipelineBuilder() = default;

    VulkanGraphicsPipelineBuilder(const VulkanGraphicsPipelineBuilder&) = delete;
    VulkanGraphicsPipelineBuilder(VulkanGraphicsPipelineBuilder&&) = default;
    VulkanGraphicsPipelineBuilder& operator=(const VulkanGraphicsPipelineBuilder&) = delete;
    VulkanGraphicsPipelineBuilder& operator=(VulkanGraphicsPipelineBuilder&&) = default;

    [[nodiscard]] constexpr auto add_shader(const std::span<const uint32_t>& shader_code, vk::ShaderStageFlagBits stage) noexcept -> VulkanGraphicsPipelineBuilder& {
        auto stage_tuple = vk::StructureChain<vk::PipelineShaderStageCreateInfo, vk::ShaderModuleCreateInfo>{
            vk::PipelineShaderStageCreateInfo()
                .setStage(stage)
                .setPName("main"),
            vk::ShaderModuleCreateInfo()
                .setCode(shader_code)
        };
        m_shader_stages.emplace_back(stage_tuple);
        return *this;
    }
    [[nodiscard]] constexpr auto set_pipeline_layout(const VulkanPipelineLayout& layout) noexcept -> VulkanGraphicsPipelineBuilder& {
        m_pipeline_layout = layout.vk_pipeline_layout();
        return *this;
    }
    [[nodiscard]] constexpr auto set_input_topology(vk::PrimitiveTopology topology) noexcept -> VulkanGraphicsPipelineBuilder& {
        m_input_assembly_state.topology = topology;
        return *this;
    }
    [[nodiscard]] constexpr auto set_rasterizer_line_width(float line_width) noexcept -> VulkanGraphicsPipelineBuilder& {
        m_rasterization_state.lineWidth = line_width;
        return *this;
    }
    [[nodiscard]] constexpr auto set_rasterizer_polygon_mode(vk::PolygonMode polygon_mode) noexcept -> VulkanGraphicsPipelineBuilder& {
        m_rasterization_state.polygonMode = polygon_mode;
        return *this;
    }
    [[nodiscard]] constexpr auto set_rasterizer_cull_mode(vk::CullModeFlags cull_mode_flags, vk::FrontFace front_face) noexcept -> VulkanGraphicsPipelineBuilder& {
        m_rasterization_state.cullMode = cull_mode_flags;
        m_rasterization_state.frontFace = front_face;
        return *this;
    }
    [[nodiscard]] constexpr auto disable_multisampling() noexcept -> VulkanGraphicsPipelineBuilder& {
        m_multisample_state.sampleShadingEnable = false;
        m_multisample_state.rasterizationSamples = vk::SampleCountFlagBits::e1;
        m_multisample_state.minSampleShading = 1.0;
        m_multisample_state.alphaToCoverageEnable = false;
        m_multisample_state.alphaToOneEnable = false;
        return *this;
    }
    [[nodiscard]] constexpr auto add_rendering_attachment(vk::PipelineColorBlendAttachmentState blending, vk::Format attachment_format) noexcept -> VulkanGraphicsPipelineBuilder& {
        m_rendering_color_attachment_blend_states.emplace_back(blending);
        m_rendering_color_attachment_formats.emplace_back(attachment_format);
        return *this;
    }
    [[nodiscard]] constexpr auto disable_depth_test() noexcept -> VulkanGraphicsPipelineBuilder& {
        m_depth_stencil_state.depthTestEnable = false;
        m_depth_stencil_state.depthWriteEnable = false;
        m_depth_stencil_state.depthCompareOp = vk::CompareOp::eNever;
        m_depth_stencil_state.depthBoundsTestEnable = false;
        m_depth_stencil_state.stencilTestEnable = false;
        m_depth_stencil_state.minDepthBounds = 0.0;
        m_depth_stencil_state.maxDepthBounds = 1.0;
        return *this;
    }
    [[nodiscard]] constexpr auto enable_depth_test(bool write) noexcept -> VulkanGraphicsPipelineBuilder& {
        m_depth_stencil_state.depthTestEnable = true;
        m_depth_stencil_state.depthWriteEnable = write;
        m_depth_stencil_state.depthCompareOp = vk::CompareOp::eGreater;
        m_depth_stencil_state.depthBoundsTestEnable = false;
        m_depth_stencil_state.stencilTestEnable = false;
        m_depth_stencil_state.minDepthBounds = 0.0;
        m_depth_stencil_state.maxDepthBounds = 1.0;
        return *this;
    }
    [[nodiscard]] constexpr auto set_depth_attachment_format(vk::Format format) noexcept -> VulkanGraphicsPipelineBuilder& {
        m_rendering_create_info.depthAttachmentFormat = format;
        return *this;
    }
    [[nodiscard]] constexpr auto build() -> VulkanGraphicsPipeline {
        auto viewport_state = vk::PipelineViewportStateCreateInfo()
            .setViewportCount(1)
            .setScissorCount(1);

        auto vertex_input_state = vk::PipelineVertexInputStateCreateInfo();

        auto color_blend_state = vk::PipelineColorBlendStateCreateInfo()
            .setLogicOpEnable(false)
            .setAttachments(m_rendering_color_attachment_blend_states);

        auto dynamic_state_list = std::array<vk::DynamicState, 2>{vk::DynamicState::eViewport, vk::DynamicState::eScissor};
        auto dynamic_state = vk::PipelineDynamicStateCreateInfo()
            .setDynamicStates(dynamic_state_list);
        m_rendering_create_info = m_rendering_create_info.setColorAttachmentFormats(m_rendering_color_attachment_formats);

        auto raw_stage_list = m_shader_stages
            | std::views::transform([](const vk::StructureChain<vk::PipelineShaderStageCreateInfo, vk::ShaderModuleCreateInfo>& chain) -> vk::PipelineShaderStageCreateInfo { return chain.get<vk::PipelineShaderStageCreateInfo>(); })
            | std::ranges::to<std::vector>();

        auto pipelineInfo = vk::GraphicsPipelineCreateInfo()
            .setStages(raw_stage_list)
            .setPVertexInputState(&vertex_input_state)
            .setPInputAssemblyState(&m_input_assembly_state)
            .setPViewportState(&viewport_state)
            .setPRasterizationState(&m_rasterization_state)
            .setPMultisampleState(&m_multisample_state)
            .setPColorBlendState(&color_blend_state)
            .setPDepthStencilState(&m_depth_stencil_state)
            .setPDynamicState(&dynamic_state)
            .setLayout(m_pipeline_layout);

        auto sc = vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo>(pipelineInfo, m_rendering_create_info);

        auto pipeline = VulkanContext::get().vk_device().createGraphicsPipeline(nullptr, sc.get<vk::GraphicsPipelineCreateInfo>());

        return VulkanGraphicsPipeline(std::move(pipeline));
    }

private:
    vk::PipelineLayout m_pipeline_layout = nullptr;
    vk::PipelineInputAssemblyStateCreateInfo m_input_assembly_state = {};
    vk::PipelineRasterizationStateCreateInfo m_rasterization_state = {};
    vk::PipelineMultisampleStateCreateInfo m_multisample_state = {};
    vk::PipelineDepthStencilStateCreateInfo m_depth_stencil_state = {};
    vk::PipelineRenderingCreateInfo m_rendering_create_info = {};
    std::vector<vk::PipelineColorBlendAttachmentState> m_rendering_color_attachment_blend_states;
    std::vector<vk::Format> m_rendering_color_attachment_formats;

    std::vector<vk::StructureChain<vk::PipelineShaderStageCreateInfo, vk::ShaderModuleCreateInfo>> m_shader_stages;
};

constexpr auto VulkanGraphicsPipeline::builder() -> VulkanGraphicsPipelineBuilder { return VulkanGraphicsPipelineBuilder(); }

}
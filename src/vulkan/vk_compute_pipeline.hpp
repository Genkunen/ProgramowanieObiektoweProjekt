#pragma once
#include "vk_compute_pipeline.hpp"
#include "vk_graphics_pipeline.hpp"

namespace pop::vulkan {

class VulkanComputePipelineBuilder;
class VulkanComputePipeline {
public:
    VulkanComputePipeline(vk::raii::Pipeline&& pipeline);

    constexpr static auto builder() -> VulkanComputePipelineBuilder;

    [[nodiscard]] auto vk_pipeline() const noexcept -> const vk::raii::Pipeline& { return m_pipeline; }

private:
    vk::raii::Pipeline m_pipeline;
};

class VulkanComputePipelineBuilder {
public:
    constexpr VulkanComputePipelineBuilder() = default;

    VulkanComputePipelineBuilder(const VulkanComputePipelineBuilder&) = delete;
    VulkanComputePipelineBuilder(VulkanComputePipelineBuilder&&) = default;
    VulkanComputePipelineBuilder& operator=(const VulkanComputePipelineBuilder&) = delete;
    VulkanComputePipelineBuilder& operator=(VulkanComputePipelineBuilder&&) = default;

    [[nodiscard]] constexpr auto set_shader(const SpirvCode& shader_code) -> VulkanComputePipelineBuilder& {
        auto stage_tuple = vk::StructureChain{
            vk::PipelineShaderStageCreateInfo()
                .setStage(vk::ShaderStageFlagBits::eCompute)
                .setPName("main"),
            shader_code.vulkan_shader_module_create_info(),
            vk::PipelineShaderStageRequiredSubgroupSizeCreateInfo{}
        };
        m_shader_stage_create_info = stage_tuple;
        return *this;
    }
    [[nodiscard]] constexpr auto set_pipeline_layout(const VulkanPipelineLayout& layout) noexcept -> VulkanComputePipelineBuilder& {
        m_pipeline_layout = layout.vk_pipeline_layout();
        return *this;
    }
    [[nodiscard]] constexpr auto set_preferred_wave_lane_count(uint32_t lane_count) noexcept -> VulkanComputePipelineBuilder& {
        assert(lane_count >= 2 && (lane_count & (lane_count - 1)) == 0 && "lane_count must be a power of 2");
        m_preferred_wave_lane_count = lane_count;
        return *this;
    }
    [[nodiscard]] constexpr auto build() -> VulkanComputePipeline {
        auto& device_vk13_props = VulkanContext::get().physical_device_vulkan13_properties();
        if (m_preferred_wave_lane_count) {
            uint32_t wave_lane_count = std::clamp(*m_preferred_wave_lane_count, device_vk13_props.minSubgroupSize, device_vk13_props.maxSubgroupSize);
            m_shader_stage_create_info.get<vk::PipelineShaderStageRequiredSubgroupSizeCreateInfo>().requiredSubgroupSize = wave_lane_count;
        } else {
            m_shader_stage_create_info.unlink<vk::PipelineShaderStageRequiredSubgroupSizeCreateInfo>();
        }

        auto compute_pipeline_create_info = vk::ComputePipelineCreateInfo()
            .setStage(m_shader_stage_create_info.get<vk::PipelineShaderStageCreateInfo>())
            .setLayout(m_pipeline_layout);


        auto pipeline = VulkanContext::get().vk_device().createComputePipeline(nullptr, compute_pipeline_create_info);
        return VulkanComputePipeline(std::move(pipeline));
    }

private:
    vk::PipelineLayout m_pipeline_layout{ nullptr };
    vk::StructureChain<vk::PipelineShaderStageCreateInfo, vk::ShaderModuleCreateInfo, vk::PipelineShaderStageRequiredSubgroupSizeCreateInfo> m_shader_stage_create_info{};

    std::optional<uint32_t> m_preferred_wave_lane_count{};
};

constexpr auto VulkanComputePipeline::builder() -> VulkanComputePipelineBuilder { return VulkanComputePipelineBuilder(); }

} // namespace pop::vulkan

#pragma once
#include "vk_prelude.hpp"
#include "vk_context.hpp"

namespace pop::vulkan {

class VulkanPipelineLayoutBuilder;

/// @class VulkanPipelineLayout
/// @brief Wrapper around a Vulkan @c vk::raii::PipelineLayout object.
class VulkanPipelineLayout {
public:
    VulkanPipelineLayout(vk::raii::PipelineLayout&& pipeline_layout);

    [[nodiscard]] constexpr static auto builder() -> VulkanPipelineLayoutBuilder;

    [[nodiscard]] auto vk_pipeline_layout() const noexcept -> const vk::raii::PipelineLayout& { return m_pipeline_layout; }
private:
    vk::raii::PipelineLayout m_pipeline_layout;
};

/// @class VulkanPipelineLayoutBuilder
/// @brief A builder for a @c VulkanPipelineLayout
class VulkanPipelineLayoutBuilder {
public:
    constexpr VulkanPipelineLayoutBuilder() = default;

    VulkanPipelineLayoutBuilder(const VulkanPipelineLayoutBuilder&) = delete;
    VulkanPipelineLayoutBuilder(VulkanPipelineLayoutBuilder&&) = default;
    VulkanPipelineLayoutBuilder& operator=(const VulkanPipelineLayoutBuilder&) = delete;
    VulkanPipelineLayoutBuilder& operator=(VulkanPipelineLayoutBuilder&&) = default;

    /// @brief Adds a descriptor set layout to the pipeline layout.
    /// @param layout The descriptor set layout to add.
    [[nodiscard]] constexpr auto add_descriptor_set_layout(vk::DescriptorSetLayout layout) noexcept -> VulkanPipelineLayoutBuilder& {
        m_descriptor_set_layouts.emplace_back(layout);
        return *this;
    }

    /// @brief Adds a push constant range to the pipeline layout.
    /// @param offset The offset of the push constant range.
    /// @param size The size of the push constant range.
    /// @param stage_flags The shader stages to which the push constant range will be exposed.
    [[nodiscard]] constexpr auto add_push_constant_range(uint32_t offset, uint32_t size, vk::ShaderStageFlags stage_flags) noexcept -> VulkanPipelineLayoutBuilder& {
        auto range = vk::PushConstantRange()
            .setStageFlags(stage_flags)
            .setOffset(offset)
            .setSize(size);

        m_push_constant_ranges.emplace_back(range);
        return *this;
    }

    /// @brief Builds the pipeline layout's @c vk::PipelineLayoutCreateInfo
    [[nodiscard]] auto build_create_info() const noexcept -> vk::PipelineLayoutCreateInfo {
        return vk::PipelineLayoutCreateInfo().setSetLayouts(m_descriptor_set_layouts).setPushConstantRanges(m_push_constant_ranges);
    }

    /// @brief Builds the pipeline layout.
    /// @returns A new @c VulkanPipelineLayout object with the set descriptor set layouts and push constant ranges.
    [[nodiscard]] auto build() const -> VulkanPipelineLayout {
        auto pipeline_layout = VulkanContext::get().vk_device().createPipelineLayout(build_create_info());

        return VulkanPipelineLayout(std::move(pipeline_layout));
    }

private:
    std::vector<vk::PushConstantRange> m_push_constant_ranges;
    std::vector<vk::DescriptorSetLayout> m_descriptor_set_layouts;
};

constexpr auto VulkanPipelineLayout::builder() -> VulkanPipelineLayoutBuilder { return VulkanPipelineLayoutBuilder(); }

} // namespace pop::vulkan
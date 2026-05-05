#pragma once
#include "render_graph_pass_resources.hpp"
#include "vulkan/vk_buffer.hpp"

namespace pop::vulkan::renderer::render_graph {

struct BufferDependency {
    BufferResourceIdentifier resource_id;
    vk::PipelineStageFlags2 stage;
    vk::AccessFlags2 access;
};

struct ImageDependency {
    ImageResourceIdentifier resource_id;
    vk::ImageLayout layout;
    vk::PipelineStageFlags2 stage;
    vk::AccessFlags2 access;
};

class PassDependenciesBuilder;
class PassDependencies {
public:
    PassDependencies(std::vector<BufferDependency>&& buffer_dependencies, std::vector<ImageDependency>&& image_dependencies);

    constexpr static auto builder() -> PassDependenciesBuilder;

    [[nodiscard]] auto buffer_dependencies() const noexcept -> const std::vector<BufferDependency>& { return m_buffer_dependencies; }
    [[nodiscard]] auto image_dependencies() const noexcept -> const std::vector<ImageDependency>& { return m_image_dependencies; }

private:
    std::vector<BufferDependency> m_buffer_dependencies;
    std::vector<ImageDependency> m_image_dependencies;
};

class PassDependenciesBuilder {
public:
    constexpr PassDependenciesBuilder() = default;

    [[nodiscard]] constexpr auto add_buffer_dependency(BufferResourceIdentifier resource_id, vk::PipelineStageFlags2 stage, vk::AccessFlags2 access) noexcept -> PassDependenciesBuilder& {
        auto dep = BufferDependency{ resource_id, stage, access };
        m_buffer_dependencies.emplace_back(dep);
        return *this;
    }
    [[nodiscard]] constexpr auto add_image_dependency(ImageResourceIdentifier resource_id, vk::ImageLayout layout, vk::PipelineStageFlags2 stage, vk::AccessFlags2 access) noexcept -> PassDependenciesBuilder& {
        auto dep = ImageDependency{ resource_id, layout, stage, access };
        m_image_dependencies.emplace_back(dep);
        return *this;
    }

    [[nodiscard]] constexpr auto build() noexcept -> PassDependencies {
        return PassDependencies(std::move(m_buffer_dependencies), std::move(m_image_dependencies));
    }

private:
    std::vector<BufferDependency> m_buffer_dependencies;
    std::vector<ImageDependency> m_image_dependencies;
};
constexpr auto PassDependencies::builder() -> PassDependenciesBuilder { return PassDependenciesBuilder(); }

template <typename StateType> class PassBase {
public:
    PassBase(PassDependencies&& dependencies) : m_dependencies(std::move(dependencies)) {}
    virtual ~PassBase() = default;

    [[nodiscard]] auto dependencies() const noexcept -> const PassDependencies& { return m_dependencies; }

    auto is_enabled() const noexcept -> bool { return m_is_enabled; }
    auto enable() noexcept -> void { m_is_enabled = true; }
    auto disable() noexcept -> void { m_is_enabled = false; }

    virtual auto invoke(vk::raii::CommandBuffer& cmd, const StateType& state, const PassResources& resources) -> void = 0;

protected:
    PassDependencies m_dependencies;

private:
    bool m_is_enabled = true;
};

}
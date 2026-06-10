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

/// @class PassDependencies
/// @brief Describes the image and buffer dependencies of a render graph pass.
class PassDependencies {
public:
    PassDependencies(std::vector<BufferDependency>&& buffer_dependencies, std::vector<ImageDependency>&& image_dependencies);

    constexpr static auto builder() -> PassDependenciesBuilder;

    /// @brief Returns the buffer dependencies of the pass.
    [[nodiscard]] auto buffer_dependencies() const noexcept -> const std::vector<BufferDependency>& { return m_buffer_dependencies; }
    /// @brief Returns the image dependencies of the pass.
    [[nodiscard]] auto image_dependencies()  const noexcept -> const std::vector<ImageDependency>& { return m_image_dependencies; }

private:
    std::vector<BufferDependency> m_buffer_dependencies;
    std::vector<ImageDependency> m_image_dependencies;
};

/// @class PassDependenciesBuilder
/// @brief Builder for PassDependencies.
class PassDependenciesBuilder {
public:
    constexpr PassDependenciesBuilder() = default;

    /// @brief Adds a buffer dependency to the pass.
    /// @param resource_id The identifier of the buffer resource.
    /// @param stage The pipeline stage that the buffer resource will be accessed in.
    /// @param access The access flags that the buffer resource will be accessed with.
    [[nodiscard]] constexpr auto add_buffer_dependency(BufferResourceIdentifier resource_id, vk::PipelineStageFlags2 stage, vk::AccessFlags2 access) noexcept -> PassDependenciesBuilder& {
        auto dep = BufferDependency{ resource_id, stage, access };
        m_buffer_dependencies.emplace_back(dep);
        return *this;
    }

    /// @brief Adds an image dependency to the pass.
    /// @param resource_id The identifier of the image resource.
    /// @param layout The image layout that the image resource will be in.
    /// @param stage The pipeline stage that the image resource will be accessed in.
    /// @param access The access flags that the image resource will be accessed with.
    [[nodiscard]] constexpr auto add_image_dependency(ImageResourceIdentifier resource_id, vk::ImageLayout layout, vk::PipelineStageFlags2 stage, vk::AccessFlags2 access) noexcept -> PassDependenciesBuilder& {
        auto dep = ImageDependency{ resource_id, layout, stage, access };
        m_image_dependencies.emplace_back(dep);
        return *this;
    }

    /// @brief Builds the PassDependencies object.
    [[nodiscard]] auto build() noexcept -> PassDependencies {
        return PassDependencies(std::move(m_buffer_dependencies), std::move(m_image_dependencies));
    }

private:
    std::vector<BufferDependency> m_buffer_dependencies;
    std::vector<ImageDependency> m_image_dependencies;
};
constexpr auto PassDependencies::builder() -> PassDependenciesBuilder { return PassDependenciesBuilder(); }

/// @class PassBase
/// @brief Base class for all render graph passes.
template <typename StateType> class PassBase {
public:
    PassBase(PassDependencies&& dependencies) : m_dependencies(std::move(dependencies)) {}
    virtual ~PassBase() = default;

    [[nodiscard]] auto dependencies() const noexcept -> const PassDependencies& { return m_dependencies; }

    virtual auto debug_name() const noexcept -> std::string { return std::format("(unnamed pass 0x{:016x})", reinterpret_cast<uintptr_t>(this)); }
    auto is_enabled() const noexcept -> bool { return m_is_enabled; }
    auto enable() noexcept -> void { m_is_enabled = true; }
    auto disable() noexcept -> void { m_is_enabled = false; }

    /// Invokes the commands for this render graph pass.
    /// @param cmd The command buffer to use for the pass.
    /// @param state The state of the render graph.
    /// @param resources The resources required by the pass.
    virtual auto invoke(vk::raii::CommandBuffer& cmd, const StateType& state, const PassResources& resources) -> void = 0;

protected:
    PassDependencies m_dependencies;

private:
    bool m_is_enabled = true;
};

}
#pragma once
#include "render_graph_pass.hpp"

namespace pop::vulkan::renderer::render_graph {

/// @brief Helper function for returning an access flag set with the write bits retained only.
inline auto mask_access_flags_with_write_bit(vk::AccessFlags2 access_flags) -> vk::AccessFlags2 {
    vk::AccessFlags2 mask = vk::AccessFlagBits2::eHostWrite | vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eShaderWrite
        | vk::AccessFlagBits2::eTransferWrite | vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eShaderStorageWrite
        | vk::AccessFlagBits2::eDepthStencilAttachmentWrite;
    return access_flags & mask;
}

/// @brief Helper function for checking if an access flag set has any flag that is a write bit.
inline auto access_flags_has_write_aspect(vk::AccessFlags2 access_flags) -> bool {
    return mask_access_flags_with_write_bit(access_flags) != vk::AccessFlags2{};
}

/// @struct PassIndexV2
/// @brief A pass index for the render graph.
struct PassIndexV2 {
    uint32_t index;
};

/// @class RenderGraphV2
/// @brief A render graph implementation, which uses a topological sort to execute passes in a correct order, and a straightforward barrier generation algorithm
///     to manage synchronization hazards between passes.
template <typename State> class RenderGraphV2 {
public:
    constexpr RenderGraphV2() = default;

    /// @brief Adds a pass to the render graph.
    /// @param pass The pass to add.
    /// @return The index of the pass in the render graph.
    auto add_pass(std::unique_ptr<PassBase<State>>&& pass) -> PassIndexV2 {
        m_passes.emplace_back(std::move(pass));
        return { static_cast<uint32_t>(m_passes.size() - 1) };
    }

    /// @brief Returns a handle to the render graph pass with a given pass index.
    auto get_pass_by_id(PassIndexV2 id) -> PassBase<State>& { return *m_passes[id.index].pass; }

    /// @brief Adds a dependency between two passes.
    /// @param from The index of the pass that depends on the other pass.
    /// @param to The index of the pass that is depended on by the other pass.
    auto add_dependency_edge(PassIndexV2 from, PassIndexV2 to) -> void {
        assert(from.index < m_passes.size() && to.index < m_passes.size() && "invalid pass index");

        m_passes[from.index].children.emplace_back(to);
        m_passes[to.index].indegree++;
    }

    /// @brief Executes the render graph.
    /// @param cmd The command buffer to execute the render graph on.
    /// @param state The state to pass to the render graph passes.
    /// @param resources The resources to pass to the render graph passes.
    auto execute(vk::raii::CommandBuffer& cmd, State& state, PassResources& resources) -> void {
        bool insert_debug_labels = VulkanContext::get().debug_utils_enabled();

        auto exec_nodes = graph_toposort_to_exec_nodes();
        generate_barriers_for_exec_nodes(exec_nodes, resources);

        for (auto& node : exec_nodes) {
            auto dependency_info = vk::DependencyInfo{}
                .setMemoryBarriers(node.global_memory_barriers)
                .setImageMemoryBarriers(node.image_memory_barriers);

            cmd.pipelineBarrier2(dependency_info);

            for (auto& pass : node.passes) {
                auto& pass_object = get_pass_by_id(pass);
                if (insert_debug_labels) {
                    auto debug_name = pass_object.debug_name();
                    auto debug_label = vk::DebugUtilsLabelEXT{}
                        .setPLabelName(debug_name.c_str());

                    cmd.beginDebugUtilsLabelEXT(debug_label);
                }

                if (pass_object.is_enabled()) {
                    pass_object.invoke(cmd, state, resources);
                }

                if (insert_debug_labels) {
                    cmd.endDebugUtilsLabelEXT();
                }
            }

        }
    }

    /// @brief Resets the image layout for a given image resource identifier.
    /// @param id The identifier of the image resource.
    auto reset_image_layout_for_image(ImageResourceIdentifier id) -> void {
        m_last_image_layouts[id] = vk::ImageLayout::eUndefined;
    }

private:
    // ---- Helper structs for tracking resource usage across passes -------------------------------------------------------------------------------------------

    struct MemoryUsageState {
        vk::PipelineStageFlags2 stages = vk::PipelineStageFlags2{};
        vk::AccessFlags2 accesses      = vk::AccessFlags2{};
    };

    MemoryUsageState m_last_global_memory_usage;
    std::unordered_map<ImageResourceIdentifier, vk::ImageLayout> m_last_image_layouts;

    // ---- Render graph internals -----------------------------------------------------------------------------------------------------------------------------

    struct ExecNode {
        std::vector<PassIndexV2> passes;
        std::vector<vk::MemoryBarrier2> global_memory_barriers = {};
        std::vector<vk::ImageMemoryBarrier2> image_memory_barriers = {};
    };

    struct PassNode {
        std::unique_ptr<PassBase<State>> pass;
        std::vector<PassIndexV2> children = {};
        int indegree = 0;
    };

    std::vector<PassNode> m_passes;

    auto graph_toposort_to_exec_nodes() -> std::vector<ExecNode> {
        std::vector<ExecNode> exec_nodes;

        // Copy all indegree values so they can be worked on locally in this function.
        std::vector<int> indegrees(m_passes.size(), 0);
        for (size_t i = 0; i < m_passes.size(); i++) {
            indegrees[i] = m_passes[i].indegree;
        }

        // First, find roots to start from.
        std::vector<PassIndexV2> active_passes;
        for (size_t i = 0; i < indegrees.size(); i++) {
            if (indegrees[i] == 0) {
                active_passes.push_back({ static_cast<uint32_t>(i) });
            }
        }

        while (true) {
            if (active_passes.empty()) {
                for (size_t i = 0; i < indegrees.size(); i++) {
                    if (indegrees[i] != 0) {
                        throw std::runtime_error("invalid render graph, or cycle in render graph detected");
                    }
                }
                // otherwise every node has been run through
                break;
            }

            std::vector<PassIndexV2> passes_active_now(active_passes.begin(), active_passes.end());
            active_passes.clear();

            for (auto& pass : passes_active_now) {
                auto& children = m_passes[pass.index].children;
                for (auto& child : children) {
                    indegrees[child.index]--;
                    if (indegrees[child.index] == 0) {
                        active_passes.push_back(child);
                    }
                }
            }

            exec_nodes.emplace_back(std::move(passes_active_now));
        }

        return exec_nodes;
    }

    auto generate_barriers_for_exec_nodes(std::vector<ExecNode>& exec_nodes, const PassResources& resources) -> void {
        for (auto& node : exec_nodes) {
            MemoryUsageState memory_usage = {};
            std::vector<std::tuple<ImageResourceIdentifier, vk::ImageLayout, vk::ImageLayout>> image_layouts;
            for (auto& pass : node.passes) {
                auto dep = m_passes[pass.index].pass->dependencies();
                auto buffer_deps = dep.buffer_dependencies();

                for (auto& buffer_dep : buffer_deps) {
                    memory_usage.stages |= buffer_dep.stage;
                    memory_usage.accesses |= buffer_dep.access;
                }

                auto image_deps = dep.image_dependencies();
                for (auto& image_dep : image_deps) {
                    memory_usage.stages |= image_dep.stage;
                    memory_usage.accesses |= image_dep.access;
                    if (image_dep.layout != m_last_image_layouts[image_dep.resource_id]) {
                        auto old_layout = m_last_image_layouts[image_dep.resource_id];
                        m_last_image_layouts[image_dep.resource_id] = image_dep.layout;
                        image_layouts.emplace_back(image_dep.resource_id, old_layout, image_dep.layout);
                    }
                }
            }

            auto global_memory_barrier = vk::MemoryBarrier2{}
                .setSrcStageMask(m_last_global_memory_usage.stages)
                .setSrcAccessMask(mask_access_flags_with_write_bit(m_last_global_memory_usage.accesses))
                .setDstStageMask(memory_usage.stages)
                .setDstAccessMask(memory_usage.accesses);

            node.global_memory_barriers.emplace_back(global_memory_barrier);

            for (auto& [image_id, old_layout, new_layout] : image_layouts) {
                auto barrier = vk::ImageMemoryBarrier2{}
                    .setImage(resources.get_image_by_identifier(image_id).vk_image())
                    .setSubresourceRange(resources.get_image_by_identifier(image_id).full_subresource_range())
                    .setSrcStageMask(m_last_global_memory_usage.stages)
                    .setSrcAccessMask(m_last_global_memory_usage.accesses)
                    .setDstStageMask(memory_usage.stages)
                    .setDstAccessMask(memory_usage.accesses)
                    .setOldLayout(old_layout)
                    .setNewLayout(new_layout);

                node.image_memory_barriers.emplace_back(barrier);
            }

            m_last_global_memory_usage = memory_usage;
        }
    }
};

} // namespace pop::vulkan::renderer::render_graph
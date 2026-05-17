#pragma once
#include "render_graph_pass.hpp"

#include <print>

namespace pop::vulkan::renderer::render_graph {

inline auto mask_access_flags_with_write_bit(vk::AccessFlags2 access_flags) -> vk::AccessFlags2 {
    vk::AccessFlags2 mask = vk::AccessFlagBits2::eHostWrite | vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eShaderWrite
        | vk::AccessFlagBits2::eTransferWrite | vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eShaderStorageWrite
        | vk::AccessFlagBits2::eDepthStencilAttachmentWrite;
    return access_flags & mask;
}

inline auto access_flags_has_write_aspect(vk::AccessFlags2 access_flags) -> bool {
    return mask_access_flags_with_write_bit(access_flags) != vk::AccessFlags2{};
}

struct PassIndexV2 {
    uint32_t index;
};

template <typename State> class RenderGraphV2 {
public:


    auto add_pass(std::unique_ptr<PassBase<State>>&& pass) -> PassIndexV2 {
        m_passes.emplace_back(std::move(pass));
        return { static_cast<uint32_t>(m_passes.size() - 1) };
    }
    auto get_pass_by_id(PassIndexV2 id) -> PassBase<State>& { return *m_passes[id.index].pass; }

    auto add_dependency_edge(PassIndexV2 from, PassIndexV2 to) -> void {
        assert(from.index < m_passes.size() && to.index < m_passes.size() && "invalid pass index");

        m_passes[from.index].children.emplace_back(to);
        m_passes[to.index].indegree++;
    }

    auto execute(vk::raii::CommandBuffer& cmd, State& state, PassResources& resources) -> void {
        bool insert_debug_labels = VulkanContext::get().debug_utils_enabled();

        auto exec_nodes = graph_toposort_to_exec_nodes();
        generate_barriers_for_exec_nodes(exec_nodes, resources);

        for (auto& node : exec_nodes) {
            auto dependency_info = vk::DependencyInfo{}
                .setMemoryBarriers(node.global_memory_barrier)
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
        vk::MemoryBarrier2 global_memory_barrier = {};
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
        for (int i = 0; i < m_passes.size(); i++) {
            indegrees[i] = m_passes[i].indegree;
        }

        // First, find roots to start from.
        std::vector<PassIndexV2> active_passes;
        for (int i = 0; i < indegrees.size(); i++) {
            if (indegrees[i] == 0) {
                active_passes.push_back({ static_cast<uint32_t>(i) });
            }
        }

        while (true) {
            if (active_passes.empty()) {
                for (int i = 0; i < indegrees.size(); i++) {
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

            node.global_memory_barrier.setSrcStageMask(m_last_global_memory_usage.stages);
            node.global_memory_barrier.setSrcAccessMask(mask_access_flags_with_write_bit(m_last_global_memory_usage.accesses));
            node.global_memory_barrier.setDstStageMask(memory_usage.stages);
            node.global_memory_barrier.setDstAccessMask(memory_usage.accesses);

            for (auto& [image_id, old_layout, new_layout] : image_layouts) {
                // An image layout change counts as a write in the Vulkan synchronization model, which implicitly causes a RAW/WAW hazard. This means that only
                // the src access can be cleared.

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

}
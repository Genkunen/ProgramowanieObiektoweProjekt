#pragma once
#include <ranges>
#include <unordered_map>

#include "render_graph_pass.hpp"
#include "render_graph_pass_resources.hpp"
#include "vulkan/vk_prelude.hpp"

#include <print>
#include <xxhash.h>

namespace pop::vulkan::renderer::render_graph {

inline auto access_flags_has_write_aspect(vk::AccessFlags2 access_flags) -> bool {
    vk::AccessFlags2 mask = vk::AccessFlagBits2::eHostWrite | vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eShaderWrite
        | vk::AccessFlagBits2::eTransferWrite | vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eShaderStorageWrite
        | vk::AccessFlagBits2::eDepthStencilAttachmentWrite;

    return (access_flags & mask) != vk::AccessFlags2{};
}

inline auto is_access_flags_pair_hazardous(vk::AccessFlags2 access_flags1, vk::AccessFlags2 access_flags2) -> bool {
    return access_flags_has_write_aspect(access_flags1) || access_flags_has_write_aspect(access_flags2);
}

struct PassIndex {
    uint32_t id = 0;

    static constexpr auto invalid() -> PassIndex { return PassIndex{ 0xFFFFFFFF }; }
    static constexpr auto zero() -> PassIndex { return PassIndex{ 0 }; }

    bool operator==(const PassIndex& other) const { return id == other.id; }
};

struct PassIdHash {
    auto operator()(const PassIndex& id) const -> size_t {
        return XXH3_64bits(&id, sizeof(id));
    }
};

template <typename State> class RenderGraph {
public:
    RenderGraph() = default;
    ~RenderGraph() = default;

    RenderGraph(const RenderGraph& other) = delete;
    RenderGraph(RenderGraph&& other) = default;
    auto operator=(const RenderGraph& other) -> RenderGraph& = delete;
    auto operator=(RenderGraph&& other) -> RenderGraph& = default;

    auto add_pass(std::unique_ptr<PassBase<State>>&& pass) -> PassIndex {
        m_passes.push_back(std::move(pass));
        return { static_cast<uint32_t>(m_passes.size() - 1) };
    }

    auto get_pass_by_id(PassIndex id) -> PassBase<State>& { return *m_passes[id.id]; }

    // TODO: this is a hack to fix last layout tracking (image recreation), clean up later
    auto reset_tracking_for_image(ImageResourceIdentifier id) -> void {
        m_image_resource_states[id].pre_hazard_usage = {};
        m_image_resource_states[id].current_usage    = {};
        m_image_resource_states[id].barrier_before_pass_id = PassIndex::invalid();
    }

    auto execute(vk::raii::CommandBuffer& cmd, State& state, PassResources& resources) -> void {
        clear_resource_states();

        analyze_current_pass_dependencies(resources);

        /*
        std::println("Barrier Debug info:");

        for (uint32_t i = 0; i < m_passes.size(); i++) {
            auto& barrier_params = m_barrier_params[PassIndex { .id = i }];
            std::println("barrier {}: {} buffer barriers, {} image barriers", i, barrier_params.buffer_barriers.size(), barrier_params.image_barriers.size());
            for (auto& barrier : barrier_params.buffer_barriers) {
                std::println("  buffer barrier: srcStageMask: {}, srcAccessMask: {}, dstStageMask: {}, dstAccessMask: {}, buffer: {:016x}",
                    vk::to_string(barrier.srcStageMask), vk::to_string(barrier.srcAccessMask), vk::to_string(barrier.dstStageMask), vk::to_string(barrier.dstAccessMask), (uint64_t)static_cast<VkBuffer>(barrier.buffer));
            }
            for (auto& barrier : barrier_params.image_barriers) {
                std::println("  image barrier: oldLayout: {}, srcStageMask: {}, srcAccessMask: {}, newLayout: {}, dstStageMask: {}, dstAccessMask: {}, image: {:016x}",
                    vk::to_string(barrier.oldLayout),
                    vk::to_string(barrier.srcStageMask),
                    vk::to_string(barrier.srcAccessMask),
                    vk::to_string(barrier.newLayout),
                    vk::to_string(barrier.dstStageMask),
                    vk::to_string(barrier.dstAccessMask),
                    (uint64_t)static_cast<VkImage>(barrier.image));
            }
        }
        */

        for (uint32_t i = 0; i < m_passes.size(); i++) {
            auto& pass = m_passes[i];

            auto pass_index = PassIndex{ .id = i };
            if (m_barrier_params.contains(pass_index)) {
                auto& barrier_params = m_barrier_params[pass_index];
                if (!(barrier_params.buffer_barriers.empty() && barrier_params.image_barriers.empty())) {
                    auto dependency_info = vk::DependencyInfo{}
                        .setBufferMemoryBarriers(barrier_params.buffer_barriers)
                        .setImageMemoryBarriers(barrier_params.image_barriers);

                    cmd.pipelineBarrier2(dependency_info);
                }
            }

            if (pass->is_enabled()) {
                pass->invoke(cmd, state, resources);
            }
        }
    }

private:
    std::vector<std::unique_ptr<PassBase<State>>> m_passes;

    // ---- Helper structs for tracking resource usage across stages -------------------------------------------------------------------------------------------

    // for buffers
    struct BufferUsageState {
        vk::PipelineStageFlags2 stages = vk::PipelineStageFlags2{};
        vk::AccessFlags2 accesses      = vk::AccessFlags2{};
    };

    struct BufferResourceState {
        BufferUsageState pre_hazard_usage = {};
        BufferUsageState current_usage    = {};
        PassIndex barrier_before_pass_id  = PassIndex::invalid();
    };

    // for images
    struct ImageUsageState {
        vk::ImageLayout layout         = vk::ImageLayout::eUndefined;
        vk::PipelineStageFlags2 stages = vk::PipelineStageFlags2{};
        vk::AccessFlags2 accesses      = vk::AccessFlags2{};
    };

    struct ImageResourceState {
        ImageUsageState pre_hazard_usage = {};
        ImageUsageState current_usage    = {};
        PassIndex barrier_before_pass_id = PassIndex::invalid();
    };

    // ---- Dependency tracking helpers ------------------------------------------------------------------------------------------------------------------------

    std::unordered_map<BufferResourceIdentifier, BufferResourceState> m_buffer_resource_states;
    std::unordered_map<ImageResourceIdentifier, ImageResourceState> m_image_resource_states;

    auto is_buffer_dependency_hazardous(const BufferDependency& dep) -> bool {
        return is_access_flags_pair_hazardous(m_buffer_resource_states[dep.resource_id].current_usage.accesses, dep.access);
    }

    auto is_image_dependency_hazardous(const ImageDependency& dep) -> bool {
        bool image_layout_changed = m_image_resource_states[dep.resource_id].current_usage.layout != dep.layout;
        bool access_flags_pair_is_hazardous = is_access_flags_pair_hazardous(m_image_resource_states[dep.resource_id].current_usage.accesses, dep.access);
        return image_layout_changed || access_flags_pair_is_hazardous;
    }

    // ---- Automatic barrier generation -----------------------------------------------------------------------------------------------------------------------

    struct BarrierParams {
        std::vector<vk::BufferMemoryBarrier2> buffer_barriers;
        std::vector<vk::ImageMemoryBarrier2> image_barriers;
    };

    std::unordered_map<PassIndex, BarrierParams, PassIdHash> m_barrier_params;

    auto process_buffer_dependency(const PassResources& resources, const BufferDependency& dep, const PassIndex of_pass_id) -> void {
        // If the dependency is not hazardous in context of previous usage, we can just OR the usage flags on top of the previous ones.
        if (!is_buffer_dependency_hazardous(dep)) {
            m_buffer_resource_states[dep.resource_id].current_usage.accesses |= dep.access;
            m_buffer_resource_states[dep.resource_id].current_usage.stages |= dep.stage;
            return;
        }

        // If they are hazardous, process the flags to insert a barrier and prepare its state for next use.

        // Get last-set pass ID and params to later insert a barrier before it.
        auto insert_barrier_before_pass_id = m_buffer_resource_states[dep.resource_id].barrier_before_pass_id;
        auto barrier_src_stage_flags = m_buffer_resource_states[dep.resource_id].pre_hazard_usage.stages;
        auto barrier_src_access_flags = m_buffer_resource_states[dep.resource_id].pre_hazard_usage.accesses;
        auto barrier_dst_stage_flags = m_buffer_resource_states[dep.resource_id].current_usage.stages;
        auto barrier_dst_access_flags = m_buffer_resource_states[dep.resource_id].current_usage.accesses;

        // Set the pre-hazard usage flags to the current usage flags and set current usage flags to this dependency's flags, so later passes can use it and
        // the next call of this function can properly insert a barrier.
        m_buffer_resource_states[dep.resource_id].pre_hazard_usage = m_buffer_resource_states[dep.resource_id].current_usage;
        m_buffer_resource_states[dep.resource_id].current_usage = { dep.stage, dep.access };
        m_buffer_resource_states[dep.resource_id].barrier_before_pass_id = of_pass_id;

        // Barriers are inserted only after 2 hazards to coalesce many non-hazardous stage/access flags. The last hazard or the only hazard if there is one is
        // added later by a finalizing pass.
        if (insert_barrier_before_pass_id == PassIndex::invalid()) return;

        auto& buffer = resources.get_buffer_by_identifier(dep.resource_id);
        auto buffer_memory_barrier = vk::BufferMemoryBarrier2{}
            .setSrcStageMask(barrier_src_stage_flags)
            .setSrcAccessMask(barrier_src_access_flags)
            .setDstStageMask(barrier_dst_stage_flags)
            .setDstAccessMask(barrier_dst_access_flags)
            .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
            .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
            .setBuffer(buffer.vk_buffer())
            .setOffset(0)
            .setSize(vk::WholeSize);

        m_barrier_params[insert_barrier_before_pass_id].buffer_barriers.emplace_back(buffer_memory_barrier);
    }

    auto process_image_dependency(const PassResources& resources, const ImageDependency& dep, const PassIndex of_pass_id) -> void {
        // If the dependency is not hazardous in context of previous usage, we can just OR the usage flags on top of the previous ones.
        if (!is_image_dependency_hazardous(dep)) {
            m_image_resource_states[dep.resource_id].current_usage.accesses |= dep.access;
            m_image_resource_states[dep.resource_id].current_usage.stages |= dep.stage;
            // Image layout stays the same.
            return;
        }

        // If they are hazardous, process the flags to insert a barrier and prepare its state for next use.

        // Get last-set pass ID and params to later insert a barrier before it.
        auto insert_barrier_before_pass_id = m_image_resource_states[dep.resource_id].barrier_before_pass_id;
        auto barrier_src_stage_flags = m_image_resource_states[dep.resource_id].pre_hazard_usage.stages;
        auto barrier_src_access_flags = m_image_resource_states[dep.resource_id].pre_hazard_usage.accesses;
        auto barrier_src_image_layout = m_image_resource_states[dep.resource_id].pre_hazard_usage.layout;
        auto barrier_dst_stage_flags = m_image_resource_states[dep.resource_id].current_usage.stages;
        auto barrier_dst_access_flags = m_image_resource_states[dep.resource_id].current_usage.accesses;
        auto barrier_dst_image_layout = m_image_resource_states[dep.resource_id].current_usage.layout;

        // Set the pre-hazard usage flags to the current usage flags and set current usage flags to this dependency's flags, so later passes can use it and
        // the next call of this function can properly insert a barrier.
        m_image_resource_states[dep.resource_id].pre_hazard_usage = m_image_resource_states[dep.resource_id].current_usage;
        m_image_resource_states[dep.resource_id].current_usage = { dep.layout, dep.stage, dep.access };
        m_image_resource_states[dep.resource_id].barrier_before_pass_id = of_pass_id;

        // Barriers are inserted only after 2 hazards to coalesce many non-hazardous stage/access flags. The last hazard or the only hazard if there is one is
        // added later by a finalizing pass.
        if (insert_barrier_before_pass_id == PassIndex::invalid()) return;

        auto& image = resources.get_image_by_identifier(dep.resource_id);
        auto image_memory_barrier = vk::ImageMemoryBarrier2{}
            .setOldLayout(barrier_src_image_layout)
            .setSrcStageMask(barrier_src_stage_flags)
            .setSrcAccessMask(barrier_src_access_flags)
            .setNewLayout(barrier_dst_image_layout)
            .setDstStageMask(barrier_dst_stage_flags)
            .setDstAccessMask(barrier_dst_access_flags)
            .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
            .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
            .setImage(image.vk_image())
            .setSubresourceRange(image.full_subresource_range());

        m_barrier_params[insert_barrier_before_pass_id].image_barriers.emplace_back(image_memory_barrier);
    }

    auto finalize_barriers_at_end_of_execution(const PassResources& resources) {
        for (auto& [res_id, barrier_params] : m_buffer_resource_states) {
            if (barrier_params.barrier_before_pass_id == PassIndex::invalid()) continue;

            auto& buffer = resources.get_buffer_by_identifier(res_id);
            auto buffer_memory_barrier = vk::BufferMemoryBarrier2{}
                .setSrcStageMask(barrier_params.pre_hazard_usage.stages)
                .setSrcAccessMask(barrier_params.pre_hazard_usage.accesses)
                .setDstStageMask(barrier_params.current_usage.stages)
                .setDstAccessMask(barrier_params.current_usage.accesses)
                .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
                .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
                .setBuffer(buffer.vk_buffer())
                .setOffset(0)
                .setSize(vk::WholeSize);

            m_barrier_params[barrier_params.barrier_before_pass_id].buffer_barriers.emplace_back(buffer_memory_barrier);
        }

        for (auto& [res_id, barrier_params] : m_image_resource_states) {
            if (barrier_params.barrier_before_pass_id == PassIndex::invalid()) continue;

            auto& image = resources.get_image_by_identifier(res_id);
            auto image_memory_barrier = vk::ImageMemoryBarrier2{}
                .setOldLayout(barrier_params.pre_hazard_usage.layout)
                .setSrcStageMask(barrier_params.pre_hazard_usage.stages)
                .setSrcAccessMask(barrier_params.pre_hazard_usage.accesses)
                .setNewLayout(barrier_params.current_usage.layout)
                .setDstStageMask(barrier_params.current_usage.stages)
                .setDstAccessMask(barrier_params.current_usage.accesses)
                .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
                .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
                .setImage(image.vk_image())
                .setSubresourceRange(image.full_subresource_range());

            m_barrier_params[barrier_params.barrier_before_pass_id].image_barriers.emplace_back(image_memory_barrier);
        }
    }

    // ---- Execution orchestration and management -------------------------------------------------------------------------------------------------------------

    auto clear_resource_states() -> void {
        for (auto& [_, buffer_state] : m_buffer_resource_states) {
            buffer_state.barrier_before_pass_id = PassIndex::invalid();
        }

        for (auto& [_, image_state] : m_image_resource_states) {
            image_state.barrier_before_pass_id = PassIndex::invalid();
        }

        m_barrier_params.clear();
    }

    auto analyze_current_pass_dependencies(const PassResources& resources) -> void {
        for (uint32_t i = 0; i < m_passes.size(); i++) {
            auto& pass = m_passes[i];
            if (!pass->is_enabled()) continue;

            auto pass_index = PassIndex{ .id = i };
            auto& pass_dependencies = pass->dependencies();
            for (auto& dep : pass_dependencies.buffer_dependencies()) {
                process_buffer_dependency(resources, dep, pass_index);
            }
            for (auto& dep : pass_dependencies.image_dependencies()) {
                process_image_dependency(resources, dep, pass_index);
            }
        }

        finalize_barriers_at_end_of_execution(resources);
    }
};

}
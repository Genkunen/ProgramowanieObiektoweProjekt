#pragma once
#include <ranges>
#include <unordered_map>

#include "render_graph_pass.hpp"
#include "render_graph_pass_resources.hpp"
#include "vulkan/vk_prelude.hpp"

namespace pop::vulkan::renderer::render_graph {

inline auto access_flags_has_read_aspect(vk::AccessFlags2 access_flags) -> bool {
    vk::AccessFlags2 mask = vk::AccessFlagBits2::eIndexRead | vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eTransferRead
    | vk::AccessFlagBits2::eHostRead | vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eUniformRead | vk::AccessFlagBits2::eColorAttachmentRead
    | vk::AccessFlagBits2::eIndirectCommandRead | vk::AccessFlagBits2::eShaderSampledRead | vk::AccessFlagBits2::eShaderStorageRead
    | vk::AccessFlagBits2::eInputAttachmentRead | vk::AccessFlagBits2::eVertexAttributeRead | vk::AccessFlagBits2::eDepthStencilAttachmentRead;

    return (access_flags & mask) != vk::AccessFlags2{};
}

inline auto access_flags_has_write_aspect(vk::AccessFlags2 access_flags) -> bool {
    vk::AccessFlags2 mask = vk::AccessFlagBits2::eHostWrite | vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eShaderWrite
        | vk::AccessFlagBits2::eTransferWrite | vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eShaderStorageWrite
        | vk::AccessFlagBits2::eDepthStencilAttachmentWrite;

    return (access_flags & mask) != vk::AccessFlags2{};
}

inline auto is_access_flags_pair_hazardous(vk::AccessFlags2 access_flags1, vk::AccessFlags2 access_flags2) -> bool {
    return access_flags_has_write_aspect(access_flags1) || access_flags_has_write_aspect(access_flags2);
}



struct PassId {
    uint32_t id;
};

template <typename State> class RenderGraph {
public:
    RenderGraph() = default;
    ~RenderGraph() = default;

    auto add_pass(std::unique_ptr<PassBase<State>> pass) -> PassId {
        m_passes.push_back(std::move(pass));
        return { static_cast<uint32_t>(m_passes.size() - 1) };
    }

    auto get_pass_by_id(PassId id) -> PassBase<State>& { return *m_passes[id.id]; }

    auto execute(vk::raii::CommandBuffer& cmd, State& state, PassResources& resources) -> void {
        assert(false && "todo");
    }

private:
    std::vector<std::unique_ptr<PassBase<State>>> m_passes;

    struct BufferResourceState {
        vk::PipelineStageFlags2 used_stages;
        vk::AccessFlags2 used_accesses;
    };

    struct ImageResourceState {
        vk::ImageLayout used_layout = vk::ImageLayout::eUndefined;
        vk::PipelineStageFlags2 used_stages;
        vk::AccessFlags2 used_accesses;
    };
    std::unordered_map<BufferResourceIdentifier, BufferResourceState> m_buffer_resource_states;
    std::unordered_map<ImageResourceIdentifier, ImageResourceState> m_image_resource_states;

    auto is_buffer_dependency_hazardous(const BufferDependency& dep) -> bool {
        return is_access_flags_pair_hazardous(m_buffer_resource_states[dep.resource_id].used_accesses, dep.access);
    }

    auto is_image_dependency_hazardous(const ImageDependency& dep) -> bool {
        bool image_layout_changed = m_image_resource_states[dep.resource_id].used_layout != dep.layout;
        bool access_flags_pair_is_hazardous = is_access_flags_pair_hazardous(m_image_resource_states[dep.resource_id].used_accesses, dep.access);
        return image_layout_changed || access_flags_pair_is_hazardous;
    }

};

}
#include "execution_plan.hpp"

namespace pop::vulkan::renderer::render_graph {

void PipelineBarrierNode::execute(vk::raii::CommandBuffer& cmd, const SimulationRenderState& state, const PassResources& resources) {
    std::vector<vk::BufferMemoryBarrier2> buffer_barriers;
    std::vector<vk::ImageMemoryBarrier2> image_barriers;

    for (auto& [buffer_id, barrier] : m_buffer_barriers) {
        auto& res = resources.get_buffer_by_identifier(buffer_id);
        barrier.buffer = res.vk_buffer();
        buffer_barriers.push_back(barrier);
    }

    for (auto& [image_id, barrier] : m_image_barriers) {
        auto& res = resources.get_image_by_identifier(image_id);
        barrier.image = res.vk_image();
        image_barriers.push_back(barrier);
    }

    auto dep_info = vk::DependencyInfo()
        .setBufferMemoryBarriers(buffer_barriers)
        .setImageMemoryBarriers(image_barriers);

    cmd.pipelineBarrier2(dep_info);
}

void PassNode::execute(vk::raii::CommandBuffer& cmd, const SimulationRenderState& state, const PassResources& resources) {
    m_pass->invoke(cmd, state, resources);
}

} // namespace pop::vulkan::renderer::render_graph
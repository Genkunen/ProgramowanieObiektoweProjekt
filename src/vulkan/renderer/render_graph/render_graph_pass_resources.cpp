#include "render_graph_pass_resources.hpp"

namespace pop::vulkan::renderer::render_graph {

auto PassResources::inject_buffer(BufferResourceIdentifier identifier, VulkanBuffer& buffer) -> void {
    m_buffers.emplace(identifier, std::ref(buffer));
}

auto PassResources::inject_image(ImageResourceIdentifier identifier, VulkanImage& image) -> void {
    m_images.emplace(identifier, std::ref(image));
}

auto PassResources::get_buffer_by_identifier(BufferResourceIdentifier identifier) const -> const VulkanBuffer& {
    return m_buffers.at(identifier);
}

auto PassResources::get_image_by_identifier(ImageResourceIdentifier identifier) const -> const VulkanImage& {
    return m_images.at(identifier);
}

} // namespace pop::vulkan::renderer::render_graph
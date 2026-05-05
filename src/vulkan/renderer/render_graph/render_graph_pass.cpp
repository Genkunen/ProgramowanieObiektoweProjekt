#include "render_graph_pass.hpp"

namespace pop::vulkan::renderer::render_graph {

PassDependencies::PassDependencies(std::vector<BufferDependency>&& buffer_dependencies, std::vector<ImageDependency>&& image_dependencies)
  : m_buffer_dependencies(std::move(buffer_dependencies)), m_image_dependencies(std::move(image_dependencies)) {}

}
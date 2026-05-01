#pragma once
#include <cstdint>
#include <glm/glm.hpp>

namespace pop::vulkan::renderer {

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texcoord;
};

struct Mesh {
    std::uint32_t allocation_index;
};

}
#pragma once
#include <cstdint>
#define GLM_FORCE_LEFT_HANDED
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
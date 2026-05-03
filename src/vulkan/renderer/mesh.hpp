#pragma once

#include <cstdint>
#include <string>

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

auto load_mesh_data_gltf(std::string filename) -> std::tuple<std::vector<Vertex>, std::vector<uint32_t>>;

}

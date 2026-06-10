#pragma once

#include <vector>
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

/// @brief Loads mesh data from a .glb or .gltf file.
/// @param filename The path to the file to load.
/// @return A tuple containing the loaded mesh vertices and indices.
auto load_mesh_data_gltf(std::string filename) -> std::tuple<std::vector<Vertex>, std::vector<uint32_t>>;

} // namespace pop::vulkan::renderer
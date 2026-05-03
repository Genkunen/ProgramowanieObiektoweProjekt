#include "mesh.hpp"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>
#include <print>
#include <utility>

namespace {

auto read_floats_accessor(const tinygltf::Model& model, const tinygltf::Accessor& acc) {
    const auto& view = model.bufferViews[acc.bufferView];
    const auto& buffer = model.buffers[view.buffer];
    const uint8_t* data = buffer.data.data() + view.byteOffset + acc.byteOffset;

    std::vector<float> retval(acc.count * tinygltf::GetNumComponentsInType(acc.type));
    std::memcpy(retval.data(), data, retval.size() * sizeof(float));
    return retval;
}

template <typename At>
auto read_floats_at(const tinygltf::Model& model, At&& at) {
    const auto& acc = model.accessors.at(std::forward<At>(at));
    return read_floats_accessor(model, acc);
}

template <typename At>
auto read_indices_at(const tinygltf::Model& model, At&& at) {
    const auto& acc = model.accessors.at(std::forward<At>(at));

    const auto& view = model.bufferViews[acc.bufferView];
    const auto& buffer = model.buffers[view.buffer];
    const uint8_t* data = buffer.data.data() + view.byteOffset + acc.byteOffset;

    std::vector<uint32_t> retval(acc.count);
    if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
        const uint16_t* src = reinterpret_cast<const uint16_t*>(data);
        for (size_t i = 0; i < acc.count; ++i) retval[i] = src[i];
    } else if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
        const uint32_t* src = reinterpret_cast<const uint32_t*>(data);
        for (size_t i = 0; i < acc.count; ++i) retval[i] = src[i];
    } else if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
        const uint8_t* src = reinterpret_cast<const uint8_t*>(data);
        for (size_t i = 0; i < acc.count; ++i) retval[i] = src[i];
    } else {
        throw std::runtime_error("Unsupported index component type");
    }
    return retval;
}

}

namespace pop::vulkan::renderer {

auto load_mesh_data_gltf(std::string filename) -> std::tuple<std::vector<Vertex>, std::vector<uint32_t>> {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    auto success = loader.LoadBinaryFromFile(&model, &err, &warn, filename);
    if (!success) {
        success = loader.LoadASCIIFromFile(&model, &err, &warn, filename);
    }
    if (!warn.empty()) {
        std::println(stderr, "[tinyGLTF] {}", warn);
    }
    if (!success || model.meshes.empty()) {
        throw std::runtime_error("Failed to load gltf mesh data: " + err);
    }

    const auto& primitives = model.meshes[0].primitives[0];
    const auto& positionAcc = model.accessors.at(primitives.attributes.at("POSITION"));

    const auto& positions = read_floats_accessor(model, positionAcc);
    const auto& normals = read_floats_at(model, primitives.attributes.at("NORMAL"));
    const auto& uvs = read_floats_at(model, primitives.attributes.at("TEXCOORD_0"));
    const auto& indices = read_indices_at(model, primitives.indices);

    std::vector<pop::vulkan::renderer::Vertex> vertices(positionAcc.count);

    for (size_t i = 0; i < vertices.size(); ++i) {
        vertices[i].position = { positions[i * 3 + 0], positions[i * 3 + 1], positions[i * 3 + 2] };
        vertices[i].normal   = { normals[i * 3 + 0],   normals[i * 3 + 1],   normals[i * 3 + 2] };
        vertices[i].texcoord = { uvs[i * 2 + 0], uvs[i * 2 + 1] };
    }

    return { std::move(vertices), std::move(indices) };
}

}

#pragma once
#include "vulkan/renderer/mesh.hpp"

#include <vector>

// TODO: This is temporary, should replace with mesh loading logic later

auto make_sphere_mesh_data(uint32_t lat_segments, uint32_t lon_segments, float radius) -> std::tuple<std::vector<pop::vulkan::renderer::Vertex>, std::vector<uint32_t>> {
    std::vector<pop::vulkan::renderer::Vertex> vertices;
    std::vector<uint32_t> indices;

    for (uint32_t lat = 0; lat <= lat_segments; lat++) {
        float theta = lat / static_cast<float>(lat_segments) * std::numbers::pi_v<float>;
        float sin_theta = std::sin(theta);
        float cos_theta = std::cos(theta);

        for (uint32_t lon = 0; lon <= lon_segments; lon++) {
            float phi = lon / static_cast<float>(lon_segments) * std::numbers::pi_v<float>;
            float sin_phi = std::sin(phi);
            float cos_phi = std::cos(phi);

            float x = cos_phi * sin_theta;
            float y = cos_theta;
            float z = sin_phi * sin_theta;

            vertices.emplace_back(glm::vec3{ x, y, z } * radius, glm::vec3{ x, y, z }, glm::vec2{ lon / lon_segments, lat / lat_segments });
        }
    }

    for (uint32_t lat = 0; lat < lat_segments; lat++) {
        for (uint32_t lon = 0; lon < lon_segments; lon++) {
            uint32_t first = (lat * (lon_segments + 1)) + lon;
            uint32_t second = first + lon_segments + 1;

            indices.emplace_back(first);
            indices.emplace_back(second);
            indices.emplace_back(first + 1);
            indices.emplace_back(second);
            indices.emplace_back(second + 1);
            indices.emplace_back(first + 1);
        }
    }

    return { std::move(vertices), std::move(indices) };
}
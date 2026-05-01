#pragma once
#include "mesh.hpp"
#include "shaders/shared_types.hpp"
#include "vulkan/vk_buffer.hpp"

namespace pop::vulkan::renderer {

class MeshPool {
public:
    MeshPool(VulkanBuffer&& vertex_buffer, VulkanBuffer&& index_buffer, vma::raii::VirtualBlock&& vertex_buffer_block, vma::raii::VirtualBlock&& index_buffer_block);

    static auto create(uint32_t max_vertices, uint32_t max_indices) -> MeshPool;

    [[nodiscard]] auto vertex_buffer() const -> const VulkanBuffer& { return m_vertex_buffer; }
    [[nodiscard]] auto index_buffer() const -> const VulkanBuffer& { return m_index_buffer; }
    [[nodiscard]] auto mesh_allocations() const -> const std::vector<shaders::MeshAllocationData>& { return m_mesh_allocations; }
    [[nodiscard]] auto mesh_allocations_table_generation() const -> uint64_t { return m_mesh_allocations_table_generation; }


    [[nodiscard]] auto allocate(uint32_t vertex_count, uint32_t index_count) -> Mesh;
    auto upload_mesh_data(Mesh mesh, const std::span<const Vertex>& vertices, const std::span<const uint32_t>& indices) -> void;

private:
    VulkanBuffer m_vertex_buffer;
    VulkanBuffer m_index_buffer;

    vma::raii::VirtualBlock m_vertex_buffer_block;
    vma::raii::VirtualBlock m_index_buffer_block;

    struct MeshAllocationHandle {
        vma::raii::VirtualAllocation vertex_allocation;
        vma::raii::VirtualAllocation index_allocation;
    };

    std::vector<shaders::MeshAllocationData> m_mesh_allocations;
    uint64_t m_mesh_allocations_table_generation = 0;

    std::vector<MeshAllocationHandle> m_mesh_allocation_handles;
};

}
#pragma once
#include "mesh.hpp"
#include "shaders/shared_types.hpp"
#include "vulkan/vk_buffer.hpp"

#include <filesystem>

namespace pop::vulkan::renderer {

/// @class MeshPool
/// @brief Manages a pool of meshes that can be allocated and uploaded to the GPU.
class MeshPool {
public:
    MeshPool(VulkanBuffer&& vertex_buffer, VulkanBuffer&& index_buffer, vma::raii::VirtualBlock&& vertex_buffer_block, vma::raii::VirtualBlock&& index_buffer_block);

    /// @brief Creates a new MeshPool instance.
    /// @param max_vertices The maximum number of vertices that can be allocated in the vertex buffer.
    /// @param max_indices The maximum number of indices that can be allocated in the index buffer.
    /// @returns A new MeshPool instance.
    static auto create(uint32_t max_vertices, uint32_t max_indices) -> MeshPool;

    /// @brief Returns the underlying buffer holding the meshes' vertices.
    [[nodiscard]] auto vertex_buffer()                     const -> const VulkanBuffer& { return m_vertex_buffer; }
    /// @brief Returns the underlying buffer holding the meshes' indices.
    [[nodiscard]] auto index_buffer()                      const -> const VulkanBuffer& { return m_index_buffer; }
    /// @brief Returns the list of mesh allocations in the pool.
    [[nodiscard]] auto mesh_allocations()                  const -> const std::vector<shaders::MeshAllocationData>& { return m_mesh_allocations; }
    /// @brief Returns the generation number of the mesh allocations table.
    [[nodiscard]] auto mesh_allocations_table_generation() const -> uint64_t { return m_mesh_allocations_table_generation; }

    /// @brief Loads a mesh from a file.
    /// @param filename The path to the mesh file.
    /// @returns A Mesh handle representing the loaded mesh.
    [[nodiscard]] auto load_mesh(std::filesystem::path filename) -> Mesh;

    /// @brief Allocates a new mesh from the pool.
    /// @param vertex_count The number of vertices in the mesh.
    /// @param index_count The number of indices in the mesh.
    /// @returns A Mesh handle representing the allocated mesh.
    [[nodiscard]] auto allocate(uint32_t vertex_count, uint32_t index_count) -> Mesh;

    /// @brief Helper method for uploading mesh data to the GPU.
    /// @param mesh The mesh to upload data to.
    /// @param vertices The vertices of the mesh.
    /// @param indices The indices of the mesh.
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

} // namespace pop::vulkan::renderer
#include "mesh_pool.hpp"

#include "mesh.hpp"

namespace pop::vulkan::renderer {

MeshPool::MeshPool(VulkanBuffer&& vertex_buffer, VulkanBuffer&& index_buffer, vma::raii::VirtualBlock&& vertex_buffer_block,
                   vma::raii::VirtualBlock&& index_buffer_block)
                       : m_vertex_buffer(std::move(vertex_buffer)), m_index_buffer(std::move(index_buffer)), m_vertex_buffer_block(std::move(vertex_buffer_block)),
                         m_index_buffer_block(std::move(index_buffer_block)) {}

auto MeshPool::create(uint32_t max_vertices, uint32_t max_indices) -> MeshPool {
    auto vertex_buffer = VulkanBuffer::builder()
        .set_size(static_cast<uint64_t>(max_vertices) * sizeof(Vertex))
        .set_usage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .set_memory_usage(vma::MemoryUsage::eAutoPreferDevice)
        .map_for_sequential_write()
        .build();

    auto index_buffer = VulkanBuffer::builder()
        .set_size(static_cast<uint64_t>(max_indices) * sizeof(uint32_t))
        .set_usage(vk::BufferUsageFlagBits::eIndexBuffer)
        .set_memory_usage(vma::MemoryUsage::eAutoPreferDevice)
        .map_for_sequential_write()
        .build();

    // Since we won't be freeing meshes anyway for the lifecycle of them in the simulation, we can use the linear allocation algorithm.
    auto vertex_buffer_block_create_info = vma::VirtualBlockCreateInfo()
        .setFlags(vma::VirtualBlockCreateFlagBits::eLinearAlgorithm)
        .setSize(vertex_buffer.size());

    auto index_buffer_block_create_info = vma::VirtualBlockCreateInfo()
        .setFlags(vma::VirtualBlockCreateFlagBits::eLinearAlgorithm)
        .setSize(index_buffer.size());

    auto vertex_buffer_block = vma::raii::createVirtualBlock(vertex_buffer_block_create_info);
    auto index_buffer_block = vma::raii::createVirtualBlock(index_buffer_block_create_info);

    return MeshPool(std::move(vertex_buffer), std::move(index_buffer), std::move(vertex_buffer_block), std::move(index_buffer_block));
}

auto MeshPool::allocate(uint32_t vertex_count, uint32_t index_count) -> Mesh {
    uint64_t vertex_area_size = static_cast<uint64_t>(vertex_count) * sizeof(Vertex);
    uint64_t index_area_size = static_cast<uint64_t>(index_count) * sizeof(uint32_t);

    auto vertex_buffer_alloc = m_vertex_buffer_block.allocate(vertex_area_size);
    auto index_buffer_alloc = m_index_buffer_block.allocate(index_area_size);

    m_mesh_allocations.emplace_back(
        vertex_count, index_count,
        vertex_buffer_alloc.getInfo().offset / sizeof(Vertex), index_buffer_alloc.getInfo().offset / sizeof(uint32_t)
    );

    m_mesh_allocation_handles.emplace_back(std::move(vertex_buffer_alloc), std::move(index_buffer_alloc));

    uint32_t allocation_index = static_cast<uint32_t>(m_mesh_allocations.size()) - 1;

    m_mesh_allocations_needs_reupload = true;

    return Mesh{ allocation_index };
}

auto MeshPool::upload_mesh_data(Mesh mesh, const std::span<const Vertex>& vertices, const std::span<const uint32_t>& indices) -> void {
    Vertex* allocation_base_vertex_buffer_ptr = reinterpret_cast<Vertex*>(m_vertex_buffer.memory_host_ptr()) + m_mesh_allocations[mesh.allocation_index].first_vertex;
    uint32_t* allocation_base_index_buffer_ptr = reinterpret_cast<uint32_t*>(m_index_buffer.memory_host_ptr()) + m_mesh_allocations[mesh.allocation_index].first_index;

    uint32_t vertex_count = m_mesh_allocations[mesh.allocation_index].vertex_count;
    uint32_t index_count = m_mesh_allocations[mesh.allocation_index].index_count;

    assert(vertex_count == vertices.size());
    assert(index_count == indices.size());

    std::ranges::copy(vertices, allocation_base_vertex_buffer_ptr);
    std::ranges::copy(indices, allocation_base_index_buffer_ptr);
}

} // namespace pop::vulkan::renderer
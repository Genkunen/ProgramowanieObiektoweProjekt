#pragma once
#include "vk_prelude.hpp"

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace pop::vulkan {

/// @brief A helper class to create a Vulkan specialization constants map.
/// @tparam T The type of the data to source data from which pipeline specialization constants are sourced.
template <typename T> class VulkanSpecializationConstantsMap {
public:
    explicit VulkanSpecializationConstantsMap(T data) : m_data(data) {}
    explicit VulkanSpecializationConstantsMap(T&& data) : m_data(std::forward<T>(data)) {}

    constexpr auto data() const noexcept -> const T& { return m_data; }
    constexpr auto data()       noexcept ->       T& { return m_data; }

    constexpr auto map_entries() const noexcept -> const std::vector<vk::SpecializationMapEntry>& { return m_map_entries; }

    /// @brief Adds a map entry for the given member of the data that will be exposed to the shader as a specialization constant with ID @c constant_id
    /// @tparam MemberType The type of the member to map.
    /// @param constant_id The ID of the specialization constant exposed to the shader.
    /// @param member The pointer to the member of the data to map as the specialization constant.
    template <typename MemberType>
    auto add_map_entry(uint32_t constant_id, MemberType T::* member) {
        uint32_t size = sizeof(MemberType);

        auto data_base = reinterpret_cast<uint8_t*>(&m_data);
        auto data_member_location = reinterpret_cast<uint8_t*>(&(m_data.*member));
        uint32_t offset = static_cast<uint32_t>(data_member_location - data_base);

        auto spec_map_entry = vk::SpecializationMapEntry()
            .setConstantID(constant_id)
            .setSize(size)
            .setOffset(offset);

        m_map_entries.emplace_back(spec_map_entry);
    }

private:
    T m_data;

    std::vector<vk::SpecializationMapEntry> m_map_entries;
};

} // namespace pop::vulkan
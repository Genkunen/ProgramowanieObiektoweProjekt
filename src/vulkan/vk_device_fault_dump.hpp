#pragma once
#include "vk_prelude.hpp"

namespace pop::vulkan {

/// @class VulkanDeviceFaultDump
/// @brief Information dump after a Vulkan device fault.
class VulkanDeviceFaultDump {
public:
    VulkanDeviceFaultDump(std::string&& fault_description, std::vector<vk::DeviceFaultAddressInfoEXT>&& device_fault_addresses, std::vector<vk::DeviceFaultVendorInfoEXT>&& device_fault_vendor_infos);

    /// @brief Checks if device fault dumping is supported.
    static auto is_dumping_supported()   -> bool;
    /// @brief Dumps device fault information.
    /// @note This function should only be called if is_dumping_supported() returns true.
    static auto dump_device_fault_info() -> VulkanDeviceFaultDump;

    /// @brief Formats the device fault information as a string that can be logged or displayed to the user.
    auto format_as_fault_message() const -> std::string;

private:
    std::string m_fault_description;
    std::vector<vk::DeviceFaultAddressInfoEXT> m_device_fault_addresses;
    std::vector<vk::DeviceFaultVendorInfoEXT> m_device_fault_vendor_infos;
};

} // namespace pop::vulkan
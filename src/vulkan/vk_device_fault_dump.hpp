#pragma once
#include "vk_prelude.hpp"

namespace pop::vulkan {

class VulkanDeviceFaultDump {
public:
    VulkanDeviceFaultDump(std::string&& fault_description, std::vector<vk::DeviceFaultAddressInfoEXT>&& device_fault_addresses, std::vector<vk::DeviceFaultVendorInfoEXT>&& device_fault_vendor_infos);

    static auto is_dumping_supported() -> bool;
    static auto dump_device_fault_info() -> VulkanDeviceFaultDump;

    auto format_as_fault_message() const -> std::string;

private:
    std::string m_fault_description;
    std::vector<vk::DeviceFaultAddressInfoEXT> m_device_fault_addresses;
    std::vector<vk::DeviceFaultVendorInfoEXT> m_device_fault_vendor_infos;
};

} // namespace pop::vulkan
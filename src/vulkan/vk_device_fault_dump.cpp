#include "vk_device_fault_dump.hpp"

#include "vk_context.hpp"

namespace pop::vulkan {

VulkanDeviceFaultDump::VulkanDeviceFaultDump(std::string&& fault_description, std::vector<vk::DeviceFaultAddressInfoEXT>&& device_fault_addresses,
    std::vector<vk::DeviceFaultVendorInfoEXT>&& device_fault_vendor_infos)
        : m_fault_description(std::move(fault_description)), m_device_fault_addresses(std::move(device_fault_addresses)),
            m_device_fault_vendor_infos(std::move(device_fault_vendor_infos)) {}

auto VulkanDeviceFaultDump::is_dumping_supported() -> bool {
    return VulkanContext::get().ext_device_fault_enabled();
}

auto VulkanDeviceFaultDump::dump_device_fault_info() -> VulkanDeviceFaultDump {
    vk::DeviceFaultCountsEXT device_fault_counts{};
    VulkanContext::get().vk_device().getFaultInfoEXT(&device_fault_counts, nullptr);

    std::vector<vk::DeviceFaultAddressInfoEXT> device_fault_addresses(device_fault_counts.addressInfoCount);
    std::vector<vk::DeviceFaultVendorInfoEXT> device_fault_vendor_infos(device_fault_counts.vendorInfoCount);

    vk::DeviceFaultInfoEXT device_fault_info{};
    device_fault_info.pAddressInfos = device_fault_addresses.data();
    device_fault_info.pVendorInfos = device_fault_vendor_infos.data();

    VulkanContext::get().vk_device().getFaultInfoEXT(&device_fault_counts, &device_fault_info);

    std::string fault_description = std::string(device_fault_info.description);
    return VulkanDeviceFaultDump(std::move(fault_description), std::move(device_fault_addresses), std::move(device_fault_vendor_infos));
}

auto VulkanDeviceFaultDump::format_as_fault_message() const -> std::string {
    std::string message = "Fault Description: " + m_fault_description + "\n\n";

    if (m_device_fault_addresses.size() > 0) {
        message += "Fault Addresses:\n";
        for (auto& fault_addr : m_device_fault_addresses) {
            message += std::format("  - {} at 0x{:016x}\n", vk::to_string(fault_addr.addressType), fault_addr.reportedAddress);
        }
        message += "\n";
    }
    if (m_device_fault_vendor_infos.size() > 0) {
        message += "Vendor Fault Information:\n";
        for (auto& vendor_info : m_device_fault_vendor_infos) {
            message += std::format("  - (Code 0x{:016x}, Data 0x{:016x}) {}\n", vendor_info.vendorFaultCode, vendor_info.vendorFaultData, vendor_info.description);
        }
    }

    return message;
}

} // namespace pop::vulkan
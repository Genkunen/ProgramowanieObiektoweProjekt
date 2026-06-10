#pragma once
#include <cstdint>
#include <filesystem>
#include <vector>
#include "vk_prelude.hpp"

namespace pop::vulkan {

/// @class SpirvCode
/// @brief A class to represent SPIR-V code.
class SpirvCode {
public:
    SpirvCode(std::vector<uint32_t>&& code);

    /// @brief Loads SPIR-V code from a file.
    /// @param file_path The path to the SPIR-V file.
    /// @return A SpirvCode object containing the SPIR-V code.
    static auto load_from_file(const std::filesystem::path& file_path) -> SpirvCode;

    /// @brief Returns the SPIR-V code as a vector of DWORDs.
    [[nodiscard]] auto code() const                             -> const std::vector<uint32_t>& { return m_code; }
    /// @brief Returns a Vulkan @c vk::ShaderModuleCreateInfo structure with the SPIR-V code.
    [[nodiscard]] auto vulkan_shader_module_create_info() const -> vk::ShaderModuleCreateInfo { return vk::ShaderModuleCreateInfo().setCode(m_code); }

private:
    std::vector<uint32_t> m_code;
};

} // namespace pop::vulkan
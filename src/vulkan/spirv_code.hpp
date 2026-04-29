#pragma once
#include <cstdint>
#include <filesystem>
#include <vector>
#include "vk_prelude.hpp"

namespace pop::vulkan {

class SpirvCode {
public:
    SpirvCode(std::vector<uint32_t>&& code);

    static auto load_from_file(const std::filesystem::path& file_path) -> SpirvCode;

    [[nodiscard]] auto code() const -> const std::vector<uint32_t>& { return m_code; }
    [[nodiscard]] auto vulkan_shader_module_create_info() const -> vk::ShaderModuleCreateInfo { return vk::ShaderModuleCreateInfo().setCode(m_code); }

private:
    std::vector<uint32_t> m_code;
};

}
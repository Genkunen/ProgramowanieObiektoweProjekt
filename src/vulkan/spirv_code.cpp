#include "spirv_code.hpp"

#include <fstream>

namespace pop::vulkan {

SpirvCode::SpirvCode(std::vector<uint32_t>&& code)
    : m_code(std::move(code)) {}

auto SpirvCode::load_from_file(const std::filesystem::path& file_path) -> SpirvCode {
    std::ifstream shader_code_file(file_path, std::ios::binary | std::ios::ate);

    if (!shader_code_file) {
        throw std::runtime_error("Failed to open shader code file");
    }

    auto file_size = shader_code_file.tellg();

    if ((file_size & 0b11) != 0) {
        throw std::runtime_error("Shader code file size is not aligned to the SPIR-V word size of 4 bytes");
    }

    auto shader_code_dword_count = file_size / sizeof(uint32_t);
    std::vector<uint32_t> shader_code(shader_code_dword_count);
    shader_code_file.seekg(0);
    shader_code_file.read(reinterpret_cast<char*>(shader_code.data()), file_size);

    if (!shader_code_file) {
        throw std::runtime_error("Failed to read shader code file");
    }
    return SpirvCode(std::move(shader_code));
}

} // namespace pop::vulkan
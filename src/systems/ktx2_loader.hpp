#pragma once
#include "systems.hpp"
#include "vulkan/vk_image.hpp"

namespace pop::systems {

/// @class Ktx2Loader
/// @brief Loads KTX2 images into Vulkan images.
class Ktx2Loader {
public:
    Ktx2Loader(vk::raii::CommandPool&& upload_cmd_pool);

    static auto create() -> Ktx2Loader;

    /// @brief Loads a KTX2 image from the specified file path and converts it to a Vulkan image.
    /// @param path The path to the KTX2 image file.
    /// @returns A VulkanImage object representing the loaded KTX2 image.
    auto load_to_vulkan_image(const std::filesystem::path& path) -> vulkan::VulkanImage;

private:
    vk::raii::CommandPool m_upload_cmd_pool;
};

} // namespace pop::systems
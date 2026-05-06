#pragma once
#include "systems.hpp"
#include "vulkan/vk_image.hpp"

namespace pop::systems {

class Ktx2Loader {
public:
    Ktx2Loader(vk::raii::CommandPool&& upload_cmd_pool);

    static auto create() -> Ktx2Loader;

    auto load_to_vulkan_image(const std::filesystem::path& path) -> vulkan::VulkanImage;

private:
    vk::raii::CommandPool m_upload_cmd_pool;
};

}
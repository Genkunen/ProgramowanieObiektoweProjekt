#include "ktx2_loader.hpp"

#include "vulkan/vk_buffer.hpp"
#include "vulkan/vk_pipeline_barriers.hpp"

#include <ktx.h>
#include <ktxvulkan.h>
#include <print>

namespace pop::systems {

Ktx2Loader::Ktx2Loader(vk::raii::CommandPool&& upload_cmd_pool)
    : m_upload_cmd_pool(std::move(upload_cmd_pool)) {}

auto Ktx2Loader::create() -> Ktx2Loader {
    auto command_pool_create_info = vk::CommandPoolCreateInfo()
        .setQueueFamilyIndex(vulkan::VulkanContext::get().vk_graphics_queue_family())
        .setFlags(vk::CommandPoolCreateFlagBits::eTransient);

    return Ktx2Loader(vulkan::VulkanContext::get().vk_device().createCommandPool(command_pool_create_info));
}

auto Ktx2Loader::load_to_vulkan_image(const std::filesystem::path& path) -> vulkan::VulkanImage {
    ktxTexture2* ktx_texture;
    ktx_error_code_e result = ktxTexture2_CreateFromNamedFile(path.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktx_texture);

    if (result != KTX_SUCCESS) {
        std::println("ERROR: Failed to load texture {}: {}", path.string(), ktxErrorString(result));
        throw std::runtime_error("Failed to load ktx2 texture");
    }

    if (ktxTexture2_NeedsTranscoding(ktx_texture)) {
        // TODO: check device support for BC7/ASTC and use those / R8G8B8A8 instead depending on availability
        ktx_transcode_fmt_e target_format = KTX_TTF_BC7_RGBA;

        ktx_error_code_e transcode_result = ktxTexture2_TranscodeBasis(ktx_texture, target_format, 0);
        if (transcode_result != KTX_SUCCESS) {
            std::println("ERROR: Failed to transcode texture {}: {}", path.string(), ktxErrorString(transcode_result));
            throw std::runtime_error("Failed to transcode ktx2 texture");
        }
    }

    uint32_t texture_width  = ktx_texture->baseWidth;
    uint32_t texture_height = ktx_texture->baseHeight;
    uint32_t texture_depth  = ktx_texture->baseDepth;
    uint32_t mip_levels     = ktx_texture->numLevels;
    uint32_t array_layers   = ktx_texture->numLayers;
    size_t   byte_size      = ktx_texture->dataSize;
    uint8_t* data           = ktx_texture->pData;
    auto     format         = static_cast<vk::Format>(ktxTexture2_GetVkFormat(ktx_texture));
    auto     usage_flags    = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
    auto     tiling         = vk::ImageTiling::eOptimal;

    assert(texture_depth == 1 && array_layers == 1 && "only 2D textures are supported at the moment");

    auto staging_buffer = vulkan::VulkanBuffer::builder()
        .set_size(byte_size)
        .set_usage(vk::BufferUsageFlagBits::eTransferSrc)
        .set_memory_usage(vma::MemoryUsage::eAutoPreferDevice)
        .map_for_sequential_write()
        .build();

    auto image = vulkan::VulkanImage::builder()
        .set_type(vk::ImageType::e2D)
        .set_extent({texture_width, texture_height, texture_depth})
        .set_mip_levels(mip_levels)
        .set_format(format)
        .set_usage(usage_flags)
        .set_tiling(tiling)
        .set_initial_layout(vk::ImageLayout::eUndefined)
        .set_memory_usage(vma::MemoryUsage::eAutoPreferDevice)
        .build();

    memcpy(staging_buffer.memory_host_ptr(), data, byte_size);

    auto cmd_alloc_info = vk::CommandBufferAllocateInfo()
        .setCommandPool(m_upload_cmd_pool)
        .setLevel(vk::CommandBufferLevel::ePrimary)
        .setCommandBufferCount(1);

    auto cmd = std::move(vulkan::VulkanContext::get().vk_device().allocateCommandBuffers(cmd_alloc_info)[0]);

    auto cmd_begin_info = vk::CommandBufferBeginInfo()
        .setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    cmd.begin(cmd_begin_info);

    vulkan::VulkanPipelineBarriers::builder()
        .insert_image_memory_barrier(image.vk_image(),
            vk::ImageLayout::eUndefined, vk::PipelineStageFlagBits2::eNone, vk::AccessFlagBits2::eNone,
            vk::ImageLayout::eTransferDstOptimal, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
            image.full_subresource_range()
        )
        .flush(cmd);

    auto subresource_layers = vk::ImageSubresourceLayers()
        .setAspectMask(vk::ImageAspectFlagBits::eColor)
        .setMipLevel(0)
        .setBaseArrayLayer(0)
        .setLayerCount(1);

    auto copy_region = vk::BufferImageCopy2()
        .setImageSubresource(subresource_layers)
        .setImageExtent(vk::Extent3D(texture_width, texture_height, 1))
        .setImageOffset({0, 0, 0})
        .setBufferOffset(0)
        .setBufferImageHeight(0)
        .setBufferRowLength(0);

    auto copy_info = vk::CopyBufferToImageInfo2()
        .setSrcBuffer(staging_buffer.vk_buffer())
        .setDstImage(image.vk_image())
        .setDstImageLayout(vk::ImageLayout::eTransferDstOptimal)
        .setRegions(copy_region);

    cmd.copyBufferToImage2(copy_info);

    vulkan::VulkanPipelineBarriers::builder()
        .insert_image_memory_barrier(image.vk_image(),
            vk::ImageLayout::eTransferDstOptimal, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
            vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead,
            image.full_subresource_range()
        )
        .flush(cmd);

    cmd.end();

    auto fence_create_info = vk::FenceCreateInfo();
    auto fence = vulkan::VulkanContext::get().vk_device().createFence(fence_create_info);

    auto cb_submit_info = vk::CommandBufferSubmitInfo()
        .setCommandBuffer(cmd);

    auto submit_info = vk::SubmitInfo2()
        .setCommandBufferInfos(cb_submit_info);

    vulkan::VulkanContext::get().vk_graphics_queue().submit2(submit_info, fence);
    std::ignore = vulkan::VulkanContext::get().vk_device().waitForFences(*fence, true, std::numeric_limits<uint64_t>::max());

    std::println("Loaded texture {}: {}x{}x{}, mip levels: {}, format: {}", path.string(), texture_width, texture_height, texture_depth, mip_levels, vk::to_string(format));

    return image;
}

} // namespace pop::systems
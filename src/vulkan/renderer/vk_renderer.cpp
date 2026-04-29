#include "vk_renderer.hpp"

#include "vulkan/vk_context.hpp"
#include "vulkan/vk_pipeline_barriers.hpp"

#include <print>

namespace pop::vulkan::renderer {

VulkanRenderer::VulkanRenderer(VulkanSwapchain&& swapchain, VulkanPipelineLayout&& triangle_pipeline_layout, std::vector<FrameInFlight>&& frames_in_flight)
    : m_swapchain(std::move(swapchain)), m_triangle_pipeline_layout(std::move(triangle_pipeline_layout)), m_frames_in_flight(std::move(frames_in_flight)) {}
VulkanRenderer::~VulkanRenderer() {
    VulkanContext::get().vk_device().waitIdle();
}

auto VulkanRenderer::create(VulkanSwapchain&& swapchain) -> VulkanRenderer {
    std::vector<FrameInFlight> frames_in_flight;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        auto command_pool_create_info = vk::CommandPoolCreateInfo()
            .setQueueFamilyIndex(VulkanContext::get().vk_graphics_queue_family());
        auto signaled_fence_create_info = vk::FenceCreateInfo()
            .setFlags(vk::FenceCreateFlagBits::eSignaled);
        auto default_semaphore_create_info = vk::SemaphoreCreateInfo();


        auto command_pool = VulkanContext::get().vk_device().createCommandPool(command_pool_create_info);
        auto frame_command_buffer = std::move(VulkanContext::get().vk_device().allocateCommandBuffers({*command_pool, vk::CommandBufferLevel::ePrimary, 1})[0]);
        auto frame_finished_fence = VulkanContext::get().vk_device().createFence(signaled_fence_create_info);
        auto image_acquired_semaphore = VulkanContext::get().vk_device().createSemaphore(default_semaphore_create_info);

        frames_in_flight.emplace_back(std::move(frame_finished_fence), std::move(image_acquired_semaphore), std::move(command_pool), std::move(frame_command_buffer));
    }

    auto triangle_pipeline_layout = VulkanPipelineLayout::builder()
        .add_push_constant_range(0, 4, vk::ShaderStageFlagBits::eFragment)
        .build();

    return VulkanRenderer{ std::move(swapchain), std::move(triangle_pipeline_layout), std::move(frames_in_flight) };
}

auto VulkanRenderer::render_frame() -> RenderResult {
    auto& frame = m_frames_in_flight[m_current_frame];
    auto& device = VulkanContext::get().vk_device();
    device.waitForFences(*frame.frame_finished_fence, true, std::numeric_limits<uint64_t>::max());

    auto swapchain_acquire_result = m_swapchain.vk_swapchain().acquireNextImage(std::numeric_limits<uint64_t>::max(), frame.image_acquired_semaphore, {});

    if (swapchain_acquire_result.result == vk::Result::eSuboptimalKHR || swapchain_acquire_result.result == vk::Result::eErrorOutOfDateKHR) {
        return RenderResult::SwapchainSuboptimal;
    }

    if (swapchain_acquire_result.result != vk::Result::eSuccess) {
        std::println("ERROR: Failed to acquire swapchain image: {}", vk::to_string(swapchain_acquire_result.result));
        throw std::runtime_error("Failed to acquire swapchain image!");
    }

    auto& swapchain_image = m_swapchain.images()[*swapchain_acquire_result];

    device.resetFences(*frame.frame_finished_fence);

    frame.command_pool.reset();

    vk::CommandBufferBeginInfo command_buffer_begin_info = vk::CommandBufferBeginInfo()
        .setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    frame.frame_command_buffer.begin(command_buffer_begin_info);

    VulkanPipelineBarriers::builder()
        .insertImageMemoryBarrier(swapchain_image.vk_image(),
            vk::ImageLayout::eUndefined, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
            vk::ImageLayout::eTransferDstOptimal, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
            swapchain_image.full_subresource_range()
        )
        .flush(frame.frame_command_buffer);

    frame.frame_command_buffer.clearColorImage(swapchain_image.vk_image(), vk::ImageLayout::eTransferDstOptimal, { 0.2f, 0.2f, 0.2f, 1.0f }, swapchain_image.full_subresource_range());

    VulkanPipelineBarriers::builder()
        .insertImageMemoryBarrier(swapchain_image.vk_image(),
            vk::ImageLayout::eTransferDstOptimal, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
            vk::ImageLayout::ePresentSrcKHR, vk::PipelineStageFlagBits2::eAllCommands, vk::AccessFlagBits2::eNone,
            swapchain_image.full_subresource_range()
        )
        .flush(frame.frame_command_buffer);

    frame.frame_command_buffer.end();

    auto wait_semaphore_submit_info = vk::SemaphoreSubmitInfo()
        .setSemaphore(*frame.image_acquired_semaphore)
        .setStageMask(vk::PipelineStageFlagBits2::eTransfer);

    auto signal_semaphore_submit_info = vk::SemaphoreSubmitInfo()
        .setSemaphore(*swapchain_image.image_present_semaphore())
        .setStageMask(vk::PipelineStageFlagBits2::eAllCommands);

    auto command_buffer_info = vk::CommandBufferSubmitInfo{}
        .setCommandBuffer(frame.frame_command_buffer);

    auto submit_info = vk::SubmitInfo2()
        .setCommandBufferInfos(command_buffer_info)
        .setWaitSemaphoreInfos(wait_semaphore_submit_info)
        .setSignalSemaphoreInfos(signal_semaphore_submit_info);

    VulkanContext::get().vk_graphics_queue().submit2(submit_info, *frame.frame_finished_fence);

    auto present_info = vk::PresentInfoKHR()
        .setWaitSemaphores(*swapchain_image.image_present_semaphore())
        .setSwapchains(*m_swapchain.vk_swapchain())
        .setImageIndices(swapchain_acquire_result.value);

    m_current_frame = (m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;

    auto present_result = VulkanContext::get().vk_present_queue().presentKHR(present_info);

    if (present_result == vk::Result::eErrorOutOfDateKHR || present_result == vk::Result::eSuboptimalKHR) {
        return RenderResult::SwapchainSuboptimal;
    }

    if (present_result != vk::Result::eSuccess) {
        std::println("ERROR: Failed to submit present: {}", vk::to_string(swapchain_acquire_result.result));
        throw std::runtime_error("Failed to submit present!");
    }

    return RenderResult::Ok;
}
auto VulkanRenderer::recreate_swapchain() -> void {
    VulkanContext::get().vk_device().waitIdle();
    m_swapchain = VulkanSwapchain::create(m_swapchain.image_extent(), std::move(m_swapchain), true);
}

} // namespace pop::vulkan::renderer
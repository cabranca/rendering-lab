#include "Queue.h"
#include "Check.h"

#include <volk/volk.h>

namespace lab {

    void Queue::init(VkDevice device, VkSwapchainKHR swapchain, uint32_t queueFamily, uint32_t queueIndex) {
        vkGetDeviceQueue(device, queueFamily, queueIndex, &m_Queue);
        std::println("Queue created");

        m_Device = device;
        m_Swapchain = swapchain;
        
        m_RenderCompleteSem = createSemaphore(device);
        m_PresentCompleteSem = createSemaphore(device);
    }

    void Queue::shutdown() {
        vkDestroySemaphore(m_Device, m_RenderCompleteSem, nullptr);
        vkDestroySemaphore(m_Device, m_PresentCompleteSem, nullptr);
    }

	uint32_t Queue::acquireNextImage() {
        uint32_t imageIndex = 0;
        chk(vkAcquireNextImageKHR(m_Device, m_Swapchain, UINT64_MAX, m_PresentCompleteSem, nullptr, &imageIndex));
        return imageIndex;
    }

    void Queue::submitSync(VkCommandBuffer cmdBuffer) {
        VkPipelineStageFlags waitFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 0,
            .pWaitDstStageMask = &waitFlags,
            .commandBufferCount = 1,
            .pCommandBuffers = &cmdBuffer,
            .signalSemaphoreCount = 0,
        };

        chk(vkQueueSubmit(m_Queue, 1, &submitInfo, nullptr));
    }

	void Queue::submitAsync(VkCommandBuffer cmdBuffer) {
        VkPipelineStageFlags waitFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &m_PresentCompleteSem,
            .pWaitDstStageMask = &waitFlags,
            .commandBufferCount = 1,
            .pCommandBuffers = &cmdBuffer,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &m_RenderCompleteSem
        };

        chk(vkQueueSubmit(m_Queue, 1, &submitInfo, nullptr));
    }

	void Queue::present(uint32_t index) {
        VkPresentInfoKHR presentInfo{
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &m_RenderCompleteSem,
            .swapchainCount = 1,
            .pSwapchains = &m_Swapchain,
            .pImageIndices = &index
        };

        chk(vkQueuePresentKHR(m_Queue, &presentInfo));
    }

    void Queue::waitIdle() {

    }

    VkSemaphore Queue::createSemaphore(VkDevice device) {
        VkSemaphoreCreateInfo semCI{ 
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .flags = 0
        };

        VkSemaphore res;

        chk(vkCreateSemaphore(device, &semCI, nullptr, &res));

        return res;
    }
}
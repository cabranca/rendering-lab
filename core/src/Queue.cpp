#include "Queue.h"
#include "Check.h"

#include <volk/volk.h>

namespace lab {

    void Queue::init(VkDevice device, VkSwapchainKHR swapchain, uint32_t queueFamily, uint32_t queueIndex, uint32_t numImages) {
        vkGetDeviceQueue(device, queueFamily, queueIndex, &m_Queue);
        std::println("Queue created");

        m_Device = device;
        m_Swapchain = swapchain;
        m_NumImages = numImages;

        for (uint32_t i = 0; i < m_NumImages; i++) {
            m_RenderCompleteSem.emplace_back(createSemaphore(device));
            m_PresentCompleteSem.emplace_back(createSemaphore(device));
            m_InFlightFences.emplace_back(createFence(device));
        }
    }

    void Queue::shutdown() {
        for (uint32_t i = 0; i < m_NumImages; i++) {
            vkDestroyFence(m_Device, m_InFlightFences.at(i), nullptr);
            vkDestroySemaphore(m_Device, m_RenderCompleteSem.at(i), nullptr);
            vkDestroySemaphore(m_Device, m_PresentCompleteSem.at(i), nullptr);
        }
    }

	VkResult Queue::acquireNextImage(uint32_t& imageIndex) {
        chk(vkWaitForFences(m_Device, 1, &m_InFlightFences.at(m_FrameIndex), VK_TRUE, UINT64_MAX));

        // Note: the fence is reset in submitAsync, right before it is signalled again. Resetting
        // here would deadlock the next frame if acquire fails (OUT_OF_DATE) and we skip the submit.
        return vkAcquireNextImageKHR(m_Device, m_Swapchain, UINT64_MAX, m_PresentCompleteSem.at(m_FrameIndex), nullptr,
                                     &imageIndex);
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
        chk(vkResetFences(m_Device, 1, &m_InFlightFences.at(m_FrameIndex)));

        VkPipelineStageFlags waitFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &m_PresentCompleteSem.at(m_FrameIndex),
            .pWaitDstStageMask = &waitFlags,
            .commandBufferCount = 1,
            .pCommandBuffers = &cmdBuffer,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &m_RenderCompleteSem.at(m_FrameIndex)
        };

        chk(vkQueueSubmit(m_Queue, 1, &submitInfo, m_InFlightFences.at(m_FrameIndex)));
    }

	VkResult Queue::present(uint32_t index) {
        VkPresentInfoKHR presentInfo{
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &m_RenderCompleteSem.at(m_FrameIndex),
            .swapchainCount = 1,
            .pSwapchains = &m_Swapchain,
            .pImageIndices = &index
        };

        VkResult result = vkQueuePresentKHR(m_Queue, &presentInfo);

        m_FrameIndex = (m_FrameIndex + 1) % m_NumImages;

        return result;
    }

    void Queue::waitIdle() {
        chk(vkDeviceWaitIdle(m_Device));
    }

    void Queue::setSwapchain(VkSwapchainKHR swapchain) {
        m_Swapchain = swapchain;
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

    VkFence Queue::createFence(VkDevice device) {
        VkFenceCreateInfo fenceCI{
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT
        };

        VkFence res;

        chk(vkCreateFence(device, &fenceCI, nullptr, &res));

        return res;
    }
}
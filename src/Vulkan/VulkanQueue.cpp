#include "VulkanQueue.h"

#include "Utils.h"

namespace lab::vk {

    VulkanQueue::VulkanQueue(VkDevice device, uint32_t familyIndex) : m_Device(device), m_FamilyIndex(familyIndex) {
        vkGetDeviceQueue(m_Device, m_FamilyIndex, 0, &m_Queue);

		m_Pool = VulkanCommandPool(device, familyIndex);
		m_SingleTimePool = VulkanCommandPool(device, familyIndex, true);
    }

	uint32_t VulkanQueue::getQueueFamilyIndex() const {
		return m_FamilyIndex;
	}

	std::vector<VkCommandBuffer> VulkanQueue::allocateCommandBuffers(uint32_t count) const {
		return m_Pool.allocateBuffers(count);
	}

	void VulkanQueue::submitCommands(VkCommandBuffer buffer, VkFence fence, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore,
	                                 const std::vector<VkPipelineStageFlags>& waitDstStageMask) const {
		VkSemaphoreSubmitInfo waitInfo{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			                            .semaphore = waitSemaphore,
			                            .stageMask = waitDstStageMask.empty()
			                                             ? VK_PIPELINE_STAGE_2_NONE
			                                             : static_cast<VkPipelineStageFlags2>(waitDstStageMask.front()) };
		VkSemaphoreSubmitInfo signalInfo{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			                              .semaphore = signalSemaphore,
			                              .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT };
		VkCommandBufferSubmitInfo cmdInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO, .commandBuffer = buffer };

		VkSubmitInfo2 submitInfo{ .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
			                      .waitSemaphoreInfoCount = 1,
			                      .pWaitSemaphoreInfos = &waitInfo,
			                      .commandBufferInfoCount = 1,
			                      .pCommandBufferInfos = &cmdInfo,
			                      .signalSemaphoreInfoCount = 1,
			                      .pSignalSemaphoreInfos = &signalInfo };
		vkCheck(vkQueueSubmit2(m_Queue, 1, &submitInfo, fence), "vkQueueSubmit2");
	}

	VkResult VulkanQueue::present(VkSemaphore waitSemaphore, VkSwapchainKHR swapchain, uint32_t imageIndex) const {
		VkPresentInfoKHR presentInfo{
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.pNext = nullptr,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &waitSemaphore,
			.swapchainCount = 1,
			.pSwapchains = &swapchain,
			.pImageIndices = &imageIndex,
			.pResults = nullptr
		};
		return vkQueuePresentKHR(m_Queue, &presentInfo);
	}

	VkCommandBuffer VulkanQueue::beginSingleTimeCommands() const {
		VkCommandBuffer cmdBuffer = m_SingleTimePool.allocateBuffers(1).front();

		VkCommandBufferBeginInfo cmdBufferBI {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.pNext = nullptr,
			.flags = 0,
			.pInheritanceInfo = nullptr
		};
		vkCheck(vkBeginCommandBuffer(cmdBuffer, &cmdBufferBI), "vkBeginCommandBuffer");
		return cmdBuffer;
	}

	void VulkanQueue::endSingleTimeCommands(VkCommandBuffer commandBuffer) const {
		vkCheck(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer");

		VkSubmitInfo submitInfo{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.pNext = nullptr,
			.waitSemaphoreCount = 0,
			.pWaitSemaphores = nullptr,
			.pWaitDstStageMask = nullptr,
			.commandBufferCount = 1,
			.pCommandBuffers = &commandBuffer,
			.signalSemaphoreCount = 0,
			.pSignalSemaphores = nullptr
		};
		vkCheck(vkQueueSubmit(m_Queue, 1, &submitInfo, nullptr), "vkQueueSubmit");
		vkCheck(vkQueueWaitIdle(m_Queue), "vkQueueWaitIdle");
	}
}
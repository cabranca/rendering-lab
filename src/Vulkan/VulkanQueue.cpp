#include "VulkanQueue.h"

#include "Logger.h"
#include "Utils.h"

namespace lab::vk {

    VulkanQueue::VulkanQueue(VkDevice device, uint32_t familyIndex) : m_Device(device), m_FamilyIndex(familyIndex) {
        vkGetDeviceQueue(m_Device, m_FamilyIndex, 0, &m_Queue);

		m_Pool = VulkanCommandPool(device, familyIndex);

		VkCommandPoolCreateInfo poolCI {
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.pNext = nullptr,
			.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
			.queueFamilyIndex = m_FamilyIndex
		};
		vkCheck(vkCreateCommandPool(m_Device, &poolCI, nullptr, &m_SingleTimePool), "vkCreateCommandPool");
    }

	VulkanQueue::~VulkanQueue() {
		if (m_SingleTimePool != VK_NULL_HANDLE)
			vkDestroyCommandPool(m_Device, m_SingleTimePool, nullptr);
	}

	uint32_t VulkanQueue::getQueueFamilyIndex() const {
		return m_FamilyIndex;
	}

	void VulkanQueue::submitCommands(VkCommandBuffer buffer, VkFence fence, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore,
	                                 const std::vector<VkPipelineStageFlags>& waitDstStageMask) const {
		VkSubmitInfo submitInfo{ .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			                     .pNext = nullptr,
			                     .waitSemaphoreCount = 1,
			                     .pWaitSemaphores = &waitSemaphore,
			                     .pWaitDstStageMask = waitDstStageMask.data(),
			                     .commandBufferCount = 1,
			                     .pCommandBuffers = &buffer,
			                     .signalSemaphoreCount = 1,
			                     .pSignalSemaphores = &signalSemaphore };
		vkCheck(vkQueueSubmit(m_Queue, 1, &submitInfo, fence), "vkQueueSubmit");
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

	VkCommandBuffer VulkanQueue::beginSingleTimeCommands() {
		VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
		VkCommandBufferAllocateInfo cmdBufferAI {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.pNext = nullptr,
			.commandPool = m_SingleTimePool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1
		};
		vkCheck(vkAllocateCommandBuffers(m_Device, &cmdBufferAI, &cmdBuffer), "vkAllocateCommandBuffers");

		VkCommandBufferBeginInfo cmdBufferBI {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.pNext = nullptr,
			.flags = 0,
			.pInheritanceInfo = nullptr
		};
		vkCheck(vkBeginCommandBuffer(cmdBuffer, &cmdBufferBI), "vkBeginCommandBuffer");
		return cmdBuffer;
	}

	void VulkanQueue::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
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
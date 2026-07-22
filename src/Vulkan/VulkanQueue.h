#pragma once

#include <vulkan/vulkan.h>

#include "VulkanCommandPool.h"

namespace lab::vk {

	class VulkanQueue {
	  public:
		VulkanQueue() = default;
		VulkanQueue(VkDevice device, uint32_t familyIndex);
		~VulkanQueue();
		VulkanQueue(const VulkanQueue& other) = default;
		VulkanQueue& operator=(const VulkanQueue& other) = default;
		VulkanQueue(VulkanQueue&& other) = default;
		VulkanQueue& operator=(VulkanQueue&& other) = default;

		[[nodiscard]] uint32_t getQueueFamilyIndex() const;

		void submitCommands(VkCommandBuffer buffer, VkFence fence, VkSemaphore semaphore, VkSemaphore signalSemaphore,
		                    const std::vector<VkPipelineStageFlags>& waitDstStageMask) const;
		VkResult present(VkSemaphore waitSemaphore, VkSwapchainKHR swapchain, uint32_t imageIndex) const;
		VkCommandBuffer beginSingleTimeCommands();
		void endSingleTimeCommands(VkCommandBuffer commandBuffer);

	  private:
		VkDevice m_Device = VK_NULL_HANDLE; // NON-OWNING
		VkQueue m_Queue = VK_NULL_HANDLE;
		uint32_t m_FamilyIndex = 0;
		VulkanCommandPool m_Pool;
		VkCommandPool m_SingleTimePool = VK_NULL_HANDLE;
	};
} // namespace lab::vk
#pragma once

#include <volk/volk.h>

#include "VulkanCommandPool.h"

namespace lab::vk {

	class VulkanQueue {
	  public:
		VulkanQueue() = default;
		VulkanQueue(VkDevice device, uint32_t familyIndex);
		VulkanQueue(const VulkanQueue& other) = delete;
		VulkanQueue& operator=(const VulkanQueue& other) = delete;
		VulkanQueue(VulkanQueue&& other) noexcept = default;
		VulkanQueue& operator=(VulkanQueue&& other) noexcept = default;

		[[nodiscard]] uint32_t getQueueFamilyIndex() const;
		[[nodiscard]] std::vector<VkCommandBuffer> allocateCommandBuffers(uint32_t count) const;

		void submitCommands(VkCommandBuffer buffer, VkFence fence, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore,
		                    const std::vector<VkPipelineStageFlags>& waitDstStageMask) const;
		VkResult present(VkSemaphore waitSemaphore, VkSwapchainKHR swapchain, uint32_t imageIndex) const;
		[[nodiscard]] VkCommandBuffer beginSingleTimeCommands() const;
		void endSingleTimeCommands(VkCommandBuffer commandBuffer) const;

	  private:
		VkDevice m_Device = VK_NULL_HANDLE; // NON-OWNING
		VkQueue m_Queue = VK_NULL_HANDLE;
		uint32_t m_FamilyIndex = 0;
		VulkanCommandPool m_Pool;
		VulkanCommandPool m_SingleTimePool;
	};
} // namespace lab::vk
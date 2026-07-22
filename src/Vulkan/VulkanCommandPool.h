#pragma once

#include <vector>

#include <vulkan/vulkan.h>

namespace lab::vk {

	class VulkanCommandPool {
	  public:
		VulkanCommandPool() = default;
		explicit VulkanCommandPool(VkDevice device, uint32_t familyIndex);
		~VulkanCommandPool();
		VulkanCommandPool(const VulkanCommandPool& other) = default;
		VulkanCommandPool& operator=(const VulkanCommandPool& other) = default;
		VulkanCommandPool(VulkanCommandPool&& other) = default;
		VulkanCommandPool& operator=(VulkanCommandPool&& other) = default;

		[[nodiscard]] std::vector<VkCommandBuffer> allocateBuffers(uint32_t count);

	  private:
		VkDevice m_Device = VK_NULL_HANDLE; // NON-OWNING
		VkCommandPool m_Pool = VK_NULL_HANDLE;
	};
} // namespace lab::vk
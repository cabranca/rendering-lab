#pragma once

#include <vector>

#include <volk/volk.h>

namespace lab::vk {

	class VulkanCommandPool {
	  public:
		VulkanCommandPool() = default;
		// transient => VK_COMMAND_POOL_CREATE_TRANSIENT_BIT (short-lived, one-shot buffers);
		// otherwise VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT (per-frame, individually resettable).
		VulkanCommandPool(VkDevice device, uint32_t familyIndex, bool transient = false);
		~VulkanCommandPool();
		VulkanCommandPool(const VulkanCommandPool& other) = delete;
		VulkanCommandPool& operator=(const VulkanCommandPool& other) = delete;
		VulkanCommandPool(VulkanCommandPool&& other) noexcept;
		VulkanCommandPool& operator=(VulkanCommandPool&& other) noexcept;

		[[nodiscard]] std::vector<VkCommandBuffer> allocateBuffers(uint32_t count) const;

	  private:
		VkDevice m_Device = VK_NULL_HANDLE; // NON-OWNING
		VkCommandPool m_Pool = VK_NULL_HANDLE;
	};
} // namespace lab::vk
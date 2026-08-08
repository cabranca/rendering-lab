#pragma once

#include <volk/volk.h>

#include "VulkanDevice.h"

namespace lab::vk {

	class VulkanBuffer {
	  public:
		VulkanBuffer() = default;
		VulkanBuffer(const VulkanDevice& device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
		~VulkanBuffer();
		VulkanBuffer(const VulkanBuffer& other) = delete;
		VulkanBuffer& operator=(const VulkanBuffer& other) = delete;
		VulkanBuffer(VulkanBuffer&& other) noexcept;
		VulkanBuffer& operator=(VulkanBuffer&& other) noexcept;

		[[nodiscard]] VkBuffer getBuffer() const;

		void setData(const void* src);
		[[nodiscard]] void* map();
		void unmap();

	  private:
		VkDevice m_Device = VK_NULL_HANDLE; // NON-OWNING
		VkBuffer m_Buffer = VK_NULL_HANDLE;
		VkDeviceMemory m_Memory = VK_NULL_HANDLE;
		VkDeviceSize m_Size = 0;
	};
} // namespace lab::vk
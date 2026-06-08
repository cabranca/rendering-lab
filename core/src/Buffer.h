#pragma once

#include "vulkan/vulkan_core.h"
#include "vma/vk_mem_alloc.h"

namespace lab {

	class VulkanContext;

	// Thin wrapper around a VMA buffer allocation.
	// - hostVisible buffers are persistently mapped; write through getMappedData().
	// - If usage contains VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, getDeviceAddress() is valid.
	class Buffer {
	  public:
		void init(const VulkanContext& ctx, VkDeviceSize size, VkBufferUsageFlags usage, bool hostVisible);
		void shutdown(const VulkanContext& ctx);

		VkBuffer getBuffer() const {
			return m_Buffer;
		}
		void* getMappedData() const {
			return m_AllocationInfo.pMappedData;
		}
		VkDeviceAddress getDeviceAddress() const {
			return m_DeviceAddress;
		}
		VkDeviceSize getSize() const {
			return m_Size;
		}

	  private:
		VmaAllocation m_Allocation{ VK_NULL_HANDLE };
		VmaAllocationInfo m_AllocationInfo{};
		VkBuffer m_Buffer{ VK_NULL_HANDLE };
		VkDeviceAddress m_DeviceAddress{ 0 };
		VkDeviceSize m_Size{ 0 };
	};
} // namespace lab

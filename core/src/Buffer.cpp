#include "Buffer.h"

#include <volk/volk.h>

#include "Check.h"
#include "VulkanContext.h"

namespace lab {

	void Buffer::init(const VulkanContext& ctx, VkDeviceSize size, VkBufferUsageFlags usage, bool hostVisible) {
		m_Size = size;
		VkBufferCreateInfo bufferCI{ .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = size, .usage = usage };
		VmaAllocationCreateInfo allocCI{ .usage = VMA_MEMORY_USAGE_AUTO };
		if (hostVisible) {
			allocCI.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
			                VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
		}
		chk(vmaCreateBuffer(ctx.getAllocator(), &bufferCI, &allocCI, &m_Buffer, &m_Allocation, &m_AllocationInfo));

		if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
			VkBufferDeviceAddressInfo bdaInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = m_Buffer };
			m_DeviceAddress = vkGetBufferDeviceAddress(ctx.getDevice(), &bdaInfo);
		}
	}

	void Buffer::shutdown(const VulkanContext& ctx) {
		vmaDestroyBuffer(ctx.getAllocator(), m_Buffer, m_Allocation);
	}
} // namespace lab

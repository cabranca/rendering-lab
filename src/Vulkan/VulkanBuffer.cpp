#include "VulkanBuffer.h"

#include "Utils.h"

namespace lab::vk {

	VulkanBuffer::VulkanBuffer(const VulkanDevice& device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
	    : m_Device(device.getDevice()) {
		VkBufferCreateInfo bufferCI{ .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			                         .pNext = nullptr,
			                         .flags = 0,
			                         .size = size,
			                         .usage = usage,
			                         .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
			                         .queueFamilyIndexCount = 0,
			                         .pQueueFamilyIndices = nullptr };

		vkCheck(vkCreateBuffer(m_Device, &bufferCI, nullptr, &m_Buffer), "vkCreateBuffer");

		VkMemoryRequirements memReq;
		vkGetBufferMemoryRequirements(m_Device, m_Buffer, &memReq);

		VkMemoryAllocateInfo memAI{ .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			                        .pNext = nullptr,
			                        .allocationSize = memReq.size,
			                        .memoryTypeIndex = device.findMemoryType(memReq.memoryTypeBits, properties) };

		vkCheck(vkAllocateMemory(m_Device, &memAI, nullptr, &m_Memory), "vkAllocateMemory");
		vkCheck(vkBindBufferMemory(m_Device, m_Buffer, m_Memory, 0), "vkBindBufferMemory");
	}

	VulkanBuffer::~VulkanBuffer() {
		if (m_Memory != VK_NULL_HANDLE)
			vkFreeMemory(m_Device, m_Memory, nullptr);

		if (m_Buffer != VK_NULL_HANDLE)
			vkDestroyBuffer(m_Device, m_Buffer, nullptr);
	}

	VkBuffer VulkanBuffer::getBuffer() const {
		return m_Buffer;
	}

	void VulkanBuffer::setData(const void* src) {
		void* dataStaging = nullptr;
		map(dataStaging);
		memcpy(dataStaging, src, m_Size);
		unmap();
	}

	void VulkanBuffer::map(void* memory) {
		vkCheck(vkMapMemory(m_Device, m_Memory, 0, m_Size, 0, &memory), "vkMapMemory");
	}

	void VulkanBuffer::unmap() {
		vkUnmapMemory(m_Device, m_Memory);
	}
} // namespace lab::vk
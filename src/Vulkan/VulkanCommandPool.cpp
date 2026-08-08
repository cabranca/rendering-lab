#include "VulkanCommandPool.h"

#include <utility>

#include "Logger.h"
#include "Utils.h"

namespace lab::vk {

    VulkanCommandPool::VulkanCommandPool(VkDevice device, uint32_t familyIndex, bool transient) : m_Device(device) {
        VkCommandPoolCreateInfo poolCI {
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.pNext = nullptr,
			.flags = transient ? VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
			                   : static_cast<VkCommandPoolCreateFlags>(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT),
			.queueFamilyIndex = familyIndex
		};
		vkCheck(vkCreateCommandPool(m_Device, &poolCI, nullptr, &m_Pool), "vkCreateCommandPool");
		CBK_DEBUG("Vulkan Command Pool created");
    }

    VulkanCommandPool::~VulkanCommandPool() {
        if (m_Pool != VK_NULL_HANDLE)
			vkDestroyCommandPool(m_Device, m_Pool, nullptr);
    }

	VulkanCommandPool::VulkanCommandPool(VulkanCommandPool&& other) noexcept
	    : m_Device(other.m_Device), m_Pool(std::exchange(other.m_Pool, VK_NULL_HANDLE)) {}

	VulkanCommandPool& VulkanCommandPool::operator=(VulkanCommandPool&& other) noexcept {
		if (this != &other) {
			if (m_Pool != VK_NULL_HANDLE)
				vkDestroyCommandPool(m_Device, m_Pool, nullptr);
			m_Device = other.m_Device;
			m_Pool = std::exchange(other.m_Pool, VK_NULL_HANDLE);
		}
		return *this;
	}

    std::vector<VkCommandBuffer> VulkanCommandPool::allocateBuffers(uint32_t count) const {
        std::vector<VkCommandBuffer> buffers(count);
        
        VkCommandBufferAllocateInfo cmdBufferAI {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.pNext = nullptr,
			.commandPool = m_Pool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = count
		};
		vkCheck(vkAllocateCommandBuffers(m_Device, &cmdBufferAI, buffers.data()), "vkAllocateCommandBuffers");

        return buffers;
    }
}
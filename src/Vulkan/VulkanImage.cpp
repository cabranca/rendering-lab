#include "VulkanImage.h"

#include "Logger.h"
#include "Utils.h"

namespace lab::vk {

	VulkanImage::VulkanImage(const VulkanDevice& device, uint32_t width, uint32_t height, uint32_t mipLevels, bool useMultisampling,
	                         VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
	                         VkImageAspectFlags aspectFlags)
	    : m_Device(device.getDevice()) {
		VkImageCreateInfo imageCI{ .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			                       .pNext = nullptr,
			                       .flags = 0,
			                       .imageType = VK_IMAGE_TYPE_2D,
			                       .format = format,
			                       .extent = { .width = width, .height = height, .depth = 1 },
			                       .mipLevels = mipLevels,
			                       .arrayLayers = 1,
			                       .samples = useMultisampling ? device.getMSAA() : VK_SAMPLE_COUNT_1_BIT,
			                       .tiling = tiling,
			                       .usage = usage,
			                       .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
			                       .queueFamilyIndexCount = VK_QUEUE_FAMILY_IGNORED,
			                       .pQueueFamilyIndices = nullptr,
			                       .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED };
		vkCheck(vkCreateImage(m_Device, &imageCI, nullptr, &m_Image), "vkCreateImage");

		VkMemoryRequirements memReq;
		vkGetImageMemoryRequirements(m_Device, m_Image, &memReq);
		VkMemoryAllocateInfo memAI{ .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			                        .pNext = nullptr,
			                        .allocationSize = memReq.size,
			                        .memoryTypeIndex = device.findMemoryType(memReq.memoryTypeBits, properties) };
		vkCheck(vkAllocateMemory(m_Device, &memAI, nullptr, &m_Memory), "vkAllocateMemory");
		vkCheck(vkBindImageMemory(m_Device, m_Image, m_Memory, 0), "vkBindImageMemory");

		VkImageViewCreateInfo viewCI{ .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			                          .pNext = nullptr,
			                          .flags = 0,
			                          .image = m_Image,
			                          .viewType = VK_IMAGE_VIEW_TYPE_2D,
			                          .format = format,
			                          .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY, .g = VK_COMPONENT_SWIZZLE_IDENTITY, .b = VK_COMPONENT_SWIZZLE_IDENTITY, .a = VK_COMPONENT_SWIZZLE_IDENTITY, },
			                          .subresourceRange = { .aspectMask = aspectFlags,
			                                                .baseMipLevel = 0,
			                                                .levelCount = mipLevels,
			                                                .baseArrayLayer = 0,
			                                                .layerCount = 1 } };
		vkCheck(vkCreateImageView(m_Device, &viewCI, nullptr, &m_View), "vkCreateImageView");
	}

	VulkanImage::~VulkanImage() {
		destroy();
	}

	VulkanImage& VulkanImage::operator=(VulkanImage&& other) noexcept {
		if (this != &other) {
			destroy();
			m_Device = other.m_Device;
			m_Image = std::exchange(other.m_Image, VK_NULL_HANDLE);
			m_Memory = std::exchange(other.m_Memory, VK_NULL_HANDLE);
			m_View = std::exchange(other.m_View, VK_NULL_HANDLE);
		}
		return *this;
	}

	VkImageView VulkanImage::getView() const {
		return m_View;
	}

	void VulkanImage::destroy() {
		if (m_View != VK_NULL_HANDLE)
			vkDestroyImageView(m_Device, m_View, nullptr);

		if (m_Memory != VK_NULL_HANDLE)
			vkFreeMemory(m_Device, m_Memory, nullptr);

		if (m_Image != VK_NULL_HANDLE)
			vkDestroyImage(m_Device, m_Image, nullptr);
	}
} // namespace lab::vk
#pragma once

#include <volk/volk.h>

#include "VulkanDevice.h"

namespace lab::vk {

	class VulkanImage {
	  public:
		VulkanImage() = default;
		VulkanImage(const VulkanDevice& device, uint32_t width, uint32_t height, uint32_t mipLevels, bool useMultisampling, VkFormat format,
		            VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImageAspectFlags aspectFlags);
		~VulkanImage();
		VulkanImage(const VulkanImage& other) = delete;
		VulkanImage& operator=(const VulkanImage& other) = delete;
		VulkanImage(VulkanImage&& other) noexcept;
		VulkanImage& operator=(VulkanImage&& other) noexcept;

		[[nodiscard]] VkImage getImage() const;
		[[nodiscard]] VkImageView getView() const;

	  private:
		VkDevice m_Device = VK_NULL_HANDLE; // NON-OWNING
		VkImage m_Image = VK_NULL_HANDLE;
		VkDeviceMemory m_Memory = VK_NULL_HANDLE;
		VkImageView m_View = VK_NULL_HANDLE;

		void destroy();
	};
} // namespace lab::vk
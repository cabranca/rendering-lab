#pragma once

#include <vulkan/vulkan.h>

namespace lab::vk {

	class VulkanCommands {
	  public:
		static void copyBuffer(VkCommandBuffer cmdBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
		static void copyBufferToImage(VkCommandBuffer cmdBuffer, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
		static void transitionImageLayout(VkCommandBuffer cmdBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
		                                  VkAccessFlags2 srcAccessMask, VkAccessFlags2 dstAccessMask, VkPipelineStageFlags2 srcStageMask,
		                                  VkPipelineStageFlags2 dstStageMask, VkImageAspectFlags aspectFlags, uint32_t mipLevels);
	};
} // namespace lab::vk
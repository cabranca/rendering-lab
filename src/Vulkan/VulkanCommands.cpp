#include "VulkanCommands.h"

namespace lab::vk {

	void VulkanCommands::copyBuffer(VkCommandBuffer cmdBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
		VkBufferCopy region{ .srcOffset = 0, .dstOffset = 0, .size = size };
		vkCmdCopyBuffer(cmdBuffer, srcBuffer, dstBuffer, 1, &region);
	}

	void VulkanCommands::copyBufferToImage(VkCommandBuffer cmdBuffer, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
		VkBufferImageCopy2 region{
			.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
			.pNext = nullptr,
			.bufferOffset = 0,
			.bufferRowLength = 0,
			.bufferImageHeight = 0,
			.imageSubresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1 },
			.imageOffset = { .x = 0, .y = 0, .z = 0 },
			.imageExtent = { .width = width, .height = height, .depth = 1 }
		};
		VkCopyBufferToImageInfo2 copyInfo{ .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2,
			                               .pNext = nullptr,
			                               .srcBuffer = buffer,
			                               .dstImage = image,
			                               .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			                               .regionCount = 1,
			                               .pRegions = &region };
		vkCmdCopyBufferToImage2(cmdBuffer, &copyInfo);
	}

	void VulkanCommands::transitionImageLayout(VkCommandBuffer buffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
	                                           VkAccessFlags2 srcAccessMask, VkAccessFlags2 dstAccessMask,
	                                           VkPipelineStageFlags2 srcStageMask, VkPipelineStageFlags2 dstStageMask,
	                                           VkImageAspectFlags aspectFlags, uint32_t mipLevels) {
		VkImageMemoryBarrier2 barrier{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			                           .pNext = nullptr,
			                           .srcStageMask = srcStageMask,
			                           .srcAccessMask = srcAccessMask,
			                           .dstStageMask = dstStageMask,
			                           .dstAccessMask = dstAccessMask,
			                           .oldLayout = oldLayout,
			                           .newLayout = newLayout,
			                           .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			                           .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			                           .image = image,
			                           .subresourceRange = { .aspectMask = aspectFlags,
			                                                 .baseMipLevel = 0,
			                                                 .levelCount = mipLevels,
			                                                 .baseArrayLayer = 0,
			                                                 .layerCount = 1 } };

		VkDependencyInfo dependencyInfo{ .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			                             .pNext = nullptr,
			                             .dependencyFlags = 0,
			                             .memoryBarrierCount = 0,
			                             .pMemoryBarriers = nullptr,
			                             .bufferMemoryBarrierCount = 0,
			                             .pBufferMemoryBarriers = nullptr,
			                             .imageMemoryBarrierCount = 1,
			                             .pImageMemoryBarriers = &barrier };

		vkCmdPipelineBarrier2(buffer, &dependencyInfo);
	}
} // namespace lab::vk
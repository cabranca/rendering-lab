#include "VulkanTexture.h"

#include <format>
#include <stdexcept>
#include <utility>

#include "stb_image.h"
#include "Logger.h"
#include "Utils.h"
#include "VulkanBuffer.h"
#include "VulkanCommands.h"

namespace lab::vk {

	VulkanTexture::VulkanTexture(std::string_view path, const VulkanDevice& device) : m_Device(device.getDevice()) {
		int width, height, channels;
		stbi_uc* pixels = stbi_load(path.data(), &width, &height, &channels, STBI_rgb_alpha);
		if (!pixels) {
			throw std::runtime_error(std::format("Couldn't load texture {}", path));
		}

		m_MipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
		auto textureSize = width * height * 4;
		VulkanBuffer stagingBuffer(device, textureSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		stagingBuffer.setData(pixels);

		stbi_image_free(pixels);

		m_Image = VulkanImage(device, static_cast<uint32_t>(width), static_cast<uint32_t>(height), m_MipLevels, false,
		                      VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
		                      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

		const auto& queue = device.getQueue();
		auto copyCmdBuffer = queue.beginSingleTimeCommands();
		VulkanCommands::transitionImageLayout(copyCmdBuffer, m_Image.getImage(), VK_IMAGE_LAYOUT_UNDEFINED,
		                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_2_NONE, VK_ACCESS_2_TRANSFER_WRITE_BIT,
		                                      VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
		                                      VK_IMAGE_ASPECT_COLOR_BIT, m_MipLevels);
		VulkanCommands::copyBufferToImage(copyCmdBuffer, stagingBuffer.getBuffer(), m_Image.getImage(), width, height);
		generateMipmaps(copyCmdBuffer, m_Image.getImage(), VK_FORMAT_R8G8B8A8_SRGB, width, height, m_MipLevels, device.getPhysicalDevice());
		queue.endSingleTimeCommands(copyCmdBuffer);

		createTextureSampler(device.getPhysicalDevice());

		CBK_DEBUG("Texture created");
	}

	VulkanTexture::~VulkanTexture() {
		if (m_Sampler != VK_NULL_HANDLE)
			vkDestroySampler(m_Device, m_Sampler, nullptr);
	}

	VulkanTexture::VulkanTexture(VulkanTexture&& other) noexcept
	    : m_Device(other.m_Device),
	      m_Image(std::move(other.m_Image)),
	      m_Sampler(std::exchange(other.m_Sampler, VK_NULL_HANDLE)),
	      m_MipLevels(other.m_MipLevels) {}

	VulkanTexture& VulkanTexture::operator=(VulkanTexture&& other) noexcept {
		if (this != &other) {
			if (m_Sampler != VK_NULL_HANDLE)
				vkDestroySampler(m_Device, m_Sampler, nullptr);
			m_Device = other.m_Device;
			m_Image = std::move(other.m_Image);
			m_Sampler = std::exchange(other.m_Sampler, VK_NULL_HANDLE);
			m_MipLevels = other.m_MipLevels;
		}
		return *this;
	}

	VkImageView VulkanTexture::getView() const {
		return m_Image.getView();
	}

	VkSampler VulkanTexture::getSampler() const {
		return m_Sampler;
	}

	void VulkanTexture::generateMipmaps(VkCommandBuffer cmdBuffer, VkImage image, VkFormat format, int32_t width, int32_t height,
	                                    uint32_t mipLevels, VkPhysicalDevice physicalDevice) {
		VkFormatProperties2 formatProperties{ .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2 };
		vkGetPhysicalDeviceFormatProperties2(physicalDevice, format, &formatProperties);
		if (!(formatProperties.formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
			CBK_FATAL("Texture image format does not support linear blitting; cannot generate mipmaps!");
			return;
		}

		VkImageMemoryBarrier2 barrier{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			                           .pNext = nullptr,
			                           .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			                           .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			                           .image = image,
			                           .subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			                                                 .baseMipLevel = 0,
			                                                 .levelCount = 1,
			                                                 .baseArrayLayer = 0,
			                                                 .layerCount = 1 } };

		VkDependencyInfo dependency{ .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			                         .pNext = nullptr,
			                         .dependencyFlags = 0,
			                         .memoryBarrierCount = 0,
			                         .pMemoryBarriers = nullptr,
			                         .bufferMemoryBarrierCount = 0,
			                         .pBufferMemoryBarriers = nullptr,
			                         .imageMemoryBarrierCount = 1,
			                         .pImageMemoryBarriers = &barrier };

		int32_t mipWidth = width;
		int32_t mipHeight = height;
		for (uint32_t i = 1; i < mipLevels; i++) {
			// Wait for level i-1 to be written, then make it a transfer source for the blit.
			barrier.subresourceRange.baseMipLevel = i - 1;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
			barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
			barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
			barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;

			vkCmdPipelineBarrier2(cmdBuffer, &dependency);

			VkImageBlit2 region{
				.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
				.pNext = nullptr,
				.srcSubresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = i - 1, .baseArrayLayer = 0, .layerCount = 1 },
				.srcOffsets = { {}, { .x = mipWidth, .y = mipHeight, .z = 1 } },
				.dstSubresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = i, .baseArrayLayer = 0, .layerCount = 1 },
				.dstOffsets = { {}, { .x = mipWidth > 1 ? mipWidth / 2 : 1, .y = mipHeight > 1 ? mipHeight / 2 : 1, .z = 1 } }
			};

			VkBlitImageInfo2 blitInfo{ .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
				                       .pNext = nullptr,
				                       .srcImage = image,
				                       .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				                       .dstImage = image,
				                       .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				                       .regionCount = 1,
				                       .pRegions = &region,
				                       .filter = VK_FILTER_LINEAR };

			vkCmdBlitImage2(cmdBuffer, &blitInfo);

			// Level i-1 is finished: make it available to the fragment shader.
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
			barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
			barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
			barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

			vkCmdPipelineBarrier2(cmdBuffer, &dependency);

			if (mipWidth > 1)
				mipWidth /= 2;
			if (mipHeight > 1)
				mipHeight /= 2;
		}

		// The last level was only ever a blit destination, so it's still in TRANSFER_DST.
		barrier.subresourceRange.baseMipLevel = mipLevels - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
		barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

		vkCmdPipelineBarrier2(cmdBuffer, &dependency);
	}

	void VulkanTexture::createTextureSampler(VkPhysicalDevice physicalDevice) {
		VkPhysicalDeviceProperties prop;
		vkGetPhysicalDeviceProperties(physicalDevice, &prop);

		VkSamplerCreateInfo samplerCI{ .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			                           .pNext = nullptr,
			                           .flags = 0,
			                           .magFilter = VK_FILTER_LINEAR,
			                           .minFilter = VK_FILTER_LINEAR,
			                           .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
			                           .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			                           .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			                           .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			                           .mipLodBias = 0.0f,
			                           .anisotropyEnable = VK_TRUE,
			                           .maxAnisotropy = prop.limits.maxSamplerAnisotropy,
			                           .compareEnable = VK_FALSE,
			                           .compareOp = VK_COMPARE_OP_ALWAYS,
			                           .minLod = 0.0f,
			                           .maxLod = VK_LOD_CLAMP_NONE,
			                           .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
			                           .unnormalizedCoordinates = VK_FALSE };
		vkCheck(vkCreateSampler(m_Device, &samplerCI, nullptr, &m_Sampler), "vkCreateSampler");
	}
} // namespace lab::vk
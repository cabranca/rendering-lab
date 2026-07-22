#pragma once

#include "VulkanImage.h"

namespace lab::vk {

	class VulkanTexture {
	  public:
		VulkanTexture() = default;
		explicit VulkanTexture(std::string_view path, const VulkanDevice* device);
		~VulkanTexture();
		VulkanTexture(const VulkanTexture& other) = default;
		VulkanTexture& operator=(const VulkanTexture& other) = default;
		VulkanTexture(VulkanTexture&& other) = default;
		VulkanTexture& operator=(VulkanTexture&& other) = default;

		[[nodiscard]] VkImageView getView() const;
		[[nodiscard]] VkSampler getSampler() const;

	  private:
		VkDevice m_Device = VK_NULL_HANDLE; // NON-OWNING
		VulkanImage m_Image;
		VkSampler m_Sampler = VK_NULL_HANDLE;

		uint32_t m_MipLevels = 0;

		static void generateMipmaps(VkCommandBuffer cmdBuffer, VkImage image, VkFormat format, int32_t width, int32_t height,
		                            uint32_t mipLevels, VkPhysicalDevice physicalDevice);
		void createTextureSampler(VkPhysicalDevice physicalDevice);
	};
} // namespace lab::vk
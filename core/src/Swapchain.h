#pragma once

#include <vector>

#include <vulkan/vulkan_core.h>

#include "DeviceManager.h"

namespace lab {

	class Swapchain {
	  public:
		void init(VkDevice device, const PhysicalDevice& physicalDevice, VkSurfaceKHR surface, uint32_t queueFamilyIndex);
		void shutdown(VkDevice device);

		uint32_t getNumImages() const;
		VkImage getImage(uint32_t index) const;
		VkImageView getImageView(uint32_t index) const;
		VkSwapchainKHR getSwapchain() const;
		VkSurfaceFormatKHR getSurfaceFormat() const;
		VkExtent2D getExtent() const;

	  private:
		VkInstance m_Instance;
		VkSwapchainKHR m_Swapchain;
		std::vector<VkImage> m_Images;
		std::vector<VkImageView> m_ImageViews;
		VkSurfaceFormatKHR m_SurfaceFormat;
		VkExtent2D m_Extent{};

		VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& presentModes);
		uint32_t chooseNumImages(const VkSurfaceCapabilitiesKHR& surfaceCaps);
		VkSurfaceFormatKHR chooseSurfaceFormatAndColorSpace(const std::vector<VkSurfaceFormatKHR>& surfaceFormats);
		VkImageView createImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags flags, VkImageViewType viewType,
		                            uint32_t layerCount, uint32_t mipLevels);
	};
} // namespace lab
#pragma once

#include <vector>

#include <vulkan/vulkan_core.h>

namespace lab {

	class Swapchain {
	  public:
		void init(VkInstance instance);
		void shutdown();

	  private:
		VkInstance m_Instance;
		VkSwapchainKHR m_Swapchain;
		std::vector<VkImage> m_Images;
		std::vector<VkImageView> m_ImageViews;

        VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& presentModes);
        uint32_t chooseNumImages(const VkSurfaceCapabilitiesKHR& surfaceCaps);
		VkSurfaceFormatKHR chooseSurfaceFormatAndColorSpace(const std::vector<VkSurfaceFormatKHR>& surfaceFormats);
	};
} // namespace lab
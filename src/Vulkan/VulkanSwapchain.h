#pragma once

#include <vector>

#include <volk/volk.h>

#include "VulkanDevice.h"
#include "VulkanImage.h"

namespace lab::vk {

	class VulkanSwapchain {
	  public:
		VulkanSwapchain(VulkanDevice* device, SDL_Window* window);
		~VulkanSwapchain();
        VulkanSwapchain(const VulkanSwapchain& other) = delete;
		VulkanSwapchain& operator=(const VulkanSwapchain& other) = delete;
		VulkanSwapchain(VulkanSwapchain&& other) = delete;
		VulkanSwapchain& operator=(VulkanSwapchain&& other) = delete;

		[[nodiscard]] VkSwapchainKHR getSwapchain() const;
        [[nodiscard]] VkSurfaceFormatKHR getFormat() const;
        [[nodiscard]] VkExtent2D getExtent() const;
		[[nodiscard]] VkImage getImage(uint32_t index) const;
        [[nodiscard]] VkImageView getView(uint32_t index) const;
		[[nodiscard]] VkSemaphore getSemaphore(uint32_t index) const;
		[[nodiscard]] const VulkanImage& getColorImage() const;
		[[nodiscard]] const VulkanImage& getDepthImage() const;
		[[nodiscard]] VkFormat getDepthFormat() const;

		uint32_t acquireImage(VkSemaphore presentCompleteSempahore);
		void recreateSwapchain();

	  private:
	  	SDL_Window* m_WindowHandle = nullptr;
        VulkanDevice* m_Device = nullptr; // NON-OWNING
		VkSurfaceFormatKHR m_SelectedFormat;
		VkExtent2D m_Extent;
		VkFormat m_DepthFormat;

		VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
		std::vector<VkImage> m_Images;
		std::vector<VkImageView> m_ImageViews;
		std::vector<VkSemaphore> m_RenderFinishedSemaphores;        
		VulkanImage m_ColorImage;
		VulkanImage m_DepthImage;

        void createSwapchain(VkSwapchainKHR oldSwapchain);
        [[nodiscard]] static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
		[[nodiscard]] static VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
		[[nodiscard]] VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
		[[nodiscard]] static uint32_t chooseSwapMinImageCount(const VkSurfaceCapabilitiesKHR& surfaceCapabilities);
		void createImageViews();
		void createRenderFinishedSemaphores();
		void createRenderTargets();
		VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
		void cleanupSwapchain();
	};
} // namespace lab::vk
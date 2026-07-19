#pragma once

#include <vector>

#include <vulkan/vulkan.h>

#include "VulkanDevice.h"

namespace lab::vk {

	class VulkanSwapchain {
	  public:
		VulkanSwapchain(const VulkanDevice& device, SDL_Window* window);
		~VulkanSwapchain();
        VulkanSwapchain(const VulkanSwapchain& other) = delete;
		VulkanSwapchain& operator=(const VulkanSwapchain& other) = delete;
		VulkanSwapchain(VulkanSwapchain&& other) = delete;
		VulkanSwapchain& operator=(VulkanSwapchain&& other) = delete;

        [[nodiscard]] VkSurfaceFormatKHR getFormat() const;
        [[nodiscard]] VkExtent2D getExtent() const;
        [[nodiscard]] VkImageView getView(uint32_t index) const;

	  private:
		VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
		std::vector<VkImage> m_Images;
		VkSurfaceFormatKHR m_SelectedFormat;
		VkExtent2D m_Extent;
		std::vector<VkImageView> m_ImageViews;
        
        SDL_Window* m_WindowHandle = nullptr;
        VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE; // NON-OWNING
        VkDevice m_Device = VK_NULL_HANDLE; // NON-OWNING
        VkSurfaceKHR m_Surface = VK_NULL_HANDLE; //NON-OWNING
        uint32_t m_QueueFamilyIndex = 0;
        

        void createSwapchain(VkSwapchainKHR oldSwapchain);
        [[nodiscard]] static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
		[[nodiscard]] static VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
		[[nodiscard]] VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
		[[nodiscard]] static uint32_t chooseSwapMinImageCount(const VkSurfaceCapabilitiesKHR& surfaceCapabilities);
		void createImageViews();
	};
} // namespace lab::vk
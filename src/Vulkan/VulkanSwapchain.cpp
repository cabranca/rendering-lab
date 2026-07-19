#include "VulkanSwapchain.h"

#include "Logger.h"
#include "Utils.h"

namespace lab::vk {

	VulkanSwapchain::VulkanSwapchain(const VulkanDevice& device, SDL_Window* window)
	    : m_WindowHandle(window), m_PhysicalDevice(device.getPhysicalDevice()), m_Device(device.getDevice()),
	      m_Surface(device.getSurface()), m_QueueFamilyIndex(device.getQueueFamilyIndex()) {
		createSwapchain(VK_NULL_HANDLE);
		createImageViews();
	}

	VulkanSwapchain::~VulkanSwapchain() {
        for (const auto& view: m_ImageViews) {
			vkDestroyImageView(m_Device, view, nullptr);
		}
		m_ImageViews.clear();

		if (m_Swapchain != VK_NULL_HANDLE) {
			vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
			CBK_DEBUG("Vulkan Swapchain destroyed");
		}
    }

	VkSurfaceFormatKHR VulkanSwapchain::getFormat() const {
		return m_SelectedFormat;
	}

	VkExtent2D VulkanSwapchain::getExtent() const {
		return m_Extent;
	}

	VkImageView VulkanSwapchain::getView(uint32_t index) const {
		return m_ImageViews[index];
	}

    void VulkanSwapchain::createSwapchain(VkSwapchainKHR oldSwapchain) {
		VkSurfaceCapabilitiesKHR surfaceCaps;
		vkCheck(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_PhysicalDevice, m_Surface, &surfaceCaps),
		        "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

		uint32_t surfaceFormatsCount = 0;
		vkCheck(vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &surfaceFormatsCount, nullptr),
		        "vkGetPhysicalDeviceSurfaceFormatsKHR");
		std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatsCount);
		vkCheck(vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &surfaceFormatsCount, surfaceFormats.data()),
		        "vkGetPhysicalDeviceSurfaceFormatsKHR");

		uint32_t presentModesCount = 0;
		vkCheck(vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice, m_Surface, &presentModesCount, nullptr),
		        "vkGetPhysicalDeviceSurfacePresentModesKHR");
		std::vector<VkPresentModeKHR> presentModes(presentModesCount);
		vkCheck(vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice, m_Surface, &presentModesCount, presentModes.data()),
		        "vkGetPhysicalDeviceSurfacePresentModesKHR");

		m_SelectedFormat = chooseSwapSurfaceFormat(surfaceFormats);
		const auto selectedPresentMode = chooseSwapPresentMode(presentModes);
		m_Extent = chooseSwapExtent(surfaceCaps);
		const auto minImageCount = chooseSwapMinImageCount(surfaceCaps);

		VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		VkSwapchainCreateInfoKHR swapchainCI{ .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			                                  .pNext = nullptr,
			                                  .flags = 0,
			                                  .surface = m_Surface,
			                                  .minImageCount = minImageCount,
			                                  .imageFormat = m_SelectedFormat.format,
			                                  .imageColorSpace = m_SelectedFormat.colorSpace,
			                                  .imageExtent = m_Extent,
			                                  .imageArrayLayers = 1,
			                                  .imageUsage = usage,
			                                  .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
			                                  .queueFamilyIndexCount = 1,
			                                  .pQueueFamilyIndices = &m_QueueFamilyIndex,
			                                  .preTransform = surfaceCaps.currentTransform,
			                                  .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			                                  .presentMode = selectedPresentMode,
			                                  .clipped = VK_TRUE,
			                                  .oldSwapchain = oldSwapchain };
		vkCheck(vkCreateSwapchainKHR(m_Device, &swapchainCI, nullptr, &m_Swapchain), "vkCreateSwapchainKHR");

		uint32_t imageCount = 0;
		vkCheck(vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, nullptr), "vkGetSwapchainImagesKHR");
		m_Images.resize(imageCount);
		vkCheck(vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, m_Images.data()), "vkGetSwapchainImagesKHR");
		CBK_DEBUG("The number of Swapchain images is {}", imageCount);

		CBK_DEBUG("Vulkan Swapchain created");
	}

	VkSurfaceFormatKHR VulkanSwapchain::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
		VkSurfaceFormatKHR res = availableFormats[0];
		for (const auto& format: availableFormats) {
			if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
				res = format;
		}
		CBK_DEBUG("Selected surface format is {}", getSurfaceFormatStr(res));
		return res;
	}

	VkPresentModeKHR VulkanSwapchain::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
		VkPresentModeKHR res = VK_PRESENT_MODE_FIFO_KHR;
		for (const auto& mode: availablePresentModes) {
			if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
				res = mode;
		}
		CBK_DEBUG("Selected present mode is {}", getPresentModeStr(res));
		return res;
	}

	VkExtent2D VulkanSwapchain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
			return capabilities.currentExtent;
		int width, height;
		SDL_GetWindowSizeInPixels(m_WindowHandle, &width, &height);
		return { std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
			     std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height) };
	}

	uint32_t VulkanSwapchain::chooseSwapMinImageCount(const VkSurfaceCapabilitiesKHR& surfaceCapabilities) {
		auto minImageCount = std::max(3U, surfaceCapabilities.minImageCount);
		if ((0 < surfaceCapabilities.maxImageCount) && (surfaceCapabilities.maxImageCount < minImageCount)) {
			minImageCount = surfaceCapabilities.maxImageCount;
		}
		return minImageCount;
	}

	void VulkanSwapchain::createImageViews() {
		m_ImageViews.resize(m_Images.size());
		for (size_t i = 0; i < m_Images.size(); i++) {
            VkImageViewCreateInfo viewCI{ .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			                          .pNext = nullptr,
			                          .flags = 0,
			                          .image = m_Images[i],
			                          .viewType = VK_IMAGE_VIEW_TYPE_2D,
			                          .format = m_SelectedFormat.format,
			                          .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY, .g = VK_COMPONENT_SWIZZLE_IDENTITY, .b = VK_COMPONENT_SWIZZLE_IDENTITY, .a = VK_COMPONENT_SWIZZLE_IDENTITY, },
			                          .subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			                                                .baseMipLevel = 0,
			                                                .levelCount = 1,
			                                                .baseArrayLayer = 0,
			                                                .layerCount = 1 } };
		    vkCheck(vkCreateImageView(m_Device, &viewCI, nullptr, &m_ImageViews[i]), "vkCreateImageView");
		}
	}
} // namespace lab::vk
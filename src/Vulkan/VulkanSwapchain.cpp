#include "VulkanSwapchain.h"

#include "Logger.h"
#include "Utils.h"

namespace lab::vk {

	VulkanSwapchain::VulkanSwapchain(VulkanDevice* device, SDL_Window* window)
	    : m_WindowHandle(window), m_Device(device), m_SelectedFormat(), m_Extent(),
	      m_DepthFormat() {
		createSwapchain(VK_NULL_HANDLE);
		createImageViews();
		createRenderFinishedSemaphores();
		createRenderTargets();
	}

	VulkanSwapchain::~VulkanSwapchain() {
		for (const auto& semaphore : m_RenderFinishedSemaphores) {
			vkDestroySemaphore(m_Device->getDevice(), semaphore, nullptr);
		}

		for (const auto& view: m_ImageViews) {
			vkDestroyImageView(m_Device->getDevice(), view, nullptr);
		}
		m_ImageViews.clear();

		if (m_Swapchain != VK_NULL_HANDLE) {
			vkDestroySwapchainKHR(m_Device->getDevice(), m_Swapchain, nullptr);
			CBK_DEBUG("Vulkan Swapchain destroyed");
		}
	}

	VkSwapchainKHR VulkanSwapchain::getSwapchain() const {
		return m_Swapchain;
	}

	VkSurfaceFormatKHR VulkanSwapchain::getFormat() const {
		return m_SelectedFormat;
	}

	VkExtent2D VulkanSwapchain::getExtent() const {
		return m_Extent;
	}

	VkImage VulkanSwapchain::getImage(uint32_t index) const {
		return m_Images[index];
	}

	VkImageView VulkanSwapchain::getView(uint32_t index) const {
		return m_ImageViews[index];
	}

	VkSemaphore VulkanSwapchain::getSemaphore(uint32_t index) const {
		return m_RenderFinishedSemaphores[index];
	}

	const VulkanImage& VulkanSwapchain::getColorImage() const {
		return m_ColorImage;
	}

	const VulkanImage& VulkanSwapchain::getDepthImage() const {
		return m_DepthImage;
	}

	VkFormat VulkanSwapchain::getDepthFormat() const {
		return m_DepthFormat;
	}

	uint32_t VulkanSwapchain::acquireImage(VkSemaphore presentCompleteSempahore) {
		uint32_t imageIndex = 0;
		VkResult result = vkAcquireNextImageKHR(m_Device->getDevice(), m_Swapchain, UINT64_MAX, presentCompleteSempahore, nullptr, &imageIndex);
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			recreateSwapchain();
			UINT32_MAX; // What about a recursive call? Can it cause stack overflow?
		}
		// VK_SUBOPTIMAL_KHR still acquired an image and signalled the semaphore, so it has to be drawn and
		// presented; the present below reports it again and recreates then.
		if (result != VK_SUBOPTIMAL_KHR) {
			vkCheck(result, "vkAcquireNextImageKHR");
		}
		return imageIndex;
	}

	void VulkanSwapchain::createSwapchain(VkSwapchainKHR oldSwapchain) {
		VkSurfaceCapabilitiesKHR surfaceCaps;
		vkCheck(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_Device->getPhysicalDevice(), m_Device->getSurface(), &surfaceCaps),
		        "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

		uint32_t surfaceFormatsCount = 0;
		vkCheck(vkGetPhysicalDeviceSurfaceFormatsKHR(m_Device->getPhysicalDevice(), m_Device->getSurface(), &surfaceFormatsCount, nullptr),
		        "vkGetPhysicalDeviceSurfaceFormatsKHR");
		std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatsCount);
		vkCheck(vkGetPhysicalDeviceSurfaceFormatsKHR(m_Device->getPhysicalDevice(), m_Device->getSurface(), &surfaceFormatsCount, surfaceFormats.data()),
		        "vkGetPhysicalDeviceSurfaceFormatsKHR");

		uint32_t presentModesCount = 0;
		vkCheck(vkGetPhysicalDeviceSurfacePresentModesKHR(m_Device->getPhysicalDevice(), m_Device->getSurface(), &presentModesCount, nullptr),
		        "vkGetPhysicalDeviceSurfacePresentModesKHR");
		std::vector<VkPresentModeKHR> presentModes(presentModesCount);
		vkCheck(vkGetPhysicalDeviceSurfacePresentModesKHR(m_Device->getPhysicalDevice(), m_Device->getSurface(), &presentModesCount, presentModes.data()),
		        "vkGetPhysicalDeviceSurfacePresentModesKHR");

		m_SelectedFormat = chooseSwapSurfaceFormat(surfaceFormats);
		const auto selectedPresentMode = chooseSwapPresentMode(presentModes);
		m_Extent = chooseSwapExtent(surfaceCaps);
		const auto minImageCount = chooseSwapMinImageCount(surfaceCaps);

		VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		auto queueFamilyIndex = m_Device->getQueue().getQueueFamilyIndex();

		VkSwapchainCreateInfoKHR swapchainCI{ .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			                                  .pNext = nullptr,
			                                  .flags = 0,
			                                  .surface = m_Device->getSurface(),
			                                  .minImageCount = minImageCount,
			                                  .imageFormat = m_SelectedFormat.format,
			                                  .imageColorSpace = m_SelectedFormat.colorSpace,
			                                  .imageExtent = m_Extent,
			                                  .imageArrayLayers = 1,
			                                  .imageUsage = usage,
			                                  .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
			                                  .queueFamilyIndexCount = 1,
			                                  .pQueueFamilyIndices = &queueFamilyIndex,
			                                  .preTransform = surfaceCaps.currentTransform,
			                                  .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			                                  .presentMode = selectedPresentMode,
			                                  .clipped = VK_TRUE,
			                                  .oldSwapchain = oldSwapchain };
		vkCheck(vkCreateSwapchainKHR(m_Device->getDevice(), &swapchainCI, nullptr, &m_Swapchain), "vkCreateSwapchainKHR");

		uint32_t imageCount = 0;
		vkCheck(vkGetSwapchainImagesKHR(m_Device->getDevice(), m_Swapchain, &imageCount, nullptr), "vkGetSwapchainImagesKHR");
		m_Images.resize(imageCount);
		vkCheck(vkGetSwapchainImagesKHR(m_Device->getDevice(), m_Swapchain, &imageCount, m_Images.data()), "vkGetSwapchainImagesKHR");
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
			vkCheck(vkCreateImageView(m_Device->getDevice(), &viewCI, nullptr, &m_ImageViews[i]), "vkCreateImageView");
		}
	}

	void VulkanSwapchain::createRenderFinishedSemaphores() {
		for (const auto& semaphore: m_RenderFinishedSemaphores) {
			vkDestroySemaphore(m_Device->getDevice(), semaphore, nullptr);
		}
		m_RenderFinishedSemaphores.assign(m_Images.size(), VK_NULL_HANDLE);

		VkSemaphoreCreateInfo semaphoreCI{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = nullptr, .flags = 0 };

		for (auto& semaphore: m_RenderFinishedSemaphores) {
			vkCheck(vkCreateSemaphore(m_Device->getDevice(), &semaphoreCI, nullptr, &semaphore), "vkCreateSemaphore");
		}
	}

	void VulkanSwapchain::createRenderTargets() {
		m_ColorImage = VulkanImage(m_Device, m_Extent.width, m_Extent.height, 1, true, m_SelectedFormat.format, VK_IMAGE_TILING_OPTIMAL,
		                           VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
		m_DepthFormat = findSupportedFormat({ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
		                                    VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_2_DEPTH_STENCIL_ATTACHMENT_BIT);
		m_DepthImage = VulkanImage(m_Device, m_Extent.width, m_Extent.height, 1, false, m_DepthFormat, VK_IMAGE_TILING_OPTIMAL,
		                           VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);
	}

	VkFormat VulkanSwapchain::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling,
	                                              VkFormatFeatureFlags features) {
		for (const auto& format: candidates) {
			VkFormatProperties2 props{ .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2 };
			vkGetPhysicalDeviceFormatProperties2(m_Device->getPhysicalDevice(), format, &props);
			if (((tiling == VK_IMAGE_TILING_LINEAR) && ((props.formatProperties.linearTilingFeatures & features) == features)) ||
			    ((tiling == VK_IMAGE_TILING_OPTIMAL) && ((props.formatProperties.optimalTilingFeatures & features) == features)))
				return format;
		}
		CBK_FATAL("Failed to find supported format!");
		return VK_FORMAT_MAX_ENUM;
	}

	void VulkanSwapchain::recreateSwapchain() {
		m_Device->waitIdle();
		cleanupSwapchain();
		createImageViews();
		createRenderFinishedSemaphores();
		createRenderTargets();
	}

	void VulkanSwapchain::cleanupSwapchain() {
		for (const auto& view: m_ImageViews) {
			vkDestroyImageView(m_Device->getDevice(), view, nullptr);
		}
		m_ImageViews.clear();
		VkSwapchainKHR oldSwapchain = m_Swapchain;
		createSwapchain(oldSwapchain);
		vkDestroySwapchainKHR(m_Device->getDevice(), oldSwapchain, nullptr);
	}
} // namespace lab::vk
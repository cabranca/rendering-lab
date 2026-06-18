#include "Swapchain.h"

#include <cassert>
#include <print>

#include <volk/volk.h>

#include "Check.h"

namespace lab {

	void Swapchain::init(VkDevice device, const PhysicalDevice& physicalDevice, VkSurfaceKHR surface, uint32_t queueFamilyIndex) {
		uint32_t numImages = chooseNumImages(physicalDevice.SurfaceCaps);
		VkPresentModeKHR presentMode = choosePresentMode(physicalDevice.PresentModes);
		m_SurfaceFormat = chooseSurfaceFormatAndColorSpace(physicalDevice.SurfaceFormats);
		m_Extent = physicalDevice.SurfaceCaps.currentExtent;

		VkSwapchainCreateInfoKHR swapchainCI{ .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			                                  .surface = surface,
			                                  .minImageCount = numImages,
			                                  .imageFormat = m_SurfaceFormat.format,
			                                  .imageColorSpace = m_SurfaceFormat.colorSpace,
			                                  .imageExtent = m_Extent,
			                                  .imageArrayLayers = 1,
			                                  .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			                                  .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
			                                  .queueFamilyIndexCount = 1,
			                                  .pQueueFamilyIndices = &queueFamilyIndex,
			                                  .preTransform = physicalDevice.SurfaceCaps.currentTransform,
			                                  .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			                                  .presentMode = presentMode,
			                                  .clipped = VK_TRUE };

		chk(vkCreateSwapchainKHR(device, &swapchainCI, nullptr, &m_Swapchain));
		std::println("Swapchain created");

		uint32_t numSwapchainImages = 0;
		chk(vkGetSwapchainImagesKHR(device, m_Swapchain, &numSwapchainImages, nullptr));
		assert(numImages == numSwapchainImages);
		std::println("Number of images: {}", numSwapchainImages);

		m_Images.resize(numImages);
		m_ImageViews.resize(numImages);
		chk(vkGetSwapchainImagesKHR(device, m_Swapchain, &numSwapchainImages, m_Images.data()));

		int layerCount = 1;
		int mipLevels = 1;
		for (uint32_t i = 0; i < numSwapchainImages; i++)
			m_ImageViews.at(i) = createImageView(device, m_Images.at(i), m_SurfaceFormat.format, VK_IMAGE_ASPECT_COLOR_BIT,
			                                     VK_IMAGE_VIEW_TYPE_2D, layerCount, mipLevels);
	}

	void Swapchain::shutdown(VkDevice device) {
        for (size_t i = 0; i < m_ImageViews.size(); i++)
            vkDestroyImageView(device, m_ImageViews.at(i), nullptr);
		vkDestroySwapchainKHR(device, m_Swapchain, nullptr);
		std::println("Swapchain destroyed");
	}

	uint32_t Swapchain::getNumImages() const {
		return m_Images.size();
	}

	VkImage Swapchain::getImage(uint32_t index) const {
		return m_Images.at(index);
	}

	VkImageView Swapchain::getImageView(uint32_t index) const {
		return m_ImageViews.at(index);
	}

	VkSwapchainKHR Swapchain::getSwapchain() const {
		return m_Swapchain;
	}

	VkSurfaceFormatKHR Swapchain::getSurfaceFormat() const {
		return m_SurfaceFormat;
	}

	VkExtent2D Swapchain::getExtent() const {
		return m_Extent;
	}

	VkPresentModeKHR Swapchain::choosePresentMode(const std::vector<VkPresentModeKHR>& presentModes) {
		for (const auto& mode: presentModes) {
			if (mode == VK_PRESENT_MODE_FIFO_KHR)
				return mode;
		}
		std::println("FIFO present mode not found in surface! Shutting down program");
		exit(1);
	}

	uint32_t Swapchain::chooseNumImages(const VkSurfaceCapabilitiesKHR& surfaceCaps) {
		auto requestedNumImages = surfaceCaps.minImageCount + 1;
		int finalNumImages{ 0 };
		if (surfaceCaps.maxImageCount > 0 && requestedNumImages > surfaceCaps.maxImageCount)
			finalNumImages = surfaceCaps.maxImageCount;
		else
			finalNumImages = finalNumImages = requestedNumImages;

		return finalNumImages;
	}

	VkSurfaceFormatKHR Swapchain::chooseSurfaceFormatAndColorSpace(const std::vector<VkSurfaceFormatKHR>& surfaceFormat) {
		for (const auto& format: surfaceFormat) {
			if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
				return format;
		}
		std::println("SRGBA format and color sapces not found in surface! Shutting down program");
		exit(1);
	}

	VkImageView Swapchain::createImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags flags,
	                                       VkImageViewType viewType, uint32_t layerCount, uint32_t mipLevels) {
		VkImageViewCreateInfo viewCI{ .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			                          .flags = 0,
			                          .image = image,
			                          .viewType = viewType,
			                          .format = format,
			                          .components = { .r = VK_COMPONENT_SWIZZLE_IDENTITY,
			                                          .g = VK_COMPONENT_SWIZZLE_IDENTITY,
			                                          .b = VK_COMPONENT_SWIZZLE_IDENTITY,
			                                          .a = VK_COMPONENT_SWIZZLE_IDENTITY },
			                          .subresourceRange = { .aspectMask = flags,
			                                                .baseMipLevel = 0,
			                                                .levelCount = mipLevels,
			                                                .baseArrayLayer = 0,
			                                                .layerCount = layerCount } };
        
        VkImageView res;
        chk(vkCreateImageView(device, &viewCI, nullptr, &res));
        return res;
	}   
} // namespace lab

#include "Swapchain.h"

#include <cassert>

#include <volk/volk.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include "Check.h"
#include "VulkanContext.h"
#include "Window.h"

namespace lab {

	void Swapchain::init(const VulkanContext& ctx, const Window& window) {
		chk(SDL_Vulkan_CreateSurface(window.getHandle(), ctx.getInstance(), nullptr, &m_Surface));

		// Pick a supported depth/stencil format once; it stays fixed for the swapchain's life.
		std::vector<VkFormat> depthFormatList{ VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
		for (VkFormat format: depthFormatList) {
			VkFormatProperties2 formatProperties{ .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2 };
			vkGetPhysicalDeviceFormatProperties2(ctx.getPhysicalDevice(), format, &formatProperties);
			if (formatProperties.formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
				m_DepthFormat = format;
				break;
			}
		}
		assert(m_DepthFormat != VK_FORMAT_UNDEFINED);

		createSwapchain(ctx, window, VK_NULL_HANDLE);
		createDepth(ctx);
	}

	void Swapchain::createSwapchain(const VulkanContext& ctx, const Window& window, VkSwapchainKHR oldSwapchain) {
		auto device = ctx.getDevice();

		m_WindowSize = window.getSize();
		VkSurfaceCapabilitiesKHR surfaceCaps{};
		chk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx.getPhysicalDevice(), m_Surface, &surfaceCaps));
		VkExtent2D swapchainExtent{ surfaceCaps.currentExtent };
		if (surfaceCaps.currentExtent.width == 0xFFFFFFFF) {
			swapchainExtent = { .width = static_cast<uint32_t>(m_WindowSize.x), .height = static_cast<uint32_t>(m_WindowSize.y) };
		}

		VkSwapchainCreateInfoKHR swapchainCI{
			.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.surface = m_Surface,
			.minImageCount = surfaceCaps.minImageCount,
			.imageFormat = m_ImageFormat,
			.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
			.imageExtent{ .width = swapchainExtent.width, .height = swapchainExtent.height },
			.imageArrayLayers = 1,
			.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
			.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			.presentMode = VK_PRESENT_MODE_FIFO_KHR,
			.oldSwapchain = oldSwapchain,
		};
		chk(vkCreateSwapchainKHR(device, &swapchainCI, nullptr, &m_Swapchain));

		uint32_t imageCount{ 0 };
		chk(vkGetSwapchainImagesKHR(device, m_Swapchain, &imageCount, nullptr));
		m_SwapchainImages.resize(imageCount);
		chk(vkGetSwapchainImagesKHR(device, m_Swapchain, &imageCount, m_SwapchainImages.data()));
		m_SwapchainImageViews.resize(imageCount);
		for (uint32_t i = 0; i < imageCount; i++) {
			VkImageViewCreateInfo viewCI{ .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				                          .image = m_SwapchainImages[i],
				                          .viewType = VK_IMAGE_VIEW_TYPE_2D,
				                          .format = m_ImageFormat,
				                          .subresourceRange{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 } };
			chk(vkCreateImageView(device, &viewCI, nullptr, &m_SwapchainImageViews[i]));
		}
	}

	void Swapchain::createDepth(const VulkanContext& ctx) {
		VkImageCreateInfo depthImageCI{
			.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			.imageType = VK_IMAGE_TYPE_2D,
			.format = m_DepthFormat,
			.extent{ .width = static_cast<uint32_t>(m_WindowSize.x), .height = static_cast<uint32_t>(m_WindowSize.y), .depth = 1 },
			.mipLevels = 1,
			.arrayLayers = 1,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.tiling = VK_IMAGE_TILING_OPTIMAL,
			.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		};
		VmaAllocationCreateInfo allocCI{ .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, .usage = VMA_MEMORY_USAGE_AUTO };
		chk(vmaCreateImage(ctx.getAllocator(), &depthImageCI, &allocCI, &m_DepthImage, &m_DepthImageAllocation, nullptr));
		VkImageViewCreateInfo depthViewCI{ .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			                               .image = m_DepthImage,
			                               .viewType = VK_IMAGE_VIEW_TYPE_2D,
			                               .format = m_DepthFormat,
			                               .subresourceRange{ .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .levelCount = 1, .layerCount = 1 } };
		chk(vkCreateImageView(ctx.getDevice(), &depthViewCI, nullptr, &m_DepthImageView));
	}

	void Swapchain::destroySwapchainResources(const VulkanContext& ctx) {
		auto device = ctx.getDevice();
		vmaDestroyImage(ctx.getAllocator(), m_DepthImage, m_DepthImageAllocation);
		vkDestroyImageView(device, m_DepthImageView, nullptr);
		for (auto view: m_SwapchainImageViews)
			vkDestroyImageView(device, view, nullptr);
		m_SwapchainImageViews.clear();
	}

	void Swapchain::recreate(const VulkanContext& ctx, const Window& window) {
		VkSwapchainKHR oldSwapchain = m_Swapchain;
		destroySwapchainResources(ctx);
		createSwapchain(ctx, window, oldSwapchain);
		vkDestroySwapchainKHR(ctx.getDevice(), oldSwapchain, nullptr);
		createDepth(ctx);
	}

	void Swapchain::shutdown(const VulkanContext& ctx) {
		destroySwapchainResources(ctx);
		vkDestroySwapchainKHR(ctx.getDevice(), m_Swapchain, nullptr);
		vkDestroySurfaceKHR(ctx.getInstance(), m_Surface, nullptr);
	}
} // namespace lab

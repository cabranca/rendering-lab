#include "Swapchain.h"

#include <iostream>

#include <volk/volk.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include "VulkanContext.h"

namespace lab {

    static inline void chk(VkResult result) {
        if (result != VK_SUCCESS) {
            std::cerr << "Vulkan call returned an error (" << result << ")\n";
            throw std::exception();
        }
    }

    static inline void chk(bool result) {
        if (!result) {
            std::cerr << "Call returned an error\n";
            exit(result);
        }
    }

    void Swapchain::init(const VulkanContext& ctx) {
        auto instance = ctx.getInstance();
        auto device = ctx.getDevice();
        auto physicalDevice = ctx.getPhysicalDevice();
        auto allocator = ctx.getAllocator();

        // Window and m_Surface
        SDL_Window* window = SDL_CreateWindow("How to Vulkan", 1600u, 1900u, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
        chk(SDL_Vulkan_CreateSurface(window, instance, nullptr, &m_Surface));
        chk(SDL_GetWindowSize(window, &windowSize.x, &windowSize.y));
        VkSurfaceCapabilitiesKHR surfaceCaps{};
        chk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, m_Surface, &surfaceCaps));
        VkExtent2D swapchainExtent{ surfaceCaps.currentExtent };
        if (surfaceCaps.currentExtent.width == 0xFFFFFFFF) {
            swapchainExtent = { .width = static_cast<uint32_t>(windowSize.x), .height = static_cast<uint32_t>(windowSize.y) };
        }


        // Swap chain
        const VkFormat imageFormat{ VK_FORMAT_B8G8R8A8_SRGB };
        VkSwapchainCreateInfoKHR swapchainCI{
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface = m_Surface,
            .minImageCount = surfaceCaps.minImageCount,
            .imageFormat = imageFormat,
            .imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
            .imageExtent{.width = swapchainExtent.width, .height = swapchainExtent.height },
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode = VK_PRESENT_MODE_FIFO_KHR
        };
        chk(vkCreateSwapchainKHR(device, &swapchainCI, nullptr, &m_Swapchain));
        uint32_t imageCount{ 0 };
        chk(vkGetSwapchainImagesKHR(device, m_Swapchain, &imageCount, nullptr));
        m_SwapchainImages.resize(imageCount);
        chk(vkGetSwapchainImagesKHR(device, m_Swapchain, &imageCount, m_SwapchainImages.data()));
        m_SwapchainImageViews.resize(imageCount);
        for (auto i = 0; i < imageCount; i++) {
            VkImageViewCreateInfo viewCI{ .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = m_SwapchainImages[i], .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = imageFormat, .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 } };
            chk(vkCreateImageView(device, &viewCI, nullptr, &m_SwapchainImageViews[i]));
        }


        // Depth attachment
        std::vector<VkFormat> depthFormatList{ VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
        VkFormat depthFormat{ VK_FORMAT_UNDEFINED };
        for (VkFormat& format : depthFormatList) {
            VkFormatProperties2 formatProperties{ .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2 };
            vkGetPhysicalDeviceFormatProperties2(physicalDevice, format, &formatProperties);
            if (formatProperties.formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
                depthFormat = format;
                break;
            }
        }
        assert(depthFormat != VK_FORMAT_UNDEFINED);
        VkImageCreateInfo depthImageCI{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = depthFormat,
            .extent{.width = static_cast<uint32_t>(windowSize.x), .height = static_cast<uint32_t>(windowSize.y), .depth = 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };
        VmaAllocationCreateInfo allocCI{ .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, .usage = VMA_MEMORY_USAGE_AUTO };
        chk(vmaCreateImage(allocator, &depthImageCI, &allocCI, &m_DepthImage, &m_DepthImageAllocation, nullptr));
        VkImageViewCreateInfo depthViewCI{ .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = m_DepthImage, .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = depthFormat, .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .levelCount = 1, .layerCount = 1 } };
        chk(vkCreateImageView(device, &depthViewCI, nullptr, &m_DepthImageView));
        
    }

    void Swapchain::shutdown(const VulkanContext& ctx) {
        auto instance = ctx.getInstance();
        auto device = ctx.getDevice();
        auto allocator = ctx.getAllocator();

        vmaDestroyImage(allocator, m_DepthImage, m_DepthImageAllocation);
        vkDestroyImageView(device, m_DepthImageView, nullptr);
        for (auto i = 0; i < m_SwapchainImageViews.size(); i++)
            vkDestroyImageView(device, m_SwapchainImageViews[i], nullptr);
        vkDestroySwapchainKHR(device, m_Swapchain, nullptr);
        vkDestroySurfaceKHR(instance, m_Surface, nullptr);
    }
}
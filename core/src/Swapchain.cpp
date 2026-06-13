#include "Swapchain.h"

#include <print>
#include <vulkan/vulkan_core.h>

namespace lab {

    void Swapchain::init(VkInstance instance, VkSurfaceKHR surface) {
        
        
        VkSwapchainCreateInfoKHR swapchainCI {
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface = surface,
            .minImageCount = 
        };
    }

    void Swapchain::shutdown() {

    }

    VkPresentModeKHR Swapchain::choosePresentMode(const std::vector<VkPresentModeKHR>& presentModes) {
        for (const auto& mode : presentModes) {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
                return mode;
        }
        std::println("Mailbox present mode not found in surface! Shutting down program");
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

    VkSurfaceFormatKHR Swapchain::chooseSurfaceFormatAndColorSpace(const std::vector<VkSurfaceFormatKHR>& surfaceFormats) {
        for (const auto& format : surfaceFormats) {
            if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                return format;
        }
        std::println("SRGBA format and color sapces not found in surface! Shutting down program");
        exit(1);
    }
}

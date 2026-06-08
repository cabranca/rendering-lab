#pragma once

#include <vector>
#include <glm/glm.hpp>

#include "vulkan/vulkan_core.h"
#include "vma/vk_mem_alloc.h"

namespace lab {

    class VulkanContext;
    class Window;

    class Swapchain {
        public:
        void init(const VulkanContext& ctx, const Window& window);
        // Rebuilds the swapchain and depth buffer at the window's current size.
        // The caller must vkDeviceWaitIdle first. Render-complete semaphores are
        // owned by VulkanCore and recreated there (image count may change).
        void recreate(const VulkanContext& ctx, const Window& window);
        void shutdown(const VulkanContext& ctx);

        VkSwapchainKHR getSwapchain() const { return m_Swapchain; }
        const std::vector<VkImage>& getImages() const { return m_SwapchainImages; }
        const std::vector<VkImageView>& getImageViews() const { return m_SwapchainImageViews; }
        uint32_t getImageCount() const { return static_cast<uint32_t>(m_SwapchainImages.size()); }
        VkFormat getImageFormat() const { return m_ImageFormat; }
        VkImage getDepthImage() const { return m_DepthImage; }
        VkImageView getDepthImageView() const { return m_DepthImageView; }
        VkFormat getDepthFormat() const { return m_DepthFormat; }
        VkExtent2D getExtent() const { return { static_cast<uint32_t>(m_WindowSize.x), static_cast<uint32_t>(m_WindowSize.y) }; }

        private:
        void createSwapchain(const VulkanContext& ctx, const Window& window, VkSwapchainKHR oldSwapchain);
        void createDepth(const VulkanContext& ctx);
        void destroySwapchainResources(const VulkanContext& ctx);

        VkSurfaceKHR m_Surface{ VK_NULL_HANDLE };
        VkSwapchainKHR m_Swapchain{ VK_NULL_HANDLE };
        VkFormat m_ImageFormat{ VK_FORMAT_B8G8R8A8_SRGB };
        std::vector<VkImage> m_SwapchainImages;
        std::vector<VkImageView> m_SwapchainImageViews;
        VkImage m_DepthImage{ VK_NULL_HANDLE };
        VmaAllocation m_DepthImageAllocation{ VK_NULL_HANDLE };
        VkImageView m_DepthImageView{ VK_NULL_HANDLE };
        VkFormat m_DepthFormat{ VK_FORMAT_UNDEFINED };
        glm::ivec2 m_WindowSize{};
    };
}

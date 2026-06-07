
#pragma once

#include <vector>
#include <glm/glm.hpp>

#include "vulkan/vulkan_core.h"
#include "vma/vk_mem_alloc.h"

#include "VulkanContext.h"

namespace lab {

    class Swapchain {
        public:
        void init(const VulkanContext& ctx);
        void shutdown(const VulkanContext& ctx);

        private:
        VkSurfaceKHR m_Surface{ VK_NULL_HANDLE };
        VkSwapchainKHR m_Swapchain{ VK_NULL_HANDLE };
        VkImage m_DepthImage;
        VmaAllocation m_DepthImageAllocation;
        VkImageView m_DepthImageView;
        std::vector<VkImage> m_SwapchainImages;
        std::vector<VkImageView> m_SwapchainImageViews;
        glm::ivec2 windowSize{};
    };
}
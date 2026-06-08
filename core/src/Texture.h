#pragma once

#include <string_view>

#include "vulkan/vulkan_core.h"
#include "vma/vk_mem_alloc.h"

namespace lab {

    class VulkanContext;

    // A sampled 2D texture loaded from a KTX file (mip levels included) and
    // uploaded to device-local memory.
    class Texture {
        public:
        void init(const VulkanContext& ctx, std::string_view path);
        void shutdown(const VulkanContext& ctx);

        VkDescriptorImageInfo getDescriptorInfo() const {
            return { .sampler = m_Sampler, .imageView = m_View, .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL };
        }

        private:
        VmaAllocation m_Allocation{ VK_NULL_HANDLE };
        VkImage m_Image{ VK_NULL_HANDLE };
        VkImageView m_View{ VK_NULL_HANDLE };
        VkSampler m_Sampler{ VK_NULL_HANDLE };
    };
}

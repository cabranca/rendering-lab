#pragma once

#include <cstdint>
#include <functional>

#include "vulkan/vulkan_core.h"
#include "vma/vk_mem_alloc.h"

namespace lab {

    class VulkanContext {
        public:
        void init(std::string_view applicationName);
        void shutdown();

        // Allocates a one-time-submit command buffer, runs `record`, then submits
        // and waits for completion. Intended for load-time transfers/transitions.
        void submitImmediate(const std::function<void(VkCommandBuffer)>& record) const;

        VkInstance getInstance() const {return m_Instance;}
        VkDevice getDevice() const {return m_Device;}
        VkPhysicalDevice getPhysicalDevice() const {return m_PhysicalDevice;}
        VkQueue getQueue() const {return m_Queue;}
        uint32_t getQueueFamily() const {return m_QueueFamily;}
        VmaAllocator getAllocator() const {return m_Allocator;}

        private:
        VkInstance m_Instance{ VK_NULL_HANDLE };
        VkDevice m_Device{ VK_NULL_HANDLE };
        VkPhysicalDevice m_PhysicalDevice{ VK_NULL_HANDLE };
        VkQueue m_Queue{ VK_NULL_HANDLE };
        uint32_t m_QueueFamily{ 0 };
        VmaAllocator m_Allocator{ VK_NULL_HANDLE };
    };
}

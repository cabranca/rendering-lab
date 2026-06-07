#pragma once

#include "vulkan/vulkan_core.h"
#include "vma/vk_mem_alloc.h"

namespace lab {

    class VulkanContext {
        public:
        void init();
        void shutdown();

        VkInstance getInstance() const {return m_Instance;}
        VkDevice getDevice() const {return m_Device;}
        VkPhysicalDevice getPhysicalDevice() const {return m_PhysicalDevice;}
        VkQueue getQueue() const {return m_Queue;}
        VmaAllocator getAllocator() const {return m_Allocator;}

        private:
        VkInstance m_Instance;
        VkDevice m_Device;
        VkPhysicalDevice m_PhysicalDevice;
        VkQueue m_Queue;
        VmaAllocator m_Allocator;
    };
}
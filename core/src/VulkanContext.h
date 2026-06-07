#pragma once

#include "vulkan/vulkan_core.h"
#include "vma/vk_mem_alloc.h"

namespace lab {

    class VulkanContext {
        public:
        void init();
        void shutdown();

        private:
        VkInstance m_Instance;
        VkDevice m_Device;
        VkPhysicalDevice m_PhysicalDevice;
        VkQueue m_Queue;
        VmaAllocator m_Allocator;
    };
}
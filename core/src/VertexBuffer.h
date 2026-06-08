#pragma once

#include <span>

#include "vulkan/vulkan_core.h"
#include "vma/vk_mem_alloc.h"

#include "Common.h"

namespace lab {

    class VulkanContext;

    // A drawable mesh: vertices and indices packed into a single device buffer.
    // The index data starts right after the vertices, at byte getIndexOffset().
    class VertexBuffer {
        public:
        void init(const VulkanContext& ctx, std::span<Vertex> vertices, std::span<uint16_t> indices);
        void shutdown(const VulkanContext& ctx);

        VkBuffer getBuffer() const { return m_VBuffer; }
        VkDeviceSize getIndexOffset() const { return m_VBsize; }
        uint32_t getIndexCount() const { return m_IndexCount; }

        private:
        VmaAllocation m_VBufferAllocation{ VK_NULL_HANDLE };
        VkBuffer m_VBuffer{ VK_NULL_HANDLE };
        VkDeviceSize m_VBsize{ 0 };
        uint32_t m_IndexCount{ 0 };
    };
}

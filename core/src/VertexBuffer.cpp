#include "VertexBuffer.h"

#include <cstring>

#include <volk/volk.h>

#include "Check.h"
#include "VulkanContext.h"

namespace lab {

	void VertexBuffer::init(const VulkanContext& ctx, std::span<Vertex> vertices, std::span<uint16_t> indices) {
		auto allocator = ctx.getAllocator();

		m_IndexCount = static_cast<uint32_t>(indices.size());
		m_VBsize = sizeof(Vertex) * vertices.size();
		VkDeviceSize iBufSize{ sizeof(uint16_t) * indices.size() };
		VkBufferCreateInfo bufferCI{ .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			                         .size = m_VBsize + iBufSize,
			                         .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT };
		VmaAllocationCreateInfo vBufferAllocCI{ .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
			                                             VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
			                                             VMA_ALLOCATION_CREATE_MAPPED_BIT,
			                                    .usage = VMA_MEMORY_USAGE_AUTO };
		VmaAllocationInfo vBufferAllocInfo{};
		chk(vmaCreateBuffer(allocator, &bufferCI, &vBufferAllocCI, &m_VBuffer, &m_VBufferAllocation, &vBufferAllocInfo));
		memcpy(vBufferAllocInfo.pMappedData, vertices.data(), m_VBsize);
		memcpy(static_cast<char*>(vBufferAllocInfo.pMappedData) + m_VBsize, indices.data(), iBufSize);
	}

	void VertexBuffer::shutdown(const VulkanContext& ctx) {
		vmaDestroyBuffer(ctx.getAllocator(), m_VBuffer, m_VBufferAllocation);
	}
} // namespace lab

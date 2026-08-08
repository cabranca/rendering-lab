#include "VulkanMesh.h"

#include "VulkanCommands.h"
#include "VulkanQueue.h"

namespace lab::vk {

	VulkanMesh::VulkanMesh(const VulkanDevice& device, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
	    : m_IndexCount(static_cast<uint32_t>(indices.size())) {
		VkDeviceSize vertexBufferSize = sizeof(Vertex) * vertices.size();
		VulkanBuffer vertexStaging(device, vertexBufferSize, VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT,
		                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		vertexStaging.setData(vertices.data());
		m_VertexBuffer = VulkanBuffer(device, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT,
		                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		VkDeviceSize indexBufferSize = sizeof(indices[0]) * indices.size();
		VulkanBuffer indexStaging(device, indexBufferSize, VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT,
		                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		indexStaging.setData(indices.data());
		m_IndexBuffer = VulkanBuffer(device, indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT,
		                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		// One single-time submission copies both buffers; the staging buffers stay alive on the
		// stack until endSingleTimeCommands has waited for the transfer to complete.
		const auto& queue = device.getQueue();
		auto cmdBuffer = queue.beginSingleTimeCommands();
		VulkanCommands::copyBuffer(cmdBuffer, vertexStaging.getBuffer(), m_VertexBuffer.getBuffer(), vertexBufferSize);
		VulkanCommands::copyBuffer(cmdBuffer, indexStaging.getBuffer(), m_IndexBuffer.getBuffer(), indexBufferSize);
		queue.endSingleTimeCommands(cmdBuffer);
	}

	void VulkanMesh::bind(VkCommandBuffer cmdBuffer) const {
		VkDeviceSize offset = 0;
		VkBuffer vertexBuffer = m_VertexBuffer.getBuffer();
		vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &vertexBuffer, &offset);
		vkCmdBindIndexBuffer(cmdBuffer, m_IndexBuffer.getBuffer(), 0, VK_INDEX_TYPE_UINT32);
	}

	void VulkanMesh::draw(VkCommandBuffer cmdBuffer) const {
		vkCmdDrawIndexed(cmdBuffer, m_IndexCount, 1, 0, 0, 0);
	}
} // namespace lab::vk

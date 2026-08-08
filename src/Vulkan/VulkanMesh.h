#pragma once

#include <cstdint>
#include <vector>

#include <volk/volk.h>

#include "Vertex.h"
#include "VulkanBuffer.h"
#include "VulkanDevice.h"

namespace lab::vk {

	// Owns a device-local vertex/index buffer pair uploaded from CPU-side geometry. Move-only,
	// inherited from its VulkanBuffer members (copy is implicitly deleted, move implicitly defaulted).
	class VulkanMesh {
	  public:
		VulkanMesh() = default;
		VulkanMesh(const VulkanDevice& device, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);

		void bind(VkCommandBuffer cmdBuffer) const;
		void draw(VkCommandBuffer cmdBuffer) const;

	  private:
		VulkanBuffer m_VertexBuffer;
		VulkanBuffer m_IndexBuffer;
		uint32_t m_IndexCount = 0;
	};
} // namespace lab::vk

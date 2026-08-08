#pragma once

#include <string_view>
#include <vector>

#include <volk/volk.h>

#include "VulkanDevice.h"
#include "VulkanMesh.h"

namespace lab::vk {

	// A drawable model: an ordered collection of meshes (one per OBJ shape). Move-only via its
	// VulkanMesh members. Construct through the loadOBJ factory.
	class VulkanModel {
	  public:
		VulkanModel() = default;

		static VulkanModel loadOBJ(const VulkanDevice& device, std::string_view path);

		void draw(VkCommandBuffer cmdBuffer) const;

	  private:
		std::vector<VulkanMesh> m_Meshes;
	};
} // namespace lab::vk

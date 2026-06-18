#pragma once

#include <volk/volk.h>
#include "vulkan/vulkan_core.h"
#include <glm/glm.hpp>

namespace lab {

	struct BufferAndMemory {
		VkBuffer Buffer{ VK_NULL_HANDLE };
		VkDeviceMemory Mem{ VK_NULL_HANDLE };
		VkDeviceSize AllocationSize{ 0 };

		void destroy(VkDevice device) {
			vkDestroyBuffer(device, Buffer, nullptr);
		}
	};

	struct Vertex {
		glm::vec3 pos;
		glm::vec2 uv;
	};
} // namespace lab
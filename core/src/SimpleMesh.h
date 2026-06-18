#pragma once

#include <volk/volk.h>
#include "Common.h"
#include "vulkan/vulkan_core.h"
namespace lab {

	struct SimpleMesh {
		size_t VertexBufferSize{ 0 };
		BufferAndMemory VB;

		void destroy(VkDevice device) {
			VB.destroy(device);
		}
	};
} // namespace lab
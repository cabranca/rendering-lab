#pragma once

#include "vulkan/vulkan_core.h"

namespace lab {

	class GraphicsPipeline {
	  public:
		void init(VkDevice device, VkRenderPass renderPass, VkShaderModule vs, VkShaderModule fs);
		void shutdown();

		void bind(VkCommandBuffer cmdBuffer);

	  private:
		VkDevice m_Device{ VK_NULL_HANDLE };
		VkPipeline m_Pipeline{ VK_NULL_HANDLE };
		VkPipelineLayout m_PipelineLayout{ VK_NULL_HANDLE };
	};
} // namespace lab

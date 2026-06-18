#pragma once

#include "SimpleMesh.h"
#include "vulkan/vulkan_core.h"

namespace lab {

	class GraphicsPipeline {
	  public:
		void init(VkDevice device, VkRenderPass renderPass, VkShaderModule vs, VkShaderModule fs, const SimpleMesh* mesh,
		          uint32_t numImages);
		void shutdown();

		void bind(VkCommandBuffer cmdBuffer, int imageIndex);

	  private:
		VkDevice m_Device{ VK_NULL_HANDLE };
		VkPipeline m_Pipeline{ VK_NULL_HANDLE };
		VkPipelineLayout m_PipelineLayout{ VK_NULL_HANDLE };
		VkDescriptorSetLayout m_DescriptorSetLayout{ VK_NULL_HANDLE };
		VkDescriptorPool m_DescriptorPool{ VK_NULL_HANDLE };
		std::vector<VkDescriptorSet> m_DescriptorSets;

		void createDescriptorSets(uint32_t numImages, const SimpleMesh* mesh);
		void createDescriptorPool(uint32_t numImages);
		void createDescriptorSetLayout();
		void allocateDescriptorSets(uint32_t numImages);
		void updateDescriptorSets(uint32_t numImages, const SimpleMesh* mesh);
	};
} // namespace lab

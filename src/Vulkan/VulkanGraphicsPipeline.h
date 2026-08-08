#pragma once

#include <vector>

#include <volk/volk.h>

#include <Math/MatrixFactory.h>

#include "Vertex.h"
#include "VulkanBuffer.h"
#include "VulkanDevice.h"
#include "VulkanSwapchain.h"
#include "VulkanTexture.h"

namespace lab::vk {

    struct UniformBufferObject {
		math::Mat4 Model;
		math::Mat4 View;
		math::Mat4 Proj;
	};

	class VulkanGraphicsPipeline {
	  public:
        VulkanGraphicsPipeline(VulkanDevice* device, const VulkanSwapchain& swapchain, uint32_t maxFramesInFlight);
        ~VulkanGraphicsPipeline();
		VulkanGraphicsPipeline(const VulkanGraphicsPipeline& other) = delete;
		VulkanGraphicsPipeline& operator=(const VulkanGraphicsPipeline& other) = delete;
		VulkanGraphicsPipeline(VulkanGraphicsPipeline&& other) = delete;
		VulkanGraphicsPipeline& operator=(VulkanGraphicsPipeline&& other) = delete;

        void updateUniformBuffer(uint32_t currentImage, VkExtent2D extent);
        void bind(VkCommandBuffer cb, uint32_t frameIndex);

	  private:
        constexpr static std::string_view k_TexturePath = "assets/textures/viking_room.png";

        VkDevice m_Device = VK_NULL_HANDLE; // NON-OWNING
        VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
		VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
		VkPipeline m_GraphicsPipeline = VK_NULL_HANDLE;
		VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
		std::vector<VkDescriptorSet> m_DescriptorSets;

        VulkanTexture m_Texture;
        std::vector<VulkanBuffer> m_UniformBuffers;
		std::vector<void*> m_UniformBuffersMapped;

        void createUniformBuffers(VulkanDevice* device, uint32_t maxFramesInFlight);
        void createDescriptorSetLayout();
		void createGraphicsPipeline(VkSampleCountFlagBits sampleCount, VkFormat colorFormat, VkFormat depthFormat);
		[[nodiscard]] VkShaderModule createShaderModule(const std::vector<char>& code) const;
        void createDescriptorPool(uint32_t maxFramesInFlight);
		void createDescriptorSets(uint32_t maxFramesInFlight);
	};
} // namespace lab::vk
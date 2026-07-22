#pragma once

#include <vector>

#include <vulkan/vulkan.h>

#include <Math/MatrixFactory.h>

#include "VulkanBuffer.h"
#include "VulkanDevice.h"
#include "VulkanImage.h"
#include "VulkanSwapchain.h"
#include "VulkanTexture.h"
#include "Window.h"

namespace lab::vk {

	struct Vertex {
		math::Vector3 Position;
		math::Vector2 TexCoords;

		static VkVertexInputBindingDescription getBindingDescription() {
			return { .binding = 0, .stride = sizeof(Vertex), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX };
		}

		static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
			return { { { .location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, Position) },
				       { .location = 1, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(Vertex, TexCoords) } } };
		}

		bool operator==(const Vertex& other) const {
			return Position == other.Position && TexCoords == other.TexCoords;
		}
	};

} // namespace lab::vk

template <>
struct std::hash<lab::vk::Vertex> {
	size_t operator()(const lab::vk::Vertex& vertex) const noexcept {
		return std::hash<math::Vector3>()(vertex.Position) ^ (std::hash<math::Vector2>()(vertex.TexCoords) << 1);
	}
};

namespace lab::vk {

	struct UniformBufferObject {
		math::Mat4 Model;
		math::Mat4 View;
		math::Mat4 Proj;
	};

	class VulkanEngine {
	  public:
		explicit VulkanEngine(const Window& window);
		void init(const Window& window);
		void shutdown();
		void drawFrame();

	  private:
		constexpr static std::string_view k_ModelPath = "assets/models/viking/viking_room.obj";
		constexpr static std::string_view k_TexturePath = "assets/textures/viking_room.png";
		constexpr static int k_MaxFramesInFlight = 3;
		constexpr static VkSampleCountFlagBits k_MaxMSAA = VK_SAMPLE_COUNT_8_BIT;
		int m_FrameIndex = 0;
		SDL_Window* m_WindowHandle = nullptr; // NOT THE OWNER

		std::vector<Vertex> m_Vertices;
		std::vector<uint32_t> m_Indices;
		std::unordered_map<Vertex, uint32_t> m_UniqueVertices;

		

		VulkanDevice m_Device;
		VulkanSwapchain m_Swapchain;
		VulkanBuffer m_VertexBuffer;
		VulkanBuffer m_IndexBuffer;
		std::vector<VulkanBuffer> m_UniformBuffers;
		std::vector<void*> m_UniformBuffersMapped;
		VulkanTexture m_Texture;

		
		VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
		VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
		VkPipeline m_GraphicsPipeline = VK_NULL_HANDLE;
		VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
		std::vector<VkDescriptorSet> m_DescriptorSets;
		std::vector<VkCommandBuffer> m_CmdBuffers;
		std::vector<VkSemaphore> m_PresentCompleteSemaphores;
		
		std::vector<VkFence> m_DrawFences;

		
		
		void createDescriptorSetLayout();
		void createGraphicsPipeline();
		[[nodiscard]] VkShaderModule createShaderModule(const std::vector<char>& code) const;
		void loadModel();
		void createVertexBuffer();
		void createIndexBuffer();
		void createUniformBuffers();
		void createDescriptorPool();
		void createDescriptorSets();
		void createCommandBuffers();
		void recordCommandBuffer(uint32_t frameIndex);
		void createSyncObjects();
		
		void updateUniformBuffer(uint32_t currentImage);
	};
} // namespace lab::vk
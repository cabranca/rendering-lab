#pragma once

#include <vector>

#include <vulkan/vulkan.h>

#include <Math/MatrixFactory.h>

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
	};

	struct UniformBufferObject {
		math::Mat4 Model;
		math::Mat4 View;
		math::Mat4 Proj;
	};

	class VulkanEngine {
	  public:
		void init(const Window& window);
		void shutdown();
		void drawFrame();

	  private:
		constexpr static std::array<Vertex, 8> k_Vertices = { { { .Position = { -0.5f, -0.5f, 0.0f }, .TexCoords = { 1.0f, 0.0f } },
			                                                    { .Position = { 0.5f, -0.5f, 0.0f }, .TexCoords = { 0.0f, 0.0f } },
			                                                    { .Position = { 0.5f, 0.5f, 0.0f }, .TexCoords = { 0.0f, 1.0f } },
			                                                    { .Position = { -0.5f, 0.5f, 0.0f }, .TexCoords = { 1.0f, 1.0f } },
			                                                    { .Position = { -0.5f, -0.5f, -0.5f }, .TexCoords = { 1.0f, 0.0f } },
			                                                    { .Position = { 0.5f, -0.5f, -0.5f }, .TexCoords = { 0.0f, 0.0f } },
			                                                    { .Position = { 0.5f, 0.5f, -0.5f }, .TexCoords = { 0.0f, 1.0f } },
			                                                    { .Position = { -0.5f, 0.5f, -0.5f }, .TexCoords = { 1.0f, 1.0f } } } };
		constexpr static std::array<uint16_t, 12> k_Indices = { 0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4 };

		constexpr static int k_MaxFramesInFlight = 3;
		int m_FrameIndex = 0;
		SDL_Window* m_WindowHandle = nullptr; // NOT THE OWNER

		VkInstance m_Instance = VK_NULL_HANDLE;
		VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;
		VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
		VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
		uint32_t m_QueueFamilyIndex = 0;
		VkDevice m_Device = VK_NULL_HANDLE;
		VkQueue m_Queue = VK_NULL_HANDLE;
		VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
		std::vector<VkImage> m_Images;
		VkSurfaceFormatKHR m_SelectedFormat;
		VkExtent2D m_Extent;
		std::vector<VkImageView> m_ImageViews;
		VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
		VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
		VkPipeline m_GraphicsPipeline = VK_NULL_HANDLE;
		VkCommandPool m_CmdPool = VK_NULL_HANDLE;
		VkImage m_DepthImage = VK_NULL_HANDLE;
		VkDeviceMemory m_DepthImageMemory = VK_NULL_HANDLE;
		VkImageView m_DepthImageView = VK_NULL_HANDLE;
		VkFormat m_DepthFormat;
		VkImage m_TextureImage = VK_NULL_HANDLE;
		VkDeviceMemory m_TextureImageMemory = VK_NULL_HANDLE;
		VkImageView m_TextureImageView = VK_NULL_HANDLE;
		VkSampler m_TextureSampler = VK_NULL_HANDLE;
		VkBuffer m_VertexBuffer = VK_NULL_HANDLE;
		VkDeviceMemory m_VertexBufferMemory = VK_NULL_HANDLE;
		VkBuffer m_IndexBuffer = VK_NULL_HANDLE;
		VkDeviceMemory m_IndexBufferMemory = VK_NULL_HANDLE;
		std::vector<VkBuffer> m_UniformBuffers;
		std::vector<VkDeviceMemory> m_UniformBuffersMemory;
		std::vector<void*> m_UniformBuffersMapped;
		VkCommandPool m_SingleTimeCmdPool = VK_NULL_HANDLE;
		VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
		std::vector<VkDescriptorSet> m_DescriptorSets;
		std::vector<VkCommandBuffer> m_CmdBuffers;
		std::vector<VkSemaphore> m_PresentCompleteSemaphores;
		std::vector<VkSemaphore> m_RenderFinishedSemaphores;
		std::vector<VkFence> m_DrawFences;

		void createInstance();
		static std::vector<VkLayerProperties> getAvailableLayers();
		static std::vector<VkExtensionProperties> getAvailableExtensions();
		void createDebugMessenger();
		void createSurface();
		void pickPhysicalDevice();
		void createLogicalDevice();
		void createSwapchain(VkSwapchainKHR oldSwapchain);
		[[nodiscard]] static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
		[[nodiscard]] static VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
		[[nodiscard]] VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
		[[nodiscard]] static uint32_t chooseSwapMinImageCount(const VkSurfaceCapabilitiesKHR& surfaceCapabilities);
		void createImageViews();
		VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
		void createDescriptorSetLayout();
		void createGraphicsPipeline();
		[[nodiscard]] VkShaderModule createShaderModule(const std::vector<char>& code) const;
		void createCommandPools();
		void createDepthResources();
		VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
		void createTextureImage();
		std::pair<VkImage, VkDeviceMemory> createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
		                                               VkImageUsageFlags usage, VkMemoryPropertyFlags properties);
		void createTextureImageView();
		void createTextureSampler();
		void createVertexBuffer();
		void createIndexBuffer();
		void createUniformBuffers();
		std::pair<VkBuffer, VkDeviceMemory> createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
		void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
		uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
		VkCommandBuffer beginSingleTimeCommands();
		void endSingleTimeCommands(VkCommandBuffer commandBuffer);
		void copyBufferToImage(VkCommandBuffer cmdBuffer, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
		void createDescriptorPool();
		void createDescriptorSets();
		void createCommandBuffers();
		void recordCommandBuffer(uint32_t frameIndex);
		void transitionImageLayout(VkCommandBuffer buffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
		                           VkAccessFlags2 srcAccessMask, VkAccessFlags2 dstAccessMask, VkPipelineStageFlags2 srcStageMask,
		                           VkPipelineStageFlags2 dstStageMask, VkImageAspectFlags aspectFlags);
		void createSyncObjects();
		void createRenderFinishedSemaphores();
		void updateUniformBuffer(uint32_t currentImage);
		void recreateSwapchain();
		void cleanupSwapchain();
	};
} // namespace lab::vk
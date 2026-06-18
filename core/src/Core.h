#pragma once

#include <SDL3/SDL_video.h>
#include <string_view>
#include <vulkan/vulkan_core.h>

#include "DeviceManager.h"
#include "Instance.h"
#include "Queue.h"
#include "Swapchain.h"

namespace lab {

	class Core {
	  public:
		~Core();
		void init(std::string_view appName, SDL_Window* window); // NOT THE OWNER OF THE WINDOW
		void shutdown();

		VkDevice getDevice() const;
		VkExtent2D getExtent() const;
		uint32_t getNumImages() const;
		VkImage getImage(uint32_t index) const;
		void createCommandBuffers(uint32_t numImages, VkCommandBuffer* cmdBuffers);
		void freeCommandBuffers(uint32_t bufferCount, const VkCommandBuffer* cmdBuffers);
		Queue* getQueue();
		VkRenderPass createSimpleRenderPass();
		std::vector<VkFramebuffer> createFrameBuffers(VkRenderPass renderPass);
		void destroyFrameBuffers(const std::vector<VkFramebuffer>& frameBuffers);

		// Recreates the swapchain (and its image views) against the current surface size. Returns
		// false when the window is minimized (zero extent) and recreation should be skipped.
		// Callers must recreate any swapchain-dependent resources (framebuffers, command buffers).
		bool recreateSwapchain();


		static void beginCommandBuffer(VkCommandBuffer buffer, VkCommandBufferUsageFlags flags);

	  private:
		Instance m_Instance;
		VkDebugUtilsMessengerEXT m_DebugMessenger{ VK_NULL_HANDLE };
		VkSurfaceKHR m_Surface{ VK_NULL_HANDLE };
		DeviceManager m_DeviceManager;
		uint32_t m_QueueFamily{ 0 };
		Queue m_Queue;
		VkDevice m_Device{ VK_NULL_HANDLE };
		Swapchain m_Swapchain;
		VkCommandPool m_CmdPool;

		void createDebugCallback();
		void createSurface(SDL_Window* window);
		void createDevice();
		void createCommandPool();
	};
} // namespace lab
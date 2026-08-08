#pragma once

#include <vector>

#include <volk/volk.h>

#include "VulkanDevice.h"
#include "VulkanGraphicsPipeline.h"
#include "VulkanInstance.h"
#include "VulkanModel.h"
#include "VulkanSwapchain.h"
#include "Window.h"

namespace lab::vk {

	class VulkanEngine {
	  public:
		explicit VulkanEngine(const Window& window);
		void shutdown();
		void drawFrame();

	  private:
		constexpr static std::string_view k_ModelPath = "assets/models/viking/viking_room.obj";
		constexpr static uint32_t k_MaxFramesInFlight = 3;
		constexpr static VkSampleCountFlagBits k_MaxMSAA = VK_SAMPLE_COUNT_8_BIT;
		int m_FrameIndex = 0;
		SDL_Window* m_WindowHandle = nullptr; // NOT THE OWNER

		VulkanInstance m_Instance;
		VulkanDevice m_Device;
		VulkanSwapchain m_Swapchain;
		VulkanModel m_Model;

		VulkanGraphicsPipeline m_Pipeline;

		std::vector<VkCommandBuffer> m_CmdBuffers;
		std::vector<VkSemaphore> m_PresentCompleteSemaphores;
		
		std::vector<VkFence> m_DrawFences;

		void createCommandBuffers();
		void recordCommandBuffer(uint32_t frameIndex);
		void createSyncObjects();
	};
} // namespace lab::vk
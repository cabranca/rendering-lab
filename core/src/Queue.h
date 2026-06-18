#pragma once

#include "vulkan/vulkan_core.h"
#include <vector>
namespace lab {

	class Queue {
	  public:
        void init(VkDevice device, VkSwapchainKHR swapchain, uint32_t queueFamily, uint32_t queueIndex, uint32_t numImages);
        void shutdown();
		VkResult acquireNextImage(uint32_t& imageIndex);
        void submitSync(VkCommandBuffer cmdBuffer);
		void submitAsync(VkCommandBuffer cmdBuffer);
		VkResult present(uint32_t index);
        void waitIdle();
        void setSwapchain(VkSwapchainKHR swapchain);

        static VkSemaphore createSemaphore(VkDevice device);
        static VkFence createFence(VkDevice device);

	  private:
        VkDevice m_Device{ VK_NULL_HANDLE };
        VkSwapchainKHR m_Swapchain{ VK_NULL_HANDLE };
        VkQueue m_Queue{ VK_NULL_HANDLE };
        uint32_t m_NumImages{ 0 };
        std::vector<VkSemaphore> m_RenderCompleteSem;
        std::vector<VkSemaphore> m_PresentCompleteSem;
        std::vector<VkFence> m_InFlightFences;
        uint8_t m_FrameIndex{ 0 };
	};
} // namespace lab
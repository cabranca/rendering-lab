#pragma once

#include "vulkan/vulkan_core.h"
namespace lab {

	class Queue {
	  public:
        void init(VkDevice device, VkSwapchainKHR swapchain, uint32_t queueFamily, uint32_t queueIndex);
        void shutdown();
		uint32_t acquireNextImage();
        void submitSync(VkCommandBuffer cmdBuffer);
		void submitAsync(VkCommandBuffer cmdBuffer);
		void present(uint32_t index);
        void waitIdle();

        static VkSemaphore createSemaphore(VkDevice device);

	  private:
        VkDevice m_Device{ VK_NULL_HANDLE };
        VkSwapchainKHR m_Swapchain{ VK_NULL_HANDLE };
        VkQueue m_Queue{ VK_NULL_HANDLE };
        VkSemaphore m_RenderCompleteSem;
        VkSemaphore m_PresentCompleteSem;
	};
} // namespace lab
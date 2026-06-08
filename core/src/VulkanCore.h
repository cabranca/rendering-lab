#pragma once

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

#include "vulkan/vulkan_core.h"

#include "Swapchain.h"
#include "VertexBuffer.h"
#include "VulkanContext.h"
#include "Window.h"

namespace lab {

	// Reusable Vulkan boilerplate shared by every demo: device/context, window,
	// swapchain and the per-frame render loop. Demos own their own pipelines,
	// descriptors, shaders and draw calls; this just hands them a command buffer
	// each frame between beginFrame() and endFrame().
	class VulkanCore {
	  public:
		static constexpr uint32_t maxFramesInFlight{ 2 };

		void init(std::string_view applicatioName);
		void shutdown();

		// Imports a model and uploads it to a GPU vertex/index buffer. The caller
		// owns the returned mesh and must release it with mesh.shutdown(getContext()).
		VertexBuffer loadModel(std::string_view path);

		// Acquires the next swapchain image, begins the command buffer and dynamic
		// rendering (color + depth, cleared), and sets a full-window viewport/scissor.
		// Returns the command buffer to record into, or VK_NULL_HANDLE when the frame
		// is skipped (swapchain was rebuilt) — the caller should then continue its loop.
		VkCommandBuffer beginFrame();
		// Ends rendering, transitions the image to present, submits and presents.
		void endFrame();
		// Request a swapchain rebuild; call from the SDL window-resized event.
		void requestResize() {
			m_Resized = true;
		}

		const VulkanContext& getContext() const {
			return m_VulkanCtx;
		}
		VkDevice getDevice() const {
			return m_VulkanCtx.getDevice();
		}
		VmaAllocator getAllocator() const {
			return m_VulkanCtx.getAllocator();
		}
		Window& getWindow() {
			return m_Window;
		}
		VkFormat getColorFormat() const {
			return m_Swapchain.getImageFormat();
		}
		VkFormat getDepthFormat() const {
			return m_Swapchain.getDepthFormat();
		}
		VkExtent2D getExtent() const {
			return m_Swapchain.getExtent();
		}
		uint32_t getFrameIndex() const {
			return m_FrameIndex;
		}

	  private:
		void recreateSwapchain();

		VulkanContext m_VulkanCtx;
		Window m_Window;
		Swapchain m_Swapchain;

		VkCommandPool m_CommandPool{ VK_NULL_HANDLE };
		std::array<VkCommandBuffer, maxFramesInFlight> m_CommandBuffers{};
		std::array<VkFence, maxFramesInFlight> m_Fences{};
		std::array<VkSemaphore, maxFramesInFlight> m_ImageAcquiredSemaphores{};
		std::vector<VkSemaphore> m_RenderCompleteSemaphores; // one per swapchain image

		uint32_t m_FrameIndex{ 0 };
		uint32_t m_ImageIndex{ 0 };
		bool m_Resized{ false };
	};
} // namespace lab

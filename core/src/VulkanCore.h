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

		void init(std::string_view applicationName);
		void shutdown();

		// Imports a model and uploads it to a GPU vertex/index buffer. The caller
		// owns the returned mesh and must release it with mesh.shutdown(getContext()).
		VertexBuffer loadModel(std::string_view path);

		// Acquires the next swapchain image and begins recording its one-time command
		// buffer. Returns the command buffer, or VK_NULL_HANDLE when the frame is
		// skipped (swapchain was rebuilt) — the caller should then continue its loop.
		// The caller owns the render structure: compose the cmd* helpers below between
		// beginFrame() and endFrame() (transition, begin/end rendering, viewport, ...).
		VkCommandBuffer beginFrame();
		// Ends the command buffer, submits and presents. All recording — including
		// vkCmdEndRendering and the transition back to present — must already be done.
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
		const std::vector<VkImageView>& getImageViews() const {
			return m_Swapchain.getImageViews();
		}
		// The current frame's swapchain color image / view (valid between beginFrame()
		// and endFrame()), plus the shared depth target. Demos pass these to the cmd*
		// helpers to render into the swapchain image.
		VkImage getCurrentImage() const {
			return m_Swapchain.getImages()[m_ImageIndex];
		}
		VkImageView getCurrentImageView() const {
			return m_Swapchain.getImageViews()[m_ImageIndex];
		}
		VkImage getDepthImage() const {
			return m_Swapchain.getDepthImage();
		}
		VkImageView getDepthImageView() const {
			return m_Swapchain.getDepthImageView();
		}

		// Command-recording helpers demos compose into render passes between
		// beginFrame() and endFrame(). They depend only on their arguments, so they
		// also work on demo-owned images (e.g. an off-screen post-processing target),
		// not just the swapchain. cmdBeginRendering clears color (black) + depth.
		void cmdTransitionToAttachment(VkCommandBuffer cb, VkImage colorImage, VkImage depthImage) const;
		void cmdBeginRendering(VkCommandBuffer cb, VkImageView colorView, VkImageView depthView, VkExtent2D extent) const;
		void cmdSetFullViewportScissor(VkCommandBuffer cb, VkExtent2D extent) const;
		void cmdTransitionToPresent(VkCommandBuffer cb, VkImage colorImage) const;

	  private:
		// One-time setup split out of init().
		void createCommandResources();
		void createFrameSyncObjects();
		// Render-complete semaphores are per swapchain image, so they are rebuilt
		// whenever the swapchain is (re)created.
		void createRenderCompleteSemaphores();
		void destroyRenderCompleteSemaphores();
		void recreateSwapchain();

		// Per-frame steps split out of beginFrame()/endFrame().
		bool acquireNextImage();
		void submitFrame(VkCommandBuffer commandBuffer);
		VkResult presentFrame();

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

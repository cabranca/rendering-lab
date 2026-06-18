#include "GraphicsPipeline.h"
#include <volk/volk.h>
#include <SDL3/SDL.h>

#include <Core.h>
#include <ShaderCompiler.h>

#include <Check.h>

class VulkanApp {
  public:
	~VulkanApp() {
		m_Queue->waitIdle();
		vkDestroyShaderModule(m_Core.getDevice(), m_Vs, nullptr);
		vkDestroyShaderModule(m_Core.getDevice(), m_Fs, nullptr);
		m_GraphicsPipeline.shutdown();
		m_Core.freeCommandBuffers(m_CommandBuffers.size(), m_CommandBuffers.data());
		m_Core.destroyFrameBuffers(m_FrameBuffers);
		vkDestroyRenderPass(m_Core.getDevice(), m_RenderPass, nullptr);
	}

	void onResize() {
		m_FramebufferResized = true;
	}

	void init(std::string_view appName, SDL_Window* window) {
		m_Window = window;
		m_Core.init(appName, window);
		m_NumImages = m_Core.getNumImages();
		m_Queue = m_Core.getQueue();
		m_RenderPass = m_Core.createSimpleRenderPass();
		m_FrameBuffers = m_Core.createFrameBuffers(m_RenderPass);
		createShaders();
		createPipeline();
		createCommandBuffers();
		recordCommandBuffers();
	}

	void renderScene() {
		// Rebuild before rendering so the frame we present already matches the new size.
		// Detecting the resize only after present would show one stale old-size frame first.
		if (m_FramebufferResized) {
			m_FramebufferResized = false;
			recreateSwapchain();
		}

		uint32_t imageIndex = 0;
		VkResult acquireResult = m_Queue->acquireNextImage(imageIndex);
		if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
			recreateSwapchain();
			return;
		}

		m_Queue->submitAsync(m_CommandBuffers.at(imageIndex));

		VkResult presentResult = m_Queue->present(imageIndex);
		if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR)
			recreateSwapchain();
	}

  private:
	SDL_Window* m_Window; // Not the owner
	lab::Core m_Core;
	lab::Queue* m_Queue;
	uint32_t m_NumImages{ 0 };
	std::vector<VkCommandBuffer> m_CommandBuffers;
	VkRenderPass m_RenderPass{ VK_NULL_HANDLE };
	std::vector<VkFramebuffer> m_FrameBuffers{ VK_NULL_HANDLE };
	VkShaderModule m_Vs;
	VkShaderModule m_Fs;
	lab::GraphicsPipeline m_GraphicsPipeline;
	bool m_FramebufferResized{ false };

	void recreateSwapchain() {
		if (!m_Core.recreateSwapchain())
			return; // Window minimized; try again on a later frame.

		m_Core.destroyFrameBuffers(m_FrameBuffers);
		m_FrameBuffers = m_Core.createFrameBuffers(m_RenderPass);
		recordCommandBuffers();
	}

	void createCommandBuffers() {
		m_CommandBuffers.resize(m_NumImages);
		m_Core.createCommandBuffers(m_NumImages, m_CommandBuffers.data());
	}

	void createShaders() {
		m_Vs = lab::loadGLSLShader(m_Core.getDevice(), "assets/test.vert");
		m_Fs = lab::loadGLSLShader(m_Core.getDevice(), "assets/test.frag");
	}

	void createPipeline() {
		m_GraphicsPipeline.init(m_Core.getDevice(), m_RenderPass, m_Vs, m_Fs);
	}

	void recordCommandBuffers() {
		VkExtent2D extent = m_Core.getExtent();

		VkClearColorValue clearColor{ 1.0f, 0.f, 0.f, 0.f };
		VkClearValue clearValue{ .color = clearColor };

		VkRenderPassBeginInfo renderPassBI{ .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			                                .renderPass = m_RenderPass,
			                                .renderArea = { .offset = { .x = 0, .y = 0 }, .extent = extent },
			                                .clearValueCount = 1,
			                                .pClearValues = &clearValue };

		VkViewport viewport{ .x = 0.f,
			                 .y = 0.f,
			                 .width = static_cast<float>(extent.width),
			                 .height = static_cast<float>(extent.height),
			                 .minDepth = 0.f,
			                 .maxDepth = 1.f };
		VkRect2D scissor{ .offset = { .x = 0, .y = 0 }, .extent = extent };

		for (uint32_t i = 0; i < m_CommandBuffers.size(); i++) {
			lab::Core::beginCommandBuffer(m_CommandBuffers.at(i), VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);

			renderPassBI.framebuffer = m_FrameBuffers.at(i);

			vkCmdBeginRenderPass(m_CommandBuffers.at(i), &renderPassBI, VK_SUBPASS_CONTENTS_INLINE);

			m_GraphicsPipeline.bind(m_CommandBuffers.at(i));

			vkCmdSetViewport(m_CommandBuffers.at(i), 0, 1, &viewport);
			vkCmdSetScissor(m_CommandBuffers.at(i), 0, 1, &scissor);

			uint32_t vertexCount = 3;
			uint32_t instanceCount = 1;
			uint32_t firstVertex = 0;
			uint32_t firstInstance = 0;

			vkCmdDraw(m_CommandBuffers.at(i), vertexCount, instanceCount, firstVertex, firstInstance);

			vkCmdEndRenderPass(m_CommandBuffers.at(i));

			lab::chk(vkEndCommandBuffer(m_CommandBuffers.at(i)));
		}
	}
};

int main() {
	SDL_Init(SDL_INIT_VIDEO);
	std::println("SDL initialized");

	auto* window = SDL_CreateWindow("InstanceDemo", 1600, 900, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

	VulkanApp app;
	app.init("Instance Demo", window);

	SDL_Event event;
	bool running = true;
	while (running) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_QUIT) {
				running = false;
			} else if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
				app.onResize();
			}
		}
		app.renderScene();
	}

	SDL_DestroyWindow(window);
	SDL_Quit();
	std::println("SDL quitted");

	return 0;
}

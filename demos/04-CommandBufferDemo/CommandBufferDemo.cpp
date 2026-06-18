#include <volk/volk.h>
#include <SDL3/SDL.h>

#include <Core.h>

#include <Check.h>

class VulkanApp {
  public:
	~VulkanApp() {
		m_Queue->waitIdle();
		m_Core.freeCommandBuffers(m_CommandBuffers.size(), m_CommandBuffers.data());
	}

	void init(std::string_view appName, SDL_Window* window) {
		m_Core.init(appName, window);
		m_NumImages = m_Core.getNumImages();
		m_Queue = m_Core.getQueue();
		createCommandBuffers();
		recordCommandBuffers();
	}

	void renderScene() {
		uint32_t imageIndex = m_Queue->acquireNextImage();

		m_Queue->submitAsync(m_CommandBuffers.at(imageIndex));

		m_Queue->present(imageIndex);
	}

  private:
	lab::Core m_Core;
	lab::Queue* m_Queue;
	uint32_t m_NumImages{ 0 };
	std::vector<VkCommandBuffer> m_CommandBuffers;

	void createCommandBuffers() {
		m_CommandBuffers.resize(m_NumImages);
		m_Core.createCommandBuffers(m_NumImages, m_CommandBuffers.data());
	}

	void recordCommandBuffers() {
		VkClearColorValue clearColor{ 1.0f, 0.f, 0.f, 0.f };

		VkImageSubresourceRange imageRange{
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1
		};

		for (uint32_t i = 0; i < m_CommandBuffers.size(); i++) {
			VkImageMemoryBarrier presentToClearBarrier{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				                                        .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
				                                        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
				                                        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				                                        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				                                        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				                                        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				                                        .image = m_Core.getImage(i),
				                                        .subresourceRange = imageRange };

			VkImageMemoryBarrier clearToPresentBarrier{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				                                        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
				                                        .dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
				                                        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				                                        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				                                        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				                                        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				                                        .image = m_Core.getImage(i),
				                                        .subresourceRange = imageRange };

			lab::Core::beginCommandBuffer(m_CommandBuffers.at(i), VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);

			vkCmdPipelineBarrier(m_CommandBuffers.at(i), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
			                     nullptr, 1, &presentToClearBarrier);

			vkCmdClearColorImage(m_CommandBuffers.at(i), m_Core.getImage(i), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &imageRange);
			
			vkCmdPipelineBarrier(m_CommandBuffers.at(i), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
			                     nullptr, 1, &clearToPresentBarrier);

			lab::chk(vkEndCommandBuffer(m_CommandBuffers.at(i)));
		}
	}
};

int main() {
	SDL_Init(SDL_INIT_VIDEO);
	std::println("SDL initialized");

	auto* window = SDL_CreateWindow("InstanceDemo", 1600, 1900, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

	VulkanApp app;
	app.init("Instance Demo", window);

	SDL_Event event;
	bool running = true;
	while (running) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_QUIT) {
				running = false;
			}
		}
		app.renderScene();
	}

	SDL_DestroyWindow(window);
	SDL_Quit();
	std::println("SDL quitted");

	return 0;
}
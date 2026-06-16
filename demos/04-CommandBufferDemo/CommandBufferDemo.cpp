#include <volk/volk.h>
#include <SDL3/SDL_init.h>

#include <Core.h>

#include <Check.h>

class VulkanApp {
  public:
	~VulkanApp() {
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
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1
		};

		for (uint32_t i = 0; i < m_CommandBuffers.size(); i++) {
			lab::Core::beginCommandBuffer(m_CommandBuffers.at(i), VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);

			vkCmdClearColorImage(m_CommandBuffers.at(i), m_Core.getImage(i), VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1, &imageRange);
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

	while(true) {
		app.renderScene();
	}

	SDL_DestroyWindow(window);
	SDL_Quit();
	std::println("SDL quitted");

	return 0;
}
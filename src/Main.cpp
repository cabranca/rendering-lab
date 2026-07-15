#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>

#include "Vulkan/VulkanEngine.h"
#include "Window.h"

#include "Logger.h"

int main() {
	lab::Window window;
	lab::vk::VulkanEngine engine;

	lab::Logger::init();
	lab::Logger::setLevel("debug");
	
	window.init("Rendering Lab", 1600, 900, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
	engine.init(window);

	bool isRunning = true;
	while(isRunning) {
		SDL_Event ev;
		while (SDL_PollEvent(&ev)) {
			if (ev.type == SDL_EVENT_QUIT) {
				isRunning = false;
			}
		}
		engine.drawFrame();
	}

	engine.shutdown();
	window.shutdown();
	return 0;
}

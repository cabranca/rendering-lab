#pragma once

#include <SDL3/SDL_video.h>
#include <string_view>

#include "DeviceManager.h"
#include "Instance.h"

namespace lab {

	class Core {
	  public:
		void init(std::string_view appName, SDL_Window* window); // NOT THE OWNER OF THE WINDOW
		void shutdown();

	  private:
		Instance m_Instance;
		VkDebugUtilsMessengerEXT m_DebugMessenger{ VK_NULL_HANDLE };
		VkSurfaceKHR m_Surface{ VK_NULL_HANDLE };
		DeviceManager m_DeviceManager;
		uint32_t m_QueueFamily{ 0 };

		void createDebugCallback();
		void createSurface(SDL_Window* window);
	};
} // namespace lab
#pragma once

#include <vector>

#include <volk/volk.h>

#include "Window.h"

namespace lab::vk {

	// Owns the Vulkan instance-scope objects: the VkInstance, the debug messenger, and the
	// window surface. Split out from VulkanDevice so instance and device lifetimes are separate
	// (the instance outlives the device, which owns queue/command-pool children of the device).
	class VulkanInstance {
	  public:
		explicit VulkanInstance(const Window& window);
		~VulkanInstance();
		VulkanInstance(const VulkanInstance& other) = delete;
		VulkanInstance& operator=(const VulkanInstance& other) = delete;
		VulkanInstance(VulkanInstance&& other) = delete;
		VulkanInstance& operator=(VulkanInstance&& other) = delete;

		[[nodiscard]] VkInstance getInstance() const;
		[[nodiscard]] VkSurfaceKHR getSurface() const;

	  private:
		VkInstance m_Instance = VK_NULL_HANDLE;
		VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;
		VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
		SDL_Window* m_WindowHandle = nullptr; // NOT THE OWNER

		void createInstance();
		static std::vector<VkLayerProperties> getAvailableLayers();
		static std::vector<VkExtensionProperties> getAvailableExtensions();
		void createDebugMessenger();
		void createSurface();
	};
} // namespace lab::vk

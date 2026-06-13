#include "Instance.h"

#include <iostream>
#include <vector>

// This translation unit hosts the single implementation of volk for the lab.
#define VOLK_IMPLEMENTATION
#include <volk/volk.h>
#include <SDL3/SDL_vulkan.h>

#include "Check.h"

namespace lab {

	void Instance::init(std::string_view appName) {
		volkInitialize();
		VkApplicationInfo appInfo{ .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			                       .pApplicationName = appName.data(),
			                       .apiVersion = VK_API_VERSION_1_0 };

		uint32_t extensionCount{ 0 };
		auto* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
		std::vector<const char*> extensions(sdlExtensions, sdlExtensions + extensionCount);
		extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

		std::vector<const char*> layers = { "VK_LAYER_KHRONOS_validation" };

		VkInstanceCreateInfo instanceCI{ .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			                             .pApplicationInfo = &appInfo,
			                             .enabledLayerCount = static_cast<uint32_t>(layers.size()),
			                             .ppEnabledLayerNames = layers.data(),
			                             .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
			                             .ppEnabledExtensionNames = extensions.data() };
		chk(vkCreateInstance(&instanceCI, nullptr, &m_Instance));
		std::cout << "Vulkan Instance created\n";
		volkLoadInstance(m_Instance);
	}

	void Instance::shutdown() {
		vkDestroyInstance(m_Instance, nullptr);
		std::cout << "Vulkan Instance destroyed\n";
	}

	VkInstance Instance::getInstance() const {
		return m_Instance;
	}
} // namespace lab
#include "Core.h"

#include <SDL3/SDL_vulkan.h>
#include <volk/volk.h>

#include "Check.h"
#include "Utils.h"
#include "vulkan/vulkan_core.h"

namespace lab {

	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT Severity,
	                                                    VkDebugUtilsMessageTypeFlagsEXT Type,
	                                                    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
		std::println("Debug callback: {}", pCallbackData->pMessage);
		std::println("  Severity {}", GetDebugSeverityStr(Severity));
		std::println("  Type {}", GetDebugType(Type));
		std::println("  Objects ");

		for (uint32_t i = 0; i < pCallbackData->objectCount; i++) {
			std::println("{}", pCallbackData->pObjects[i].objectHandle);
		}

		std::println("");

		return VK_FALSE; // The calling function should not be aborted
	}

	void Core::init(std::string_view appName, SDL_Window* window) {
		m_Instance.init(appName);
		createDebugCallback();
		createSurface(window);
		m_DeviceManager.init(m_Instance.getInstance(), m_Surface);
	}

	void Core::shutdown() {
		vkDestroySurfaceKHR(m_Instance.getInstance(), m_Surface, nullptr);
		std::println("SDL Surface destroyed");

		vkDestroyDebugUtilsMessengerEXT(m_Instance.getInstance(), m_DebugMessenger, nullptr);
		std::println("Debug messenger destroyed");

		m_Instance.shutdown();
	}

	void Core::createDebugCallback() {
		VkDebugUtilsMessengerCreateInfoEXT MessengerCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
			.pNext = NULL,
			.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
			                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
			.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			               VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
			.pfnUserCallback = DebugCallback,
			.pUserData = NULL
		};

		chk(vkCreateDebugUtilsMessengerEXT(m_Instance.getInstance(), &MessengerCreateInfo, nullptr, &m_DebugMessenger));

		std::println("Debug utils created");
	}

	void Core::createSurface(SDL_Window* window) {
		chk(SDL_Vulkan_CreateSurface(window, m_Instance.getInstance(), nullptr, &m_Surface));
		std::println("SDL Surface created");
	}
} // namespace lab
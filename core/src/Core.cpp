#include "Core.h"

#include <volk/volk.h>

#include "Check.h"
#include "Utils.h"
#include "vulkan/vulkan_core.h"

namespace lab {

	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT Severity,
	                                                    VkDebugUtilsMessageTypeFlagsEXT Type,
	                                                    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
		printf("Debug callback: %s\n", pCallbackData->pMessage);
		printf("  Severity %s\n", GetDebugSeverityStr(Severity));
		printf("  Type %s\n", GetDebugType(Type));
		printf("  Objects ");

		for (uint32_t i = 0; i < pCallbackData->objectCount; i++) {
			printf("%llux ", pCallbackData->pObjects[i].objectHandle);
		}

		printf("\n");

		return VK_FALSE; // The calling function should not be aborted
	}

	void Core::init(std::string_view appName) {
		m_Instance.init(appName);
		createDebugCallback();
	}

	void Core::shutdown() {
		vkDestroyDebugUtilsMessengerEXT(m_Instance.getInstance(), m_DebugMessenger, nullptr);
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

		printf("Debug utils messenger created\n");
	}
} // namespace lab
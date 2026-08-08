#include "VulkanInstance.h"

#include <cstring>
#include <format>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <volk/volk.h>
#include "vulkan/vk_enum_string_helper.h"

#include "Logger.h"
#include "Utils.h"

namespace lab::vk {

	namespace {
		// The loader reports its own start-up chatter (duplicate layer manifests, binary paths that
		// differ from the ones dyld resolved) as General messages tagged "Loader Message", though it
		// leaves the tag null on some of them. Neither form is worth surfacing below Error severity,
		// where a failed ICD or layer load still needs to reach the log.
		bool isLoaderNoise(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes,
		                   const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData) {
			constexpr std::string_view k_LoaderMessageId = "Loader Message";

			if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0) {
				return false;
			}
			if ((messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) != 0) {
				return false;
			}

			const char* messageId = pCallbackData->pMessageIdName;
			return messageId == nullptr || messageId == k_LoaderMessageId;
		}

		std::string formatObjects(const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData) {
			std::string objects;

			for (uint32_t i = 0; i < pCallbackData->objectCount; i++) {
				const VkDebugUtilsObjectNameInfoEXT& object = pCallbackData->pObjects[i];

				if (!objects.empty()) {
					objects += ", ";
				}
				objects += std::format("{} {:#x}", string_VkObjectType(object.objectType), object.objectHandle);

				if (object.pObjectName != nullptr) {
					objects += std::format(" \"{}\"", object.pObjectName);
				}
			}

			return objects;
		}

		VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		                                             VkDebugUtilsMessageTypeFlagsEXT messageTypes,
		                                             const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void*) {
			if (isLoaderNoise(messageSeverity, messageTypes, pCallbackData)) {
				return VK_FALSE;
			}

			const std::string objects = formatObjects(pCallbackData);
			const std::string message = std::format("[{}] {}: {}{}", getDebugType(messageTypes),
			                                        pCallbackData->pMessageIdName != nullptr ? pCallbackData->pMessageIdName : "-",
			                                        pCallbackData->pMessage, objects.empty() ? "" : std::format(" ({})", objects));

			switch (messageSeverity) {
				case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
					CBK_ERROR("{}", message);
					break;
				case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
					CBK_WARN("{}", message);
					break;
				default:
					CBK_DEBUG("{}", message);
					break;
			}

			return VK_FALSE; // The calling function should not be aborted
		}
	} // namespace

	VulkanInstance::VulkanInstance(const Window& window) : m_WindowHandle(window.getWindowHandle()) {
		createInstance();
		createDebugMessenger();
		createSurface();
	}

	VulkanInstance::~VulkanInstance() {
		if (m_Surface != VK_NULL_HANDLE) {
			SDL_Vulkan_DestroySurface(m_Instance, m_Surface, nullptr);
			m_Surface = VK_NULL_HANDLE;
			CBK_DEBUG("Vulkan Surface destroyed");
		}

		if (m_Instance == VK_NULL_HANDLE)
			return;

		// Destroyed before the instance, but the messenger chained into VkInstanceCreateInfo::pNext
		// still covers vkDestroyInstance itself.
		if (m_DebugMessenger != VK_NULL_HANDLE) {
			vkDestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);
			m_DebugMessenger = VK_NULL_HANDLE;
			CBK_DEBUG("Vulkan Debug Messenger destroyed");
		}

		vkDestroyInstance(m_Instance, nullptr);
		m_Instance = VK_NULL_HANDLE;
		CBK_DEBUG("Vulkan Instance destroyed");
	}

	VkInstance VulkanInstance::getInstance() const {
		return m_Instance;
	}

	VkSurfaceKHR VulkanInstance::getSurface() const {
		return m_Surface;
	}

	void VulkanInstance::createInstance() {
		vkCheck(volkInitialize(), "volkInitialize");

		const VkApplicationInfo appInfo{ .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			                             .pNext = nullptr,
			                             .pApplicationName = "Render Lab",
			                             .apiVersion = VK_API_VERSION_1_4 };

		const auto availableLayers = getAvailableLayers();
		const auto availableExtensions = getAvailableExtensions();

		std::vector<const char*> layers;
		VkInstanceCreateFlags flags = 0;
		bool debugUtilsAvailable = false;

		uint32_t sdlExtensionsCount = 0;
		const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionsCount);
		std::vector<const char*> extensions(sdlExtensions, sdlExtensions + sdlExtensionsCount);

		for (const auto& extension: availableExtensions) {
			if (std::strcmp(extension.extensionName, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0) {
				extensions.emplace_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
				flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
			}
			if (std::strcmp(extension.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0) {
				extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
				debugUtilsAvailable = true;
			}
		}

		for (const auto& layer: availableLayers) {
			if (std::strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") == 0) {
				layers.emplace_back("VK_LAYER_KHRONOS_validation");
			}
		}

		const VkDebugUtilsMessengerCreateInfoEXT debugMessengerCI{ .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
			                                                       .pNext = nullptr,
			                                                       .flags = 0,
			                                                       .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			                                                                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
			                                                       .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			                                                                      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			                                                                      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
			                                                       .pfnUserCallback = &debugCallback,
			                                                       .pUserData = nullptr };

		const VkInstanceCreateInfo instanceCI{ .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			                                   .pNext = debugUtilsAvailable ? &debugMessengerCI : nullptr,
			                                   .flags = flags,
			                                   .pApplicationInfo = &appInfo,
			                                   .enabledLayerCount = static_cast<uint32_t>(layers.size()),
			                                   .ppEnabledLayerNames = layers.data(),
			                                   .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
			                                   .ppEnabledExtensionNames = extensions.data() };

		vkCheck(vkCreateInstance(&instanceCI, nullptr, &m_Instance), "vkCreateInstance");

		volkLoadInstance(m_Instance);
		CBK_DEBUG("Vulkan Instance created");
	}

	std::vector<VkLayerProperties> VulkanInstance::getAvailableLayers() {
		uint32_t layerPropCount = 0;
		vkCheck(vkEnumerateInstanceLayerProperties(&layerPropCount, nullptr), "vkEnumerateInstanceLayerProperties");
		std::vector<VkLayerProperties> availableLayers(layerPropCount);
		vkCheck(vkEnumerateInstanceLayerProperties(&layerPropCount, availableLayers.data()), "vkEnumerateInstanceLayerProperties");

		return availableLayers;
	}

	std::vector<VkExtensionProperties> VulkanInstance::getAvailableExtensions() {
		uint32_t extensionsCount = 0;
		vkCheck(vkEnumerateInstanceExtensionProperties(nullptr, &extensionsCount, nullptr), "vkEnumerateInstanceExtensionProperties");
		std::vector<VkExtensionProperties> availableExtensions(extensionsCount);
		vkCheck(vkEnumerateInstanceExtensionProperties(nullptr, &extensionsCount, availableExtensions.data()),
		        "vkEnumerateInstanceExtensionProperties");

		return availableExtensions;
	}

	void VulkanInstance::createDebugMessenger() {
		if (vkCreateDebugUtilsMessengerEXT == nullptr) {
			CBK_WARN("VK_EXT_debug_utils unavailable, validation messages will not be reported");
			return;
		}

		const VkDebugUtilsMessengerCreateInfoEXT debugMessengerCI{ .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
			                                                       .pNext = nullptr,
			                                                       .flags = 0,
			                                                       .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			                                                                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
			                                                       .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			                                                                      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			                                                                      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
			                                                       .pfnUserCallback = &debugCallback,
			                                                       .pUserData = nullptr };
		vkCheck(vkCreateDebugUtilsMessengerEXT(m_Instance, &debugMessengerCI, nullptr, &m_DebugMessenger),
		        "vkCreateDebugUtilsMessengerEXT");

		CBK_DEBUG("Vulkan Debug Messenger created");
	}

	void VulkanInstance::createSurface() {
		bool surfaceCreated = SDL_Vulkan_CreateSurface(m_WindowHandle, m_Instance, nullptr, &m_Surface);
		if (!surfaceCreated)
			CBK_ERROR("Couldn't create Vulkan Surface!");
	}
} // namespace lab::vk

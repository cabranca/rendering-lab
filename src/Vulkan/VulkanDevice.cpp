#include <format>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#define VOLK_IMPLEMENTATION
#include <volk/volk.h>
#include "vulkan/vk_enum_string_helper.h"

#include "VulkanDevice.h"

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

	VulkanDevice::VulkanDevice(const Window& window) : m_WindowHandle(window.getWindowHandle()), m_MemoryProperties() {
		createInstance();
		createDebugMessenger();
		createSurface();
		pickPhysicalDevice();
		createLogicalDevice();
		m_Queue = VulkanQueue(m_Device, m_QueueFamilyIndex);
	}

	VulkanDevice::~VulkanDevice() {
        if (m_Device != VK_NULL_HANDLE) {
			vkDestroyDevice(m_Device, nullptr);
			m_Device = VK_NULL_HANDLE;
			CBK_DEBUG("Vulkan Device destroyed");
		}

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

    void VulkanDevice::waitIdle() {
        vkDeviceWaitIdle(m_Device);
    }

	VkPhysicalDevice VulkanDevice::getPhysicalDevice() const {
		return m_PhysicalDevice;
	}

	VkDevice VulkanDevice::getDevice() const {
		return m_Device;
	}

	const VulkanQueue& VulkanDevice::getQueue() const {
		return m_Queue;
	}

	VkSurfaceKHR VulkanDevice::getSurface() const {
		return m_Surface;
	}

	VkPhysicalDeviceMemoryProperties VulkanDevice::getMemoryProperties() const {
		return m_MemoryProperties;
	}

	VkSampleCountFlagBits VulkanDevice::getMSAA() const {
		return m_MSAASamples;
	}

	uint32_t VulkanDevice::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
		for (uint32_t i = 0; i < m_MemoryProperties.memoryTypeCount; i++) {
			if ((typeFilter & (1 << i)) && (m_MemoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
				return i;
		}

		CBK_FATAL("Failed to find suitable memory type!");
		return UINT32_MAX;
	}

    void VulkanDevice::createInstance() {
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

	std::vector<VkLayerProperties> VulkanDevice::getAvailableLayers() {
		uint32_t layerPropCount = 0;
		vkCheck(vkEnumerateInstanceLayerProperties(&layerPropCount, nullptr), "vkEnumerateInstanceLayerProperties");
		std::vector<VkLayerProperties> availableLayers(layerPropCount);
		vkCheck(vkEnumerateInstanceLayerProperties(&layerPropCount, availableLayers.data()), "vkEnumerateInstanceLayerProperties");

		return availableLayers;
	}

	std::vector<VkExtensionProperties> VulkanDevice::getAvailableExtensions() {
		uint32_t extensionsCount = 0;
		vkCheck(vkEnumerateInstanceExtensionProperties(nullptr, &extensionsCount, nullptr), "vkEnumerateInstanceExtensionProperties");
		std::vector<VkExtensionProperties> availableExtensions(extensionsCount);
		vkCheck(vkEnumerateInstanceExtensionProperties(nullptr, &extensionsCount, availableExtensions.data()),
		        "vkEnumerateInstanceExtensionProperties");

		return availableExtensions;
	}

	void VulkanDevice::createDebugMessenger() {
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

	void VulkanDevice::createSurface() {
		bool surfaceCreated = SDL_Vulkan_CreateSurface(m_WindowHandle, m_Instance, nullptr, &m_Surface);
		if (!surfaceCreated)
			CBK_ERROR("Couldn't create Vulkan Surface!");
	}

	void VulkanDevice::pickPhysicalDevice() {
		uint32_t deviceCount = 0;
		vkCheck(vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr), "vkEnumeratePhysicalDevices");
		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkCheck(vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data()), "vkEnumeratePhysicalDevices");

		for (const auto& device: devices) {
			VkPhysicalDeviceProperties props;
			vkGetPhysicalDeviceProperties(device, &props);

			if (props.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && props.deviceType != VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
				CBK_DEBUG("{}: not a discrete or integrated GPU", props.deviceName);
				continue;
			}
			if (props.apiVersion < VK_API_VERSION_1_4) {
				CBK_DEBUG("{}: VK API version is less than 1.4", props.deviceName);
				continue;
			}

			uint32_t qFamilyPropCount = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(device, &qFamilyPropCount, nullptr);
			std::vector<VkQueueFamilyProperties> qFamilyProps(qFamilyPropCount);
			vkGetPhysicalDeviceQueueFamilyProperties(device, &qFamilyPropCount, qFamilyProps.data());

			bool supportsGraphics = false;
			for (uint32_t i = 0; i < qFamilyPropCount; i++) {
				const auto& qFamily = qFamilyProps[i];
				VkBool32 supportsSurface = VK_FALSE;
				vkCheck(vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_Surface, &supportsSurface),
				        "vkGetPhysicalDeviceSrufaceSupportKHR");
				if ((qFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 && supportsSurface == VK_TRUE) {
					supportsGraphics = true;
					m_QueueFamilyIndex = i;
					break;
				}
			}

			if (!supportsGraphics) {
				CBK_DEBUG("{}: no graphics queue", props.deviceName);
				continue;
			}

			std::vector<const char*> requiredDeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
			uint32_t extensionPropCount = 0;
			vkCheck(vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionPropCount, nullptr),
			        "vkEnumerateDeviceExtensionProperties");
			std::vector<VkExtensionProperties> deviceExtensions(extensionPropCount);
			vkCheck(vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionPropCount, deviceExtensions.data()),
			        "vkEnumerateDeviceExtensionProperties");

			bool supportsExtensions = true;
			for (const auto& required: requiredDeviceExtensions) {
				bool supportsExtension = false;
				for (const auto& available: deviceExtensions) {
					if (std::strcmp(required, available.extensionName) == 0)
						supportsExtension = true;
				}
				if (!supportsExtension) {
					CBK_DEBUG("{}: missing extension {}", props.deviceName, required);
					supportsExtensions = false;
				}
			}
			if (!supportsExtensions)
				continue;

			// Chained so one query fills all three; each struct is zero-initialized because a driver
			// leaves any sType it does not recognize untouched.
			VkPhysicalDeviceVulkan13Features vk13Features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
			VkPhysicalDeviceVulkan11Features vk11Features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
				                                           .pNext = &vk13Features };
			VkPhysicalDeviceFeatures2 features2{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &vk11Features };
			vkGetPhysicalDeviceFeatures2(device, &features2);

			if (features2.features.tessellationShader != VK_TRUE) {
				CBK_DEBUG("{}: no tessellation shader support", props.deviceName);
				continue;
			}
			if (vk11Features.shaderDrawParameters != VK_TRUE) {
				CBK_DEBUG("{}: no shader draw parameters support", props.deviceName);
				continue;
			}
			if (vk13Features.dynamicRendering != VK_TRUE) {
				CBK_DEBUG("{}: no dynamic rendering support", props.deviceName);
				continue;
			}

			m_PhysicalDevice = device;
			CBK_INFO("Physical Device: {}", props.deviceName);
			break;
		}

		if (m_PhysicalDevice == VK_NULL_HANDLE) {
			CBK_ERROR("No physical device met the requirements!");
		}
		else
			m_MSAASamples = getMaxUsableSampleCount();
	}

	VkSampleCountFlagBits VulkanDevice::getMaxUsableSampleCount() {
		VkPhysicalDeviceProperties prop;
		vkGetPhysicalDeviceProperties(m_PhysicalDevice, &prop);
		// Each sample count is a single bit, so every count up to and including the cap is (k_MaxMSAA << 1) - 1.
		constexpr VkSampleCountFlags allowed = (static_cast<VkSampleCountFlags>(k_MaxMSAA) << 1) - 1;
		VkSampleCountFlags counts = prop.limits.framebufferColorSampleCounts & prop.limits.framebufferDepthSampleCounts & allowed;
		if (counts & VK_SAMPLE_COUNT_64_BIT)
			return VK_SAMPLE_COUNT_64_BIT;
		if (counts & VK_SAMPLE_COUNT_32_BIT)
			return VK_SAMPLE_COUNT_32_BIT;
		if (counts & VK_SAMPLE_COUNT_16_BIT)
			return VK_SAMPLE_COUNT_16_BIT;
		if (counts & VK_SAMPLE_COUNT_8_BIT)
			return VK_SAMPLE_COUNT_8_BIT;
		if (counts & VK_SAMPLE_COUNT_4_BIT)
			return VK_SAMPLE_COUNT_4_BIT;
		if (counts & VK_SAMPLE_COUNT_2_BIT)
			return VK_SAMPLE_COUNT_2_BIT;

		return VK_SAMPLE_COUNT_1_BIT;
	}

	void VulkanDevice::createLogicalDevice() {
		float queuePriority = 0.5F;
		VkDeviceQueueCreateInfo queueCI{ .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			                             .pNext = nullptr,
			                             .flags = 0,
			                             .queueFamilyIndex = m_QueueFamilyIndex,
			                             .queueCount = 1,
			                             .pQueuePriorities = &queuePriority };

		VkPhysicalDeviceVulkan13Features vk13Features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
			                                           .synchronization2 = VK_TRUE , .dynamicRendering = VK_TRUE};
		VkPhysicalDeviceVulkan11Features vk11Features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
			                                           .pNext = &vk13Features, .shaderDrawParameters = VK_TRUE };
		VkPhysicalDeviceFeatures2 features2{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
			                                 .pNext = &vk11Features,
			                                 .features{ .sampleRateShading = VK_TRUE, .samplerAnisotropy = VK_TRUE } };

		std::vector<const char*> requiredDeviceExtension = { VK_KHR_SWAPCHAIN_EXTENSION_NAME, "VK_KHR_portability_subset" };
		VkDeviceCreateInfo deviceCI{ .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			                         .pNext = &features2,
			                         .flags = 0,
			                         .queueCreateInfoCount = 1,
			                         .pQueueCreateInfos = &queueCI,
			                         .enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtension.size()),
			                         .ppEnabledExtensionNames = requiredDeviceExtension.data(),
			                         .pEnabledFeatures = nullptr };
		vkCheck(vkCreateDevice(m_PhysicalDevice, &deviceCI, nullptr, &m_Device), "vkCreateDevice");
		volkLoadDevice(m_Device);
		CBK_DEBUG("Vulkan Logical Device created");
	}
} // namespace lab::vk
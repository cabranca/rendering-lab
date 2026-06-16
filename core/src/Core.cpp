#include "Core.h"

#include <SDL3/SDL_vulkan.h>
#include <array>
#include <print>
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

	Core::~Core() {
		shutdown();
	}

	void Core::init(std::string_view appName, SDL_Window* window) {
		m_Instance.init(appName);
		createDebugCallback();
		createSurface(window);
		m_DeviceManager.init(m_Instance.getInstance(), m_Surface);
		m_QueueFamily = m_DeviceManager.selectDevice(VK_QUEUE_GRAPHICS_BIT, true);
		createDevice();
		m_Swapchain.init(m_Device, m_DeviceManager.getSelectedDevice(), m_Surface, m_QueueFamily);
		createCommandPool();
		m_Queue.init(m_Device, m_Swapchain.getSwapchain(), m_QueueFamily, 0);
	}

	void Core::shutdown() {
		m_Queue.shutdown();

		vkDestroyCommandPool(m_Device, m_CmdPool, nullptr);
		std::println("Command Pool destroyed");

		m_Swapchain.shutdown(m_Device);

		vkDestroyDevice(m_Device, nullptr);
		std::println("Vulkan Device destroyed");

		vkDestroySurfaceKHR(m_Instance.getInstance(), m_Surface, nullptr);
		std::println("SDL Surface destroyed");

		vkDestroyDebugUtilsMessengerEXT(m_Instance.getInstance(), m_DebugMessenger, nullptr);
		std::println("Debug messenger destroyed");

		m_Instance.shutdown();
	}

	uint32_t Core::getNumImages() const {
		return m_Swapchain.getNumImages();
	}

	VkImage Core::getImage(uint32_t index) const {
		return m_Swapchain.getImage(index);
	}

	void Core::createCommandBuffers(uint32_t numImages, VkCommandBuffer* cmdBuffers) {
		VkCommandBufferAllocateInfo cmdBufferAI {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = m_CmdPool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = numImages
		};

		chk(vkAllocateCommandBuffers(m_Device, &cmdBufferAI, cmdBuffers));
		std::println("Command Buffers allocated");
	}

	void Core::freeCommandBuffers(uint32_t bufferCount, const VkCommandBuffer* cmdBuffers) {
		vkFreeCommandBuffers(m_Device, m_CmdPool, bufferCount, cmdBuffers);
	}

	Queue* Core::getQueue() {
		return &m_Queue;
	}

	void Core::beginCommandBuffer(VkCommandBuffer buffer, VkCommandBufferUsageFlags flags) {
		VkCommandBufferBeginInfo bufferBI{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = flags,
			.pInheritanceInfo = nullptr
		};

		chk(vkBeginCommandBuffer(buffer, &bufferBI));
	}

	void Core::createDebugCallback() {
		VkDebugUtilsMessengerCreateInfoEXT MessengerCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
			.pNext = NULL,
			.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
			.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			               VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
			.pfnUserCallback = DebugCallback,
			.pUserData = NULL
		};

		chk(vkCreateDebugUtilsMessengerEXT(m_Instance.getInstance(), &MessengerCreateInfo, nullptr, &m_DebugMessenger));

		std::println("Debug utils created");
	}

	void Core::createSurface(SDL_Window* window) {
		if (!SDL_Vulkan_CreateSurface(window, m_Instance.getInstance(), nullptr, &m_Surface)) {
			std::println("SDL_Vulkan_CreateSurface failed: {}", SDL_GetError());
			throw std::runtime_error("Failed to create Vulkan surface");
		}
		std::println("SDL Surface created");
	}

	void Core::createDevice() {
		float qPriorities{ 0.f };
		VkQueue queue;
		VkDeviceQueueCreateInfo queueCI{ .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			                             .queueFamilyIndex = m_QueueFamily,
			                             .queueCount = 1,
			                             .pQueuePriorities = &qPriorities };

		std::array<const char*, 2> ext{ VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME };
		VkPhysicalDeviceFeatures feat{ /*.geometryShader = true, */.tessellationShader = true }; // Geometry shader not available in Macbook Air M5

		const auto& physicalDevice = m_DeviceManager.getSelectedDevice();
		// if (physicalDevice.Features.geometryShader != VK_TRUE) {
		// 	std::println("Geometry Shader not supported!");
		// 	exit(1);
		// }
		if (physicalDevice.Features.tessellationShader != VK_TRUE) {
			std::println("Tesselation Shader not supported!");
			exit(1);
		}

		VkDeviceCreateInfo deviceCI{ .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			                         .queueCreateInfoCount = 1,
			                         .pQueueCreateInfos = &queueCI,
			                         .enabledExtensionCount = ext.size(),
			                         .ppEnabledExtensionNames = ext.data(),
			                         .pEnabledFeatures = &feat };

		chk(vkCreateDevice(physicalDevice.Device, &deviceCI, nullptr, &m_Device));
		volkLoadDevice(m_Device);
		std::println("Vulkan Device created");
	}

	void Core::createCommandPool() {
		VkCommandPoolCreateInfo poolCI {
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = 0,
			.queueFamilyIndex = m_QueueFamily
		};

		chk(vkCreateCommandPool(m_Device, &poolCI, nullptr, &m_CmdPool));
		std::println("Command Pool created");
	}
} // namespace lab
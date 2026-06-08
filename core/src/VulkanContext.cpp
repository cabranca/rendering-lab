// This translation unit hosts the single implementation of volk and VMA for the
// whole library. The implementation macros must be defined before the headers
// they back are first included, so they live at the very top of this file.
#define VOLK_IMPLEMENTATION
#include <volk/volk.h>

// We load Vulkan entry points dynamically through volk (VK_NO_PROTOTYPES), so
// VMA must fetch its functions dynamically too rather than link them statically.
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"

#include "VulkanContext.h"

#include <vector>
#include <iostream>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include "Check.h"

namespace lab {

	void VulkanContext::init(std::string_view applicationName) {
		chk(SDL_Init(SDL_INIT_VIDEO));
		chk(SDL_Vulkan_LoadLibrary(NULL));
		volkInitialize();

		// Instance
		VkApplicationInfo appInfo{ .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			                       .pApplicationName = applicationName.data(),
			                       .apiVersion = VK_API_VERSION_1_3 };
		uint32_t instanceExtensionsCount{ 0 };
		char const* const* instanceExtensions{ SDL_Vulkan_GetInstanceExtensions(&instanceExtensionsCount) };
		VkInstanceCreateInfo instanceCI{
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pApplicationInfo = &appInfo,
			.enabledExtensionCount = instanceExtensionsCount,
			.ppEnabledExtensionNames = instanceExtensions,
		};
		chk(vkCreateInstance(&instanceCI, nullptr, &m_Instance));
		volkLoadInstance(m_Instance);

		// Device
		uint32_t deviceCount{ 0 };
		chk(vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr));
		std::vector<VkPhysicalDevice> devices(deviceCount);
		chk(vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data()));
		m_PhysicalDevice = devices.at(0);
		VkPhysicalDeviceProperties2 deviceProperties{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
		vkGetPhysicalDeviceProperties2(m_PhysicalDevice, &deviceProperties);
		std::cout << "Selected m_Device: " << deviceProperties.properties.deviceName << "\n";

		// Find a queue family for graphics
		uint32_t queueFamilyCount{ 0 };
		vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, nullptr);
		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, queueFamilies.data());
		for (size_t i = 0; i < queueFamilies.size(); i++) {
			if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				m_QueueFamily = i;
				break;
			}
		}
		chk(SDL_Vulkan_GetPresentationSupport(m_Instance, m_PhysicalDevice, m_QueueFamily));

		// Logical m_Device
		const float qfpriorities{ 1.0f };
		VkDeviceQueueCreateInfo queueCI{ .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			                             .queueFamilyIndex = m_QueueFamily,
			                             .queueCount = 1,
			                             .pQueuePriorities = &qfpriorities };
		VkPhysicalDeviceVulkan12Features enabledVk12Features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
			                                                  .descriptorIndexing = true,
			                                                  .shaderSampledImageArrayNonUniformIndexing = true,
			                                                  .descriptorBindingVariableDescriptorCount = true,
			                                                  .runtimeDescriptorArray = true,
			                                                  .bufferDeviceAddress = true };
		VkPhysicalDeviceVulkan13Features enabledVk13Features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
			                                                  .pNext = &enabledVk12Features,
			                                                  .synchronization2 = true,
			                                                  .dynamicRendering = true };
		VkPhysicalDeviceFeatures enabledVk10Features{ .samplerAnisotropy = VK_TRUE };
		const std::vector<const char*> deviceExtensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };
		VkDeviceCreateInfo deviceCI{ .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			                         .pNext = &enabledVk13Features,
			                         .queueCreateInfoCount = 1,
			                         .pQueueCreateInfos = &queueCI,
			                         .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
			                         .ppEnabledExtensionNames = deviceExtensions.data(),
			                         .pEnabledFeatures = &enabledVk10Features };
		chk(vkCreateDevice(m_PhysicalDevice, &deviceCI, nullptr, &m_Device));
		volkLoadDevice(m_Device);
		vkGetDeviceQueue(m_Device, m_QueueFamily, 0, &m_Queue);

		// VMA
		VmaVulkanFunctions vkFunctions{ .vkGetInstanceProcAddr = vkGetInstanceProcAddr, .vkGetDeviceProcAddr = vkGetDeviceProcAddr };
		VmaAllocatorCreateInfo allocatorCI{ .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
			                                .physicalDevice = m_PhysicalDevice,
			                                .device = m_Device,
			                                .pVulkanFunctions = &vkFunctions,
			                                .instance = m_Instance };
		chk(vmaCreateAllocator(&allocatorCI, &m_Allocator));
	}

	void VulkanContext::submitImmediate(const std::function<void(VkCommandBuffer)>& record) const {
		VkCommandPoolCreateInfo poolCI{ .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			                            .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
			                            .queueFamilyIndex = m_QueueFamily };
		VkCommandPool pool{ VK_NULL_HANDLE };
		chk(vkCreateCommandPool(m_Device, &poolCI, nullptr, &pool));

		VkCommandBufferAllocateInfo cbAI{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			                              .commandPool = pool,
			                              .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			                              .commandBufferCount = 1 };
		VkCommandBuffer cb{ VK_NULL_HANDLE };
		chk(vkAllocateCommandBuffers(m_Device, &cbAI, &cb));

		VkCommandBufferBeginInfo beginInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			                                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
		chk(vkBeginCommandBuffer(cb, &beginInfo));
		record(cb);
		chk(vkEndCommandBuffer(cb));

		VkFenceCreateInfo fenceCI{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
		VkFence fence{ VK_NULL_HANDLE };
		chk(vkCreateFence(m_Device, &fenceCI, nullptr, &fence));
		VkSubmitInfo submitInfo{ .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cb };
		chk(vkQueueSubmit(m_Queue, 1, &submitInfo, fence));
		chk(vkWaitForFences(m_Device, 1, &fence, VK_TRUE, UINT64_MAX));

		vkDestroyFence(m_Device, fence, nullptr);
		vkDestroyCommandPool(m_Device, pool, nullptr);
	}

	void VulkanContext::shutdown() {
		vmaDestroyAllocator(m_Allocator);
		vkDestroyDevice(m_Device, nullptr);
		vkDestroyInstance(m_Instance, nullptr);
		SDL_Vulkan_UnloadLibrary();
		SDL_Quit();
	}
} // namespace lab

#include <cstring>

#define VOLK_IMPLEMENTATION
#include <volk/volk.h>

#include "VulkanDevice.h"

#include "Logger.h"
#include "Utils.h"

namespace lab::vk {

	VulkanDevice::DeviceHandle::~DeviceHandle() {
		if (device != VK_NULL_HANDLE) {
			vkDestroyDevice(device, nullptr);
			CBK_DEBUG("Vulkan Device destroyed");
		}
	}

	VulkanDevice::VulkanDevice(const VulkanInstance& instance) : m_Instance(&instance) {
		pickPhysicalDevice();
		createLogicalDevice();
		m_Queue = VulkanQueue(m_DeviceHandle.device, m_QueueFamilyIndex);
	}

	void VulkanDevice::waitIdle() {
		vkDeviceWaitIdle(m_DeviceHandle.device);
	}

	VkPhysicalDevice VulkanDevice::getPhysicalDevice() const {
		return m_PhysicalDevice;
	}

	VkDevice VulkanDevice::getDevice() const {
		return m_DeviceHandle.device;
	}

	const VulkanQueue& VulkanDevice::getQueue() const {
		return m_Queue;
	}

	VkSurfaceKHR VulkanDevice::getSurface() const {
		return m_Instance->getSurface();
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

	VkDeviceMemory VulkanDevice::allocateMemory(const VkMemoryRequirements& requirements, VkMemoryPropertyFlags properties) const {
		VkMemoryAllocateInfo memAI{ .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			                        .pNext = nullptr,
			                        .allocationSize = requirements.size,
			                        .memoryTypeIndex = findMemoryType(requirements.memoryTypeBits, properties) };

		VkDeviceMemory memory = VK_NULL_HANDLE;
		vkCheck(vkAllocateMemory(m_DeviceHandle.device, &memAI, nullptr, &memory), "vkAllocateMemory");
		return memory;
	}

	void VulkanDevice::pickPhysicalDevice() {
		uint32_t deviceCount = 0;
		vkCheck(vkEnumeratePhysicalDevices(m_Instance->getInstance(), &deviceCount, nullptr), "vkEnumeratePhysicalDevices");
		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkCheck(vkEnumeratePhysicalDevices(m_Instance->getInstance(), &deviceCount, devices.data()), "vkEnumeratePhysicalDevices");

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
				vkCheck(vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_Instance->getSurface(), &supportsSurface),
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
			vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &m_MemoryProperties);
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

		std::vector<const char*> requiredDeviceExtension = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

		// VK_KHR_portability_subset must be enabled whenever a device advertises it, but requesting it
		// on a non-portability driver (native Linux/Windows) is invalid — so add it only when present.
		uint32_t extensionCount = 0;
		vkCheck(vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &extensionCount, nullptr),
		        "vkEnumerateDeviceExtensionProperties");
		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkCheck(vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &extensionCount, availableExtensions.data()),
		        "vkEnumerateDeviceExtensionProperties");
		for (const auto& extension: availableExtensions) {
			if (std::strcmp(extension.extensionName, "VK_KHR_portability_subset") == 0) {
				requiredDeviceExtension.emplace_back("VK_KHR_portability_subset");
				break;
			}
		}

		VkDeviceCreateInfo deviceCI{ .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			                         .pNext = &features2,
			                         .flags = 0,
			                         .queueCreateInfoCount = 1,
			                         .pQueueCreateInfos = &queueCI,
			                         .enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtension.size()),
			                         .ppEnabledExtensionNames = requiredDeviceExtension.data(),
			                         .pEnabledFeatures = nullptr };
		vkCheck(vkCreateDevice(m_PhysicalDevice, &deviceCI, nullptr, &m_DeviceHandle.device), "vkCreateDevice");
		volkLoadDevice(m_DeviceHandle.device);
		CBK_DEBUG("Vulkan Logical Device created");
	}
} // namespace lab::vk

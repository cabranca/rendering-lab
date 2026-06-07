#include "VulkanContext.h"

#include <vulkan/vulkan.h>
#include <volk/volk.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <iostream>

namespace lab {

    static inline void chk(VkResult result) {
        if (result != VK_SUCCESS) {
            std::cerr << "Vulkan call returned an error (" << result << ")\n";
            throw std::exception();
        }
    }

    static inline void chk(bool result) {
        if (!result) {
            std::cerr << "Call returned an error\n";
            exit(result);
        }
    }

    void VulkanContext::init() {
        chk(SDL_Init(SDL_INIT_VIDEO));
		chk(SDL_Vulkan_LoadLibrary(NULL));
		volkInitialize();


        // Instance
        VkApplicationInfo appInfo{ .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .pApplicationName = "How to Vulkan", .apiVersion = VK_API_VERSION_1_3 };
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
        uint32_t queueFamily{ 0 };
        for (size_t i = 0; i < queueFamilies.size(); i++) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                queueFamily = i;
                break;
            }
        }
        chk(SDL_Vulkan_GetPresentationSupport(m_Instance, m_PhysicalDevice, queueFamily));
        
        
        // Logical m_Device
        const float qfpriorities{ 1.0f };
        VkDeviceQueueCreateInfo queueCI{ .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueFamilyIndex = queueFamily, .queueCount = 1, .pQueuePriorities = &qfpriorities };
        VkPhysicalDeviceVulkan12Features enabledVk12Features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, .descriptorIndexing = true, .shaderSampledImageArrayNonUniformIndexing = true, .descriptorBindingVariableDescriptorCount = true, .runtimeDescriptorArray = true, .bufferDeviceAddress = true };
        VkPhysicalDeviceVulkan13Features enabledVk13Features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES, .pNext = &enabledVk12Features, .synchronization2 = true, .dynamicRendering = true };
        VkPhysicalDeviceFeatures enabledVk10Features{ .samplerAnisotropy = VK_TRUE };
        const std::vector<const char*> deviceExtensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        VkDeviceCreateInfo deviceCI{
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = &enabledVk13Features,
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &queueCI,
            .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
            .ppEnabledExtensionNames = deviceExtensions.data(),
            .pEnabledFeatures = &enabledVk10Features
        };
        chk(vkCreateDevice(m_PhysicalDevice, &deviceCI, nullptr, &m_Device));
        vkGetDeviceQueue(m_Device, queueFamily, 0, &m_Queue);
        
        
        // VMA
        VmaVulkanFunctions vkFunctions{ .vkGetInstanceProcAddr = vkGetInstanceProcAddr, .vkGetDeviceProcAddr = vkGetDeviceProcAddr, .vkCreateImage = vkCreateImage };
        VmaAllocatorCreateInfo allocatorCI{ .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT, .physicalDevice = m_PhysicalDevice, .device = m_Device, .pVulkanFunctions = &vkFunctions, .instance = m_Instance };
        chk(vmaCreateAllocator(&allocatorCI, &m_Allocator));
    }

    void VulkanContext::shutdown() {
        vmaDestroyAllocator(m_Allocator);
        SDL_DestroyWindow(window);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        SDL_Quit();
        vkDestroyDevice(m_Device, nullptr);
        vkDestroyInstance(m_Instance, nullptr);
    }
}
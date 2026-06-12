#include "PhysicalDevice.h"

#include <iostream>
#include <vector>

#include <volk/volk.h>
#include <SDL3/SDL_vulkan.h>

namespace lab {

	void PhysicalDevice::init(VkInstance instance) {
        uint32_t deviceCount{ 0 };
        std::vector<VkPhysicalDevice> devices;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        devices.resize(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        for (auto device : devices) {
            uint32_t propertyCount{ 0 };
            std::vector<VkQueueFamilyProperties> properties;
            vkGetPhysicalDeviceQueueFamilyProperties(device, &propertyCount, nullptr);
            properties.resize(propertyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(device, &propertyCount, properties.data());
        }
        uint32_t queueFamily{ 0 };
	}

	void PhysicalDevice::shutdown() {
		std::cout << "\n";
	}

	VkPhysicalDevice PhysicalDevice::getSelectedDevice() const {
		return m_SelectedDevice;
	}
} // namespace lab
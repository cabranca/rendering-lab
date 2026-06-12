#include "DeviceManager.h"
#include "Check.h"

#include <volk/volk.h>

namespace lab {

	void DeviceManager::init(VkInstance instance, VkSurfaceKHR& surface) {
		uint32_t deviceCount{ 0 };
		std::vector<VkPhysicalDevice> devices;
		chk(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr));
		devices.resize(deviceCount);
		m_Devices.resize(deviceCount);
		chk(vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()));

		for (size_t i = 0; i < deviceCount; i++) {
			PhysicalDevice phDevice;
			vkGetPhysicalDeviceProperties(devices.at(i), &m_Devices[i].DeviceProperties);

			std::println("Device name: {}", m_Devices.at(i).DeviceProperties.deviceName);
			uint32_t apiVersion = m_Devices.at(i).DeviceProperties.apiVersion;
			std::println("    API VERSION: {}.{}.{}.{}", VK_API_VERSION_VARIANT(apiVersion), VK_API_VERSION_MAJOR(apiVersion),
			             VK_API_VERSION_MINOR(apiVersion), VK_API_VERSION_PATCH(apiVersion));

			uint32_t queueFamilyCount{ 0 };
			vkGetPhysicalDeviceQueueFamilyProperties(devices.at(i), &queueFamilyCount, nullptr);
			m_Devices.at(i).FamilyProperties.resize(queueFamilyCount);
			m_Devices.at(i).SupportsPresent.resize(queueFamilyCount);
			vkGetPhysicalDeviceQueueFamilyProperties(devices.at(i), &queueFamilyCount, m_Devices.at(i).FamilyProperties.data());

			for (uint32_t q = 0; q < queueFamilyCount; q++) {
				const auto& familyProperties = m_Devices.at(i).FamilyProperties.at(q);
				std::println("    Family {} Num queues: {} ", q, familyProperties.queueCount);
				VkQueueFlags flags = familyProperties.queueFlags;
				std::println("    GFX {}, Compute {}, Transfer {}, Sparse binding {}", (flags & VK_QUEUE_GRAPHICS_BIT) ? "Yes" : "No",
				             (flags & VK_QUEUE_COMPUTE_BIT) ? "Yes" : "No", (flags & VK_QUEUE_TRANSFER_BIT) ? "Yes" : "No",
				             (flags & VK_QUEUE_SPARSE_BINDING_BIT) ? "Yes" : "No");

				chk(vkGetPhysicalDeviceSurfaceSupportKHR(devices.at(i), q, surface, &m_Devices.at(i).SupportsPresent.at(q)));
			}

			uint32_t formatCount = 0;
			chk(vkGetPhysicalDeviceSurfaceFormatsKHR(devices.at(i), surface, &formatCount, nullptr));
			m_Devices.at(i).SurfaceFormats.resize(formatCount);
			chk(vkGetPhysicalDeviceSurfaceFormatsKHR(devices.at(i), surface, &formatCount, m_Devices.at(i).SurfaceFormats.data()));

			chk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(devices.at(i), surface, &m_Devices.at(i).SurfaceCaps));

			uint32_t presentModesCount;
			chk(vkGetPhysicalDeviceSurfacePresentModesKHR(devices.at(i), surface, &presentModesCount, nullptr));
			m_Devices.at(i).presentModes.resize(presentModesCount);
			chk(vkGetPhysicalDeviceSurfacePresentModesKHR(devices.at(i), surface, &presentModesCount, m_Devices.at(i).presentModes.data()));

			vkGetPhysicalDeviceMemoryProperties(devices.at(i), &m_Devices.at(i).MemProps);
		}
	}

	void DeviceManager::shutdown() {}

	uint32_t DeviceManager::selectDevice(VkQueueFlags requiredQueueType, bool supportsPresent) {
		for (uint32_t i = 0; i < m_Devices.size(); i++) {
			for (uint32_t j = 0; j < m_Devices.at(i).FamilyProperties.size(); j++) {
				const auto& qFamProp = m_Devices.at(i).FamilyProperties.at(j);
				if ((qFamProp.queueFlags & requiredQueueType) &&
				    (static_cast<bool>(m_Devices.at(i).SupportsPresent.at(j) == supportsPresent))) {
					m_SelectedIndex = i;
					return j;
				}
			}
		}

		std::println("Required queue type {0} and supports present {1} not found\n", requiredQueueType, supportsPresent);

		return 0;
	}

	VkPhysicalDevice DeviceManager::getSelectedDevice() const {
		return m_Devices.at(m_SelectedIndex).Device;
	}
} // namespace lab
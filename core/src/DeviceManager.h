#pragma once

#include "vulkan/vulkan_core.h"
#include <vector>

namespace lab {

	struct PhysicalDevice {
		VkPhysicalDevice Device;
		VkPhysicalDeviceProperties DeviceProperties;
		std::vector<VkQueueFamilyProperties> FamilyProperties;
		std::vector<VkBool32> SupportsPresent;
		std::vector<VkSurfaceFormatKHR> SurfaceFormats;
		VkSurfaceCapabilitiesKHR SurfaceCaps;
		VkPhysicalDeviceMemoryProperties MemProps;
		std::vector<VkPresentModeKHR> presentModes;
	};

	class DeviceManager {
	  public:
		void init(VkInstance instance, VkSurfaceKHR& surface);
		void shutdown();

		uint32_t selectDevice(VkQueueFlags requiredQueueType, bool supportsPresent);
		VkPhysicalDevice getSelectedDevice() const;

	  private:
		std::vector<PhysicalDevice> m_Devices;
		uint32_t m_SelectedIndex;
	};
} // namespace lab
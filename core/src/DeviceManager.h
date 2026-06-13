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
		std::vector<VkPresentModeKHR> PresentModes;
		VkPhysicalDeviceFeatures Features;
	};

	class DeviceManager {
	  public:
		void init(VkInstance instance, VkSurfaceKHR& surface);

		// Returns the queue family index (should be handled differently in the future)
		uint32_t selectDevice(VkQueueFlags requiredQueueType, bool supportsPresent);
		const PhysicalDevice& getSelectedDevice() const;

	  private:
		std::vector<PhysicalDevice> m_Devices;
		uint32_t m_SelectedIndex;
	};
} // namespace lab
#pragma once

#include "vulkan/vulkan_core.h"

namespace lab {

	class PhysicalDevice {
	  public:
		void init(VkInstance instance);
		void shutdown();

		VkPhysicalDevice getSelectedDevice() const;

	  private:
		VkPhysicalDevice m_SelectedDevice;
	};
} // namespace lab
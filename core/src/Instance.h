#pragma once

#include "vulkan/vulkan_core.h"
#include <string_view>

namespace lab {

	class Instance {
	  public:
		void init(std::string_view);
		void shutdown();

		VkInstance getInstance() const;

	  private:
		VkInstance m_Instance{ VK_NULL_HANDLE };
	};
} // namespace lab
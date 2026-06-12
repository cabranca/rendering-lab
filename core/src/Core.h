#pragma once

#include <string_view>

#include "Instance.h"

namespace lab {

	class Core {
	  public:
		void init(std::string_view appName);
		void shutdown();

	  private:
		Instance m_Instance;
		VkDebugUtilsMessengerEXT m_DebugMessenger{ VK_NULL_HANDLE };

		void createDebugCallback();
	};
} // namespace lab
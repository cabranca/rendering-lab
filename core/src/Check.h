#pragma once

#include <print>

#include "vulkan/vulkan_core.h"

namespace lab {

	// Throws on a failed Vulkan call. Kept header-only so every translation unit
	// can use the same check without an extra link dependency.
	static inline void chk(VkResult result) {
		if (result != VK_SUCCESS) {
			std::println("Vulkan call returned an error ({})", static_cast<int>(result));
			throw std::runtime_error("Vulkan call failed");
		}
	}

	static inline void chk(bool result) {
		if (!result) {
			std::println("Call returned an error ({})", result);
			throw std::runtime_error("Call failed");
		}
	}
} // namespace lab

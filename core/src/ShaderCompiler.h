#pragma once

#include <string_view>

#include "vulkan/vulkan_core.h"

namespace lab {
	
	VkShaderModule loadGLSLShader(VkDevice device, std::string_view path);
} // namespace lab

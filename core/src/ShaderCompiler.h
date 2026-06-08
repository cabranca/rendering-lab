#pragma once

#include <string_view>

#include "vulkan/vulkan_core.h"

namespace lab {

	class VulkanContext;

	// Compiles a Slang source file to SPIR-V and creates a VkShaderModule containing
	// every entry point in the file (e.g. both the vertex and fragment "main").
	// The caller owns the module and destroys it with vkDestroyShaderModule.
	VkShaderModule loadSlangShader(const VulkanContext& ctx, std::string_view path);
} // namespace lab

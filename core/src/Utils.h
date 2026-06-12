#pragma once

#include <vulkan/vulkan.h>

namespace lab {
    const char* GetDebugSeverityStr(VkDebugUtilsMessageSeverityFlagBitsEXT Severity);

    const char* GetDebugType(VkDebugUtilsMessageTypeFlagsEXT Type);
}

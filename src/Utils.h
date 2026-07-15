#pragma once

#include <source_location>
#include <string>
#include <string_view>
#include <vector>

#include "vulkan/vulkan_core.h"

namespace lab {
    const char* getDebugSeverityStr(VkDebugUtilsMessageSeverityFlagBitsEXT Severity);

    // Type is a bitmask, so a message can carry several bits at once. Joins the set ones with '|'.
    std::string getDebugType(VkDebugUtilsMessageTypeFlagsEXT Type);

    uint32_t getBytesPerTexFormat(VkFormat format);

    // "<format> / <colorSpace>", e.g. "VK_FORMAT_B8G8R8A8_SRGB / VK_COLOR_SPACE_SRGB_NONLINEAR_KHR".
    std::string getSurfaceFormatStr(VkSurfaceFormatKHR format);

    const char* getPresentModeStr(VkPresentModeKHR presentMode);

    std::vector<char> readFile(std::string_view filename);

    // Logs a failed VkResult and reports whether the call succeeded. Negative result codes are
    // errors; positive ones (VK_INCOMPLETE, VK_SUBOPTIMAL_KHR, ...) are successes worth a warning.
    // `operation` names the call site for the log, e.g. "vkCreateInstance".
    void vkCheck(VkResult result, std::string_view operation,
                               std::source_location location = std::source_location::current());
} // namespace lab
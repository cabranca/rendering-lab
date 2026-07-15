#include "Utils.h"

#include <format>
#include <fstream>
#include <print>

#include <vulkan/vk_enum_string_helper.h>

#include "Logger.h"

namespace lab {

	const char* getDebugSeverityStr(VkDebugUtilsMessageSeverityFlagBitsEXT Severity) {
		switch (Severity) {
			case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
				return "Verbose";

			case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
				return "Info";

			case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
				return "Warning";

			case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
				return "Error";

			default:
				std::println("Error: Severity not recognized!");
				exit(1);
		}

		return "NO SUCH SEVERITY!";
	}

	std::string getDebugType(VkDebugUtilsMessageTypeFlagsEXT Type) {
		std::string type;

		auto append = [&type](const char* name) {
			if (!type.empty()) {
				type += '|';
			}
			type += name;
		};

		if ((Type & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) != 0) {
			append("General");
		}
		if ((Type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) != 0) {
			append("Validation");
		}
		if ((Type & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) != 0) {
			append("Performance");
		}
		if ((Type & VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT) != 0) {
			append("DeviceAddressBinding");
		}

		return type.empty() ? "Unknown" : type;
	}

	uint32_t getBytesPerTexFormat(VkFormat Format) {
		switch (Format) {
			case VK_FORMAT_R8_SINT:
			case VK_FORMAT_R8_UNORM:
				return 1;
			case VK_FORMAT_R16_SFLOAT:
				return 2;
			case VK_FORMAT_R16G16_SFLOAT:
				return 4;
			case VK_FORMAT_R16G16_SNORM:
				return 4;
			case VK_FORMAT_B8G8R8A8_UNORM:
				return 4;
			case VK_FORMAT_R8G8B8A8_UNORM:
				return 4;
			case VK_FORMAT_R16G16B16A16_SFLOAT:
				return 4 * sizeof(uint16_t);
			case VK_FORMAT_R32G32B32A32_SFLOAT:
				return 4 * sizeof(float);
			default:
				break;
		}

		return 0;
	}

	std::string getSurfaceFormatStr(VkSurfaceFormatKHR format) {
		return std::format("{} / {}", string_VkFormat(format.format), string_VkColorSpaceKHR(format.colorSpace));
	}

	const char* getPresentModeStr(VkPresentModeKHR presentMode) {
		return string_VkPresentModeKHR(presentMode);
	}

	std::vector<char> readFile(std::string_view filename) {
		std::ifstream file(filename.data(), std::ios::ate | std::ios::binary);
		if (!file.is_open())
			throw std::runtime_error("failed to open file!");
		std::vector<char> buffer(file.tellg());
		file.seekg(0, std::ios::beg);
		file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
		file.close();
		return buffer;
	}

	void vkCheck(VkResult result, std::string_view operation, std::source_location location) {
		if (result == VK_SUCCESS) {
			return;
		}

		if (result > 0) {
			CBK_WARN("{} returned {} ({}:{})", operation, string_VkResult(result),
			         location.file_name(), location.line());
			return;
		}

		CBK_ERROR("{} failed with {} ({}:{})", operation, string_VkResult(result),
		          location.file_name(), location.line());
	}
} // namespace lab

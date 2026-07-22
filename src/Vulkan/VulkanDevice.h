#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include "VulkanQueue.h"
#include "Window.h"

namespace lab::vk {

	class VulkanDevice {
	  public:
		explicit VulkanDevice(const Window& window);
		~VulkanDevice();
		VulkanDevice(const VulkanDevice& other) = delete;
		VulkanDevice& operator=(const VulkanDevice& other) = delete;
		VulkanDevice(VulkanDevice&& other) = delete;
		VulkanDevice& operator=(VulkanDevice&& other) = delete;
		
        void waitIdle();
        
		[[nodiscard]] VkPhysicalDevice getPhysicalDevice() const;
        [[nodiscard]] VkDevice getDevice() const;
        [[nodiscard]] const VulkanQueue& getQueue() const;
		[[nodiscard]] VkSurfaceKHR getSurface() const;
		[[nodiscard]] VkPhysicalDeviceMemoryProperties getMemoryProperties() const;
		[[nodiscard]] VkSampleCountFlagBits getMSAA() const;
		[[nodiscard]] uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

	  private:
        VkInstance m_Instance = VK_NULL_HANDLE;
		VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;
		VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
		VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
		uint32_t m_QueueFamilyIndex = 0;
		VkDevice m_Device = VK_NULL_HANDLE;
		VulkanQueue m_Queue;
		VkPhysicalDeviceMemoryProperties m_MemoryProperties;

        constexpr static VkSampleCountFlagBits k_MaxMSAA = VK_SAMPLE_COUNT_8_BIT;
        VkSampleCountFlagBits m_MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        SDL_Window* m_WindowHandle = nullptr; // NOT THE OWNER

        void createInstance();
		static std::vector<VkLayerProperties> getAvailableLayers();
		static std::vector<VkExtensionProperties> getAvailableExtensions();
		void createDebugMessenger();
		void createSurface();
		void pickPhysicalDevice();
		VkSampleCountFlagBits getMaxUsableSampleCount();
		void createLogicalDevice();
	};
} // namespace lab::vk
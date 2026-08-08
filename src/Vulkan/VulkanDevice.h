#pragma once

#include <vector>
#include <volk/volk.h>

#include "VulkanInstance.h"
#include "VulkanQueue.h"

namespace lab::vk {

	class VulkanDevice {
	  public:
		explicit VulkanDevice(const VulkanInstance& instance);
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
		[[nodiscard]] VkDeviceMemory allocateMemory(const VkMemoryRequirements& requirements, VkMemoryPropertyFlags properties) const;

	  private:
		// Owns the logical VkDevice. A dedicated RAII member (rather than a bare handle destroyed in
		// the class destructor body) so member-destruction order does the work: declared before
		// m_Queue, it is torn down *after* the queue and its command pools — which are children of
		// the device — with no manual reset.
		struct DeviceHandle {
			VkDevice device = VK_NULL_HANDLE;
			DeviceHandle() = default;
			~DeviceHandle();
			DeviceHandle(const DeviceHandle&) = delete;
			DeviceHandle& operator=(const DeviceHandle&) = delete;
			DeviceHandle(DeviceHandle&&) = delete;
			DeviceHandle& operator=(DeviceHandle&&) = delete;
		};

		const VulkanInstance* m_Instance = nullptr; // NON-OWNING
		VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
		uint32_t m_QueueFamilyIndex = 0;
		DeviceHandle m_DeviceHandle;
		VulkanQueue m_Queue;
		VkPhysicalDeviceMemoryProperties m_MemoryProperties{};

        constexpr static VkSampleCountFlagBits k_MaxMSAA = VK_SAMPLE_COUNT_8_BIT;
        VkSampleCountFlagBits m_MSAASamples = VK_SAMPLE_COUNT_1_BIT;

		void pickPhysicalDevice();
		VkSampleCountFlagBits getMaxUsableSampleCount();
		void createLogicalDevice();
	};
} // namespace lab::vk

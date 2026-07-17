
#include <cstring>
#include <format>
#include <string>
#include <string_view>

#define VOLK_IMPLEMENTATION
#include <volk/volk.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>
#include <vulkan/vk_enum_string_helper.h>

#include "stb_image.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include "Logger.h"
#include "VulkanEngine.h"
#include "Utils.h"

namespace lab::vk {

	namespace {
		// The loader reports its own start-up chatter (duplicate layer manifests, binary paths that
		// differ from the ones dyld resolved) as General messages tagged "Loader Message", though it
		// leaves the tag null on some of them. Neither form is worth surfacing below Error severity,
		// where a failed ICD or layer load still needs to reach the log.
		bool isLoaderNoise(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes,
		                   const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData) {
			constexpr std::string_view k_LoaderMessageId = "Loader Message";

			if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0) {
				return false;
			}
			if ((messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) != 0) {
				return false;
			}

			const char* messageId = pCallbackData->pMessageIdName;
			return messageId == nullptr || messageId == k_LoaderMessageId;
		}

		std::string formatObjects(const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData) {
			std::string objects;

			for (uint32_t i = 0; i < pCallbackData->objectCount; i++) {
				const VkDebugUtilsObjectNameInfoEXT& object = pCallbackData->pObjects[i];

				if (!objects.empty()) {
					objects += ", ";
				}
				objects += std::format("{} {:#x}", string_VkObjectType(object.objectType), object.objectHandle);

				if (object.pObjectName != nullptr) {
					objects += std::format(" \"{}\"", object.pObjectName);
				}
			}

			return objects;
		}

		VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		                                             VkDebugUtilsMessageTypeFlagsEXT messageTypes,
		                                             const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void*) {
			if (isLoaderNoise(messageSeverity, messageTypes, pCallbackData)) {
				return VK_FALSE;
			}

			const std::string objects = formatObjects(pCallbackData);
			const std::string message = std::format("[{}] {}: {}{}", getDebugType(messageTypes),
			                                        pCallbackData->pMessageIdName != nullptr ? pCallbackData->pMessageIdName : "-",
			                                        pCallbackData->pMessage, objects.empty() ? "" : std::format(" ({})", objects));

			switch (messageSeverity) {
				case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
					CBK_ERROR("{}", message);
					break;
				case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
					CBK_WARN("{}", message);
					break;
				default:
					CBK_DEBUG("{}", message);
					break;
			}

			return VK_FALSE; // The calling function should not be aborted
		}
	} // namespace

	void VulkanEngine::init(const Window& window) {
		m_WindowHandle = window.getWindowHandle();
		createInstance();
		createDebugMessenger();
		createSurface();
		pickPhysicalDevice();
		createLogicalDevice();
		createSwapchain(VK_NULL_HANDLE);
		createImageViews();
		createDescriptorSetLayout();
		createCommandPools();
		createDepthResources();
		createTextureImage();
		createTextureImageView();
		createTextureSampler();
		loadModel();
		createVertexBuffer();
		createIndexBuffer();
		createUniformBuffers();
		createGraphicsPipeline();
		createDescriptorPool();
		createDescriptorSets();
		createCommandBuffers();
		createSyncObjects();
	}

	void VulkanEngine::shutdown() {
		vkDeviceWaitIdle(m_Device);


		for (const auto& semaphore : m_PresentCompleteSemaphores) {
			vkDestroySemaphore(m_Device, semaphore, nullptr);
		}
		m_PresentCompleteSemaphores.clear();

		for (const auto& semaphore : m_RenderFinishedSemaphores) {
			vkDestroySemaphore(m_Device, semaphore, nullptr);
		}
		m_RenderFinishedSemaphores.clear();

		for (const auto& fence : m_DrawFences) {
			vkDestroyFence(m_Device, fence, nullptr);
		}
		m_DrawFences.clear();

		if (m_DescriptorPool != VK_NULL_HANDLE) {
			vkDestroyDescriptorPool(m_Device, m_DescriptorPool, nullptr);
			m_DescriptorPool = VK_NULL_HANDLE;
			CBK_DEBUG("Desriptor Pool destroyed");
		}

		if (m_DepthImageView != VK_NULL_HANDLE) {
			vkDestroyImageView(m_Device, m_DepthImageView, nullptr);
			m_DepthImageView = VK_NULL_HANDLE;
			CBK_DEBUG("Texture Image View destroyed");
		}

		if (m_DepthImageMemory != VK_NULL_HANDLE) {
			vkFreeMemory(m_Device, m_DepthImageMemory, nullptr);
			m_DepthImageMemory = VK_NULL_HANDLE;
			CBK_DEBUG("Texture deallocated");
		}

		if (m_DepthImage != VK_NULL_HANDLE) {
			vkDestroyImage(m_Device, m_DepthImage, nullptr);
			m_DepthImage = VK_NULL_HANDLE;
			CBK_DEBUG("Index Buffer destroyed");
		}

		if (m_TextureSampler != VK_NULL_HANDLE) {
			vkDestroySampler(m_Device, m_TextureSampler, nullptr);
			m_TextureSampler = VK_NULL_HANDLE;
			CBK_DEBUG("Texture Sampler destroyed");
		}

		if (m_TextureImageView != VK_NULL_HANDLE) {
			vkDestroyImageView(m_Device, m_TextureImageView, nullptr);
			m_TextureImageView = VK_NULL_HANDLE;
			CBK_DEBUG("Texture Image View destroyed");
		}

		if (m_TextureImageMemory != VK_NULL_HANDLE) {
			vkFreeMemory(m_Device, m_TextureImageMemory, nullptr);
			m_TextureImageMemory = VK_NULL_HANDLE;
			CBK_DEBUG("Texture deallocated");
		}

		if (m_TextureImage != VK_NULL_HANDLE) {
			vkDestroyImage(m_Device, m_TextureImage, nullptr);
			m_TextureImage = VK_NULL_HANDLE;
			CBK_DEBUG("Index Buffer destroyed");
		}

		for (size_t i = 0; i < k_MaxFramesInFlight; i++) {
			vkUnmapMemory(m_Device, m_UniformBuffersMemory[i]);
			vkFreeMemory(m_Device, m_UniformBuffersMemory[i], nullptr);
			vkDestroyBuffer(m_Device, m_UniformBuffers[i], nullptr);
		}
		CBK_DEBUG("Uniform Buffers deallocated and destroyed");

		if (m_IndexBufferMemory != VK_NULL_HANDLE) {
			vkFreeMemory(m_Device, m_IndexBufferMemory, nullptr);
			m_IndexBufferMemory = VK_NULL_HANDLE;
			CBK_DEBUG("Index Buffer deallocated");
		}

		if (m_IndexBuffer != VK_NULL_HANDLE) {
			vkDestroyBuffer(m_Device, m_IndexBuffer, nullptr);
			m_IndexBuffer = VK_NULL_HANDLE;
			CBK_DEBUG("Index Buffer destroyed");
		}

		if (m_VertexBufferMemory != VK_NULL_HANDLE) {
			vkFreeMemory(m_Device, m_VertexBufferMemory, nullptr);
			m_VertexBufferMemory = VK_NULL_HANDLE;
			CBK_DEBUG("Vertex Buffer deallocated");
		}

		if (m_VertexBuffer != VK_NULL_HANDLE) {
			vkDestroyBuffer(m_Device, m_VertexBuffer, nullptr);
			m_VertexBuffer = VK_NULL_HANDLE;
			CBK_DEBUG("Vertex Buffer destroyed");
		}

		if (m_SingleTimeCmdPool != VK_NULL_HANDLE) {
			vkDestroyCommandPool(m_Device, m_SingleTimeCmdPool, nullptr);
			m_SingleTimeCmdPool = VK_NULL_HANDLE;
		}

		if (m_CmdPool != VK_NULL_HANDLE) {
			vkDestroyCommandPool(m_Device, m_CmdPool, nullptr);
			m_CmdPool = VK_NULL_HANDLE;
			CBK_DEBUG("Vulkan Command Pool destroyed");
		}

		if (m_GraphicsPipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(m_Device, m_GraphicsPipeline, nullptr);
			m_GraphicsPipeline = VK_NULL_HANDLE;
			CBK_DEBUG("Vulkan Graphics Pipeline destroyed");
		}

		if (m_PipelineLayout != VK_NULL_HANDLE) {
			vkDestroyPipelineLayout(m_Device, m_PipelineLayout, nullptr);
			m_PipelineLayout = VK_NULL_HANDLE;
			CBK_DEBUG("Vulkan Pipeline Layout destroyed");
		}

		if (m_DescriptorSetLayout != VK_NULL_HANDLE) {
			vkDestroyDescriptorSetLayout(m_Device, m_DescriptorSetLayout, nullptr);
			m_DescriptorSetLayout = VK_NULL_HANDLE;
			CBK_DEBUG("Descriptor Set Layout destroyed");
		}

		for (const auto& view: m_ImageViews) {
			vkDestroyImageView(m_Device, view, nullptr);
		}
		m_ImageViews.clear();

		if (m_Swapchain != VK_NULL_HANDLE) {
			vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
			m_Swapchain = VK_NULL_HANDLE;
			CBK_DEBUG("Vulkan Swapchain destroyed");
		}

		if (m_Device != VK_NULL_HANDLE) {
			vkDestroyDevice(m_Device, nullptr);
			m_Device = VK_NULL_HANDLE;
			CBK_DEBUG("Vulkan Device destroyed");
		}

		if (m_Surface != VK_NULL_HANDLE) {
			SDL_Vulkan_DestroySurface(m_Instance, m_Surface, nullptr);
			m_Surface = VK_NULL_HANDLE;
			CBK_DEBUG("Vulkan Surface destroyed");
		}

		if (m_Instance == VK_NULL_HANDLE)
			return;

		// Destroyed before the instance, but the messenger chained into VkInstanceCreateInfo::pNext
		// still covers vkDestroyInstance itself.
		if (m_DebugMessenger != VK_NULL_HANDLE) {
			vkDestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);
			m_DebugMessenger = VK_NULL_HANDLE;
			CBK_DEBUG("Vulkan Debug Messenger destroyed");
		}

		vkDestroyInstance(m_Instance, nullptr);
		m_Instance = VK_NULL_HANDLE;
		CBK_DEBUG("Vulkan Instance destroyed");
	}

	void VulkanEngine::drawFrame() {
		updateUniformBuffer(m_FrameIndex);

		vkCheck(vkWaitForFences(m_Device, 1, &m_DrawFences[m_FrameIndex], VK_TRUE, UINT64_MAX), "vkWaitForFences");
		
		uint32_t imageIndex = 0;
		VkResult result = vkAcquireNextImageKHR(m_Device, m_Swapchain, UINT64_MAX, m_PresentCompleteSemaphores[m_FrameIndex], nullptr, &imageIndex);
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			recreateSwapchain();
			return;
		}
		// VK_SUBOPTIMAL_KHR still acquired an image and signalled the semaphore, so it has to be drawn and
		// presented; the present below reports it again and recreates then.
		if (result != VK_SUBOPTIMAL_KHR) {
			vkCheck(result, "vkAcquireNextImageKHR");
		}

		vkCheck(vkResetFences(m_Device, 1, &m_DrawFences[m_FrameIndex]), "vkResetFences");

		vkCheck(vkResetCommandBuffer(m_CmdBuffers[m_FrameIndex], 0), "vkResetCommandBuffer");
		recordCommandBuffer(m_FrameIndex);

		transitionImageLayout(m_CmdBuffers[m_FrameIndex], m_Images[imageIndex], VK_IMAGE_LAYOUT_UNDEFINED,
		                      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_2_NONE, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		                      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		                      VK_IMAGE_ASPECT_COLOR_BIT);
		transitionImageLayout(m_CmdBuffers[m_FrameIndex], m_DepthImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
		                      VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		                      VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
		                      VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
		                      VK_IMAGE_ASPECT_DEPTH_BIT);

		VkRenderingAttachmentInfo attachmentInfo{ .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			                                      .pNext = nullptr,
			                                      .imageView = m_ImageViews[imageIndex],
			                                      .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			                                      .resolveMode = VK_RESOLVE_MODE_NONE,
			                                      .resolveImageView = VK_NULL_HANDLE,
			                                      .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			                                      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			                                      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			                                      .clearValue = { .color = { 0.0f, 0.0f, 0.0f, 1.0f }, } };

		VkRenderingAttachmentInfo depthAttachmentInfo{ .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			                                           .pNext = nullptr,
			                                           .imageView = m_DepthImageView,
			                                           .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
			                                           .resolveMode = VK_RESOLVE_MODE_NONE,
			                                           .resolveImageView = VK_NULL_HANDLE,
			                                           .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			                                           .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			                                           .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			                                           .clearValue = { .depthStencil = { 1.0f, 0 } } };

		VkRenderingInfo renderingInfo{ .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
			                           .pNext = nullptr,
			                           .flags = 0,
			                           .renderArea = { .offset = { 0, 0 }, .extent = m_Extent },
			                           .layerCount = 1,
			                           .colorAttachmentCount = 1,
			                           .pColorAttachments = &attachmentInfo,
									   .pDepthAttachment = &depthAttachmentInfo };

		vkCmdBeginRendering(m_CmdBuffers[m_FrameIndex], &renderingInfo);

		vkCmdBindPipeline(m_CmdBuffers[m_FrameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipeline);

		VkDeviceSize offsets = 0;
		vkCmdBindVertexBuffers(m_CmdBuffers[m_FrameIndex], 0, 1, &m_VertexBuffer, &offsets);
		vkCmdBindIndexBuffer(m_CmdBuffers[m_FrameIndex], m_IndexBuffer, {}, VK_INDEX_TYPE_UINT32);
		vkCmdBindDescriptorSets(m_CmdBuffers[m_FrameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 0, 1,
		                        &m_DescriptorSets[m_FrameIndex], 0, nullptr);

		VkViewport viewport{ .x = 0.0f,
			                 .y = 0.0f,
			                 .width = static_cast<float>(m_Extent.width),
			                 .height = static_cast<float>(m_Extent.height),
			                 .minDepth = 0.0f,
			                 .maxDepth = 1.0f };
		VkRect2D scissor{
			.offset = { .x = 0, .y = 0 },
			.extent = m_Extent,
		};

		vkCmdSetViewport(m_CmdBuffers[m_FrameIndex], 0, 1, &viewport);
		vkCmdSetScissor(m_CmdBuffers[m_FrameIndex], 0, 1, &scissor);

		vkCmdDrawIndexed(m_CmdBuffers[m_FrameIndex], static_cast<uint32_t>(m_Indices.size()), 1, 0, 0, 0);

		vkCmdEndRendering(m_CmdBuffers[m_FrameIndex]);

		transitionImageLayout(m_CmdBuffers[m_FrameIndex], m_Images[imageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		                      VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_2_NONE,
		                      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

		vkCheck(vkEndCommandBuffer(m_CmdBuffers[m_FrameIndex]), "vkEndCommandBuffer");

		std::array<VkPipelineStageFlags, 1> waitDstStageMask = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

		VkSubmitInfo submitInfo{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.pNext = nullptr,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &m_PresentCompleteSemaphores[m_FrameIndex],
			.pWaitDstStageMask = waitDstStageMask.data(),
			.commandBufferCount = 1,
			.pCommandBuffers = &m_CmdBuffers[m_FrameIndex],
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = &m_RenderFinishedSemaphores[imageIndex]
		};
		vkCheck(vkQueueSubmit(m_Queue, 1, &submitInfo, m_DrawFences[m_FrameIndex]), "vkQueueSubmit");

		VkPresentInfoKHR presentInfo{
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.pNext = nullptr,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &m_RenderFinishedSemaphores[imageIndex],
			.swapchainCount = 1,
			.pSwapchains = &m_Swapchain,
			.pImageIndices = &imageIndex,
			.pResults = nullptr
		};
		result = vkQueuePresentKHR(m_Queue, &presentInfo);
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
			recreateSwapchain();

		m_FrameIndex = (m_FrameIndex + 1) % k_MaxFramesInFlight;
	}

	void VulkanEngine::createInstance() {
		vkCheck(volkInitialize(), "volkInitialize");

		const VkApplicationInfo appInfo{ .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			                             .pNext = nullptr,
			                             .pApplicationName = "Render Lab",
			                             .apiVersion = VK_API_VERSION_1_4 };

		const auto availableLayers = getAvailableLayers();
		const auto availableExtensions = getAvailableExtensions();

		std::vector<const char*> layers;
		VkInstanceCreateFlags flags = 0;
		bool debugUtilsAvailable = false;

		uint32_t sdlExtensionsCount = 0;
		const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionsCount);
		std::vector<const char*> extensions(sdlExtensions, sdlExtensions + sdlExtensionsCount);

		for (const auto& extension: availableExtensions) {
			if (std::strcmp(extension.extensionName, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0) {
				extensions.emplace_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
				flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
			}
			if (std::strcmp(extension.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0) {
				extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
				debugUtilsAvailable = true;
			}
		}

		for (const auto& layer: availableLayers) {
			if (std::strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") == 0) {
				layers.emplace_back("VK_LAYER_KHRONOS_validation");
			}
		}

		const VkDebugUtilsMessengerCreateInfoEXT debugMessengerCI{ .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
			                                                       .pNext = nullptr,
			                                                       .flags = 0,
			                                                       .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			                                                                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
			                                                       .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			                                                                      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			                                                                      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
			                                                       .pfnUserCallback = &debugCallback,
			                                                       .pUserData = nullptr };

		const VkInstanceCreateInfo instanceCI{ .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			                                   .pNext = debugUtilsAvailable ? &debugMessengerCI : nullptr,
			                                   .flags = flags,
			                                   .pApplicationInfo = &appInfo,
			                                   .enabledLayerCount = static_cast<uint32_t>(layers.size()),
			                                   .ppEnabledLayerNames = layers.data(),
			                                   .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
			                                   .ppEnabledExtensionNames = extensions.data() };

		vkCheck(vkCreateInstance(&instanceCI, nullptr, &m_Instance), "vkCreateInstance");

		volkLoadInstance(m_Instance);
		CBK_DEBUG("Vulkan Instance created");
	}

	std::vector<VkLayerProperties> VulkanEngine::getAvailableLayers() {
		uint32_t layerPropCount = 0;
		vkCheck(vkEnumerateInstanceLayerProperties(&layerPropCount, nullptr), "vkEnumerateInstanceLayerProperties");
		std::vector<VkLayerProperties> availableLayers(layerPropCount);
		vkCheck(vkEnumerateInstanceLayerProperties(&layerPropCount, availableLayers.data()), "vkEnumerateInstanceLayerProperties");

		return availableLayers;
	}

	std::vector<VkExtensionProperties> VulkanEngine::getAvailableExtensions() {
		uint32_t extensionsCount = 0;
		vkCheck(vkEnumerateInstanceExtensionProperties(nullptr, &extensionsCount, nullptr), "vkEnumerateInstanceExtensionProperties");
		std::vector<VkExtensionProperties> availableExtensions(extensionsCount);
		vkCheck(vkEnumerateInstanceExtensionProperties(nullptr, &extensionsCount, availableExtensions.data()),
		        "vkEnumerateInstanceExtensionProperties");

		return availableExtensions;
	}

	void VulkanEngine::createDebugMessenger() {
		if (vkCreateDebugUtilsMessengerEXT == nullptr) {
			CBK_WARN("VK_EXT_debug_utils unavailable, validation messages will not be reported");
			return;
		}

		const VkDebugUtilsMessengerCreateInfoEXT debugMessengerCI{ .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
			                                                       .pNext = nullptr,
			                                                       .flags = 0,
			                                                       .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			                                                                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
			                                                       .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			                                                                      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			                                                                      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
			                                                       .pfnUserCallback = &debugCallback,
			                                                       .pUserData = nullptr };
		vkCheck(vkCreateDebugUtilsMessengerEXT(m_Instance, &debugMessengerCI, nullptr, &m_DebugMessenger),
		        "vkCreateDebugUtilsMessengerEXT");

		CBK_DEBUG("Vulkan Debug Messenger created");
	}

	void VulkanEngine::createSurface() {
		bool surfaceCreated = SDL_Vulkan_CreateSurface(m_WindowHandle, m_Instance, nullptr, &m_Surface);
		if (!surfaceCreated)
			CBK_ERROR("Couldn't create Vulkan Surface!");
	}

	void VulkanEngine::pickPhysicalDevice() {
		uint32_t deviceCount = 0;
		vkCheck(vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr), "vkEnumeratePhysicalDevices");
		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkCheck(vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data()), "vkEnumeratePhysicalDevices");

		for (const auto& device: devices) {
			VkPhysicalDeviceProperties props;
			vkGetPhysicalDeviceProperties(device, &props);

			if (props.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && props.deviceType != VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
				CBK_DEBUG("{}: not a discrete or integrated GPU", props.deviceName);
				continue;
			}
			if (props.apiVersion < VK_API_VERSION_1_4) {
				CBK_DEBUG("{}: VK API version is less than 1.4", props.deviceName);
				continue;
			}

			uint32_t qFamilyPropCount = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(device, &qFamilyPropCount, nullptr);
			std::vector<VkQueueFamilyProperties> qFamilyProps(qFamilyPropCount);
			vkGetPhysicalDeviceQueueFamilyProperties(device, &qFamilyPropCount, qFamilyProps.data());

			bool supportsGraphics = false;
			for (uint32_t i = 0; i < qFamilyPropCount; i++) {
				const auto& qFamily = qFamilyProps[i];
				VkBool32 supportsSurface = VK_FALSE;
				vkCheck(vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_Surface, &supportsSurface),
				        "vkGetPhysicalDeviceSrufaceSupportKHR");
				if ((qFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 && supportsSurface == VK_TRUE) {
					supportsGraphics = true;
					m_QueueFamilyIndex = i;
					break;
				}
			}

			if (!supportsGraphics) {
				CBK_DEBUG("{}: no graphics queue", props.deviceName);
				continue;
			}

			std::vector<const char*> requiredDeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
			uint32_t extensionPropCount = 0;
			vkCheck(vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionPropCount, nullptr),
			        "vkEnumerateDeviceExtensionProperties");
			std::vector<VkExtensionProperties> deviceExtensions(extensionPropCount);
			vkCheck(vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionPropCount, deviceExtensions.data()),
			        "vkEnumerateDeviceExtensionProperties");

			bool supportsExtensions = true;
			for (const auto& required: requiredDeviceExtensions) {
				bool supportsExtension = false;
				for (const auto& available: deviceExtensions) {
					if (std::strcmp(required, available.extensionName) == 0)
						supportsExtension = true;
				}
				if (!supportsExtension) {
					CBK_DEBUG("{}: missing extension {}", props.deviceName, required);
					supportsExtensions = false;
				}
			}
			if (!supportsExtensions)
				continue;

			// Chained so one query fills all three; each struct is zero-initialized because a driver
			// leaves any sType it does not recognize untouched.
			VkPhysicalDeviceVulkan13Features vk13Features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
			VkPhysicalDeviceVulkan11Features vk11Features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
				                                           .pNext = &vk13Features };
			VkPhysicalDeviceFeatures2 features2{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &vk11Features };
			vkGetPhysicalDeviceFeatures2(device, &features2);

			if (features2.features.tessellationShader != VK_TRUE) {
				CBK_DEBUG("{}: no tessellation shader support", props.deviceName);
				continue;
			}
			if (vk11Features.shaderDrawParameters != VK_TRUE) {
				CBK_DEBUG("{}: no shader draw parameters support", props.deviceName);
				continue;
			}
			if (vk13Features.dynamicRendering != VK_TRUE) {
				CBK_DEBUG("{}: no dynamic rendering support", props.deviceName);
				continue;
			}

			m_PhysicalDevice = device;
			CBK_INFO("Physical Device: {}", props.deviceName);
			break;
		}

		if (m_PhysicalDevice == VK_NULL_HANDLE) {
			CBK_ERROR("No physical device met the requirements!");
		}
	}

	void VulkanEngine::createLogicalDevice() {
		float queuePriority = 0.5F;
		VkDeviceQueueCreateInfo queueCI{ .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			                             .pNext = nullptr,
			                             .flags = 0,
			                             .queueFamilyIndex = m_QueueFamilyIndex,
			                             .queueCount = 1,
			                             .pQueuePriorities = &queuePriority };

		VkPhysicalDeviceVulkan13Features vk13Features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
			                                           .synchronization2 = VK_TRUE , .dynamicRendering = VK_TRUE};
		VkPhysicalDeviceVulkan11Features vk11Features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
			                                           .pNext = &vk13Features,
			                                           .shaderDrawParameters = VK_TRUE };
		VkPhysicalDeviceFeatures2 features2{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &vk11Features, .features {.samplerAnisotropy = VK_TRUE} };

		std::vector<const char*> requiredDeviceExtension = { VK_KHR_SWAPCHAIN_EXTENSION_NAME, "VK_KHR_portability_subset" };
		VkDeviceCreateInfo deviceCI{ .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			                         .pNext = &features2,
			                         .flags = 0,
			                         .queueCreateInfoCount = 1,
			                         .pQueueCreateInfos = &queueCI,
			                         .enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtension.size()),
			                         .ppEnabledExtensionNames = requiredDeviceExtension.data(),
			                         .pEnabledFeatures = nullptr };
		vkCheck(vkCreateDevice(m_PhysicalDevice, &deviceCI, nullptr, &m_Device), "vkCreateDevice");
		volkLoadDevice(m_Device);
		CBK_DEBUG("Vulkan Logical Device created");

		vkGetDeviceQueue(m_Device, m_QueueFamilyIndex, 0, &m_Queue);
	}

	void VulkanEngine::createSwapchain(VkSwapchainKHR oldSwapchain) {
		VkSurfaceCapabilitiesKHR surfaceCaps;
		vkCheck(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_PhysicalDevice, m_Surface, &surfaceCaps),
		        "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

		uint32_t surfaceFormatsCount = 0;
		vkCheck(vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &surfaceFormatsCount, nullptr),
		        "vkGetPhysicalDeviceSurfaceFormatsKHR");
		std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatsCount);
		vkCheck(vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &surfaceFormatsCount, surfaceFormats.data()),
		        "vkGetPhysicalDeviceSurfaceFormatsKHR");

		uint32_t presentModesCount = 0;
		vkCheck(vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice, m_Surface, &presentModesCount, nullptr),
		        "vkGetPhysicalDeviceSurfacePresentModesKHR");
		std::vector<VkPresentModeKHR> presentModes(presentModesCount);
		vkCheck(vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice, m_Surface, &presentModesCount, presentModes.data()),
		        "vkGetPhysicalDeviceSurfacePresentModesKHR");

		m_SelectedFormat = chooseSwapSurfaceFormat(surfaceFormats);
		const auto selectedPresentMode = chooseSwapPresentMode(presentModes);
		m_Extent = chooseSwapExtent(surfaceCaps);
		const auto minImageCount = chooseSwapMinImageCount(surfaceCaps);

		VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		VkSwapchainCreateInfoKHR swapchainCI{ .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			                                  .pNext = nullptr,
			                                  .flags = 0,
			                                  .surface = m_Surface,
			                                  .minImageCount = minImageCount,
			                                  .imageFormat = m_SelectedFormat.format,
			                                  .imageColorSpace = m_SelectedFormat.colorSpace,
			                                  .imageExtent = m_Extent,
			                                  .imageArrayLayers = 1,
			                                  .imageUsage = usage,
			                                  .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
			                                  .queueFamilyIndexCount = 1,
			                                  .pQueueFamilyIndices = &m_QueueFamilyIndex,
			                                  .preTransform = surfaceCaps.currentTransform,
			                                  .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			                                  .presentMode = selectedPresentMode,
			                                  .clipped = VK_TRUE,
			                                  .oldSwapchain = oldSwapchain };
		vkCheck(vkCreateSwapchainKHR(m_Device, &swapchainCI, nullptr, &m_Swapchain), "vkCreateSwapchainKHR");

		uint32_t imageCount = 0;
		vkCheck(vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, nullptr), "vkGetSwapchainImagesKHR");
		m_Images.resize(imageCount);
		vkCheck(vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, m_Images.data()), "vkGetSwapchainImagesKHR");
		CBK_DEBUG("The number of Swapchain images is {}", imageCount);

		CBK_DEBUG("Vulkan Swapchain created");
	}

	VkSurfaceFormatKHR VulkanEngine::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
		VkSurfaceFormatKHR res = availableFormats[0];
		for (const auto& format: availableFormats) {
			if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
				res = format;
		}
		CBK_DEBUG("Selected surface format is {}", getSurfaceFormatStr(res));
		return res;
	}

	VkPresentModeKHR VulkanEngine::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
		VkPresentModeKHR res = VK_PRESENT_MODE_FIFO_KHR;
		for (const auto& mode: availablePresentModes) {
			if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
				res = mode;
		}
		CBK_DEBUG("Selected present mode is {}", getPresentModeStr(res));
		return res;
	}

	VkExtent2D VulkanEngine::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
			return capabilities.currentExtent;
		int width, height;
		SDL_GetWindowSizeInPixels(m_WindowHandle, &width, &height);
		return { std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
			     std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height) };
	}

	uint32_t VulkanEngine::chooseSwapMinImageCount(const VkSurfaceCapabilitiesKHR& surfaceCapabilities) {
		auto minImageCount = std::max(3U, surfaceCapabilities.minImageCount);
		if ((0 < surfaceCapabilities.maxImageCount) && (surfaceCapabilities.maxImageCount < minImageCount)) {
			minImageCount = surfaceCapabilities.maxImageCount;
		}
		return minImageCount;
	}

	void VulkanEngine::createImageViews() {
		m_ImageViews.resize(m_Images.size());
		for (size_t i = 0; i < m_Images.size(); i++) {
			m_ImageViews[i] = createImageView(m_Images[i], m_SelectedFormat.format, VK_IMAGE_ASPECT_COLOR_BIT);
		}
	}

	VkImageView VulkanEngine::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
		VkImageView imageView = VK_NULL_HANDLE;
		VkImageViewCreateInfo viewCI{ .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			                          .pNext = nullptr,
			                          .flags = 0,
			                          .image = image,
			                          .viewType = VK_IMAGE_VIEW_TYPE_2D,
			                          .format = format,
			                          .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY, .g = VK_COMPONENT_SWIZZLE_IDENTITY, .b = VK_COMPONENT_SWIZZLE_IDENTITY, .a = VK_COMPONENT_SWIZZLE_IDENTITY, },
			                          .subresourceRange = { .aspectMask = aspectFlags,
			                                                .baseMipLevel = 0,
			                                                .levelCount = 1,
			                                                .baseArrayLayer = 0,
			                                                .layerCount = 1 } };
		vkCreateImageView(m_Device, &viewCI, nullptr, &imageView);
		return imageView;
	}

	void VulkanEngine::createDescriptorSetLayout() {
		std::array<VkDescriptorSetLayoutBinding, 2> bindings{ { { .binding = 0,
			                                                      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			                                                      .descriptorCount = 1,
			                                                      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
			                                                      .pImmutableSamplers = nullptr },
			                                                    { .binding = 1,
			                                                      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			                                                      .descriptorCount = 1,
			                                                      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			                                                      .pImmutableSamplers = nullptr } } };
		VkDescriptorSetLayoutCreateInfo setLayoutCI{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.bindingCount = static_cast<uint32_t>(bindings.size()),
			.pBindings = bindings.data()
		};
		vkCheck(vkCreateDescriptorSetLayout(m_Device, &setLayoutCI, nullptr, &m_DescriptorSetLayout), "vkCreateDescriptorSetLayout");
		CBK_DEBUG("MVP Uniform Buffer layout created");
	}

	void VulkanEngine::createGraphicsPipeline() {
		const auto shaderModule = createShaderModule(readFile("assets/shaders/shader.spv"));

		std::array<VkPipelineShaderStageCreateInfo, 2> stages{ { { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			                                                       .pNext = nullptr,
			                                                       .flags = 0,
			                                                       .stage = VK_SHADER_STAGE_VERTEX_BIT,
			                                                       .module = shaderModule,
			                                                       .pName = "vertMain",
			                                                       .pSpecializationInfo = nullptr },
			                                                     { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			                                                       .pNext = nullptr,
			                                                       .flags = 0,
			                                                       .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			                                                       .module = shaderModule,
			                                                       .pName = "fragMain",
			                                                       .pSpecializationInfo = nullptr } } };

		std::array<VkDynamicState, 2> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicStateCI{ .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			                                             .pNext = nullptr,
			                                             .flags = 0,
			                                             .dynamicStateCount = dynamicStates.size(),
			                                             .pDynamicStates = dynamicStates.data() };

		VkPipelineInputAssemblyStateCreateInfo inputAssemblyCI{ .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			                                                    .pNext = nullptr,
			                                                    .flags = 0,
			                                                    .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			                                                    .primitiveRestartEnable = VK_FALSE };

		auto bindingDescription = Vertex::getBindingDescription();
		auto attributeDescriptions = Vertex::getAttributeDescriptions();
		VkPipelineVertexInputStateCreateInfo vertexInputCI{ .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			                                                .pNext = nullptr,
			                                                .flags = 0,
			                                                .vertexBindingDescriptionCount = 1,
			                                                .pVertexBindingDescriptions = &bindingDescription,
			                                                .vertexAttributeDescriptionCount = attributeDescriptions.size(),
			                                                .pVertexAttributeDescriptions = attributeDescriptions.data() };

		VkPipelineViewportStateCreateInfo viewportStateCI {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.viewportCount = 1,
			.pViewports = nullptr,
			.scissorCount = 1,
			.pScissors = nullptr
		};

		VkPipelineRasterizationStateCreateInfo rasterizerCI {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.depthClampEnable = VK_FALSE,
			.rasterizerDiscardEnable = VK_FALSE,
			.polygonMode = VK_POLYGON_MODE_FILL,
			.cullMode = VK_CULL_MODE_BACK_BIT,
			.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
			.depthBiasEnable = VK_FALSE,
			.depthBiasConstantFactor = 0.0f,
			.depthBiasClamp = 0.0f,
			.depthBiasSlopeFactor = 0.0f,
			.lineWidth = 1.0f
		};

		VkPipelineMultisampleStateCreateInfo msaaCI {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
			.sampleShadingEnable = VK_FALSE,
			.minSampleShading = 0.0f,
			.pSampleMask = nullptr,
			.alphaToCoverageEnable = VK_FALSE,
			.alphaToOneEnable = VK_FALSE
		};

		VkPipelineColorBlendAttachmentState colorBlendAttachment {
			.blendEnable = VK_FALSE,
			.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
		};

		VkPipelineColorBlendStateCreateInfo blendStateCI {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.logicOpEnable = VK_FALSE,
			.logicOp = VK_LOGIC_OP_AND,
			.attachmentCount = 1,
			.pAttachments = &colorBlendAttachment,
			.blendConstants = { 0.0f, 0.0f, 0.0f, 1.0f}
		};

		VkPipelineDepthStencilStateCreateInfo depthStencilCI{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.depthTestEnable = VK_TRUE,
			.depthWriteEnable = VK_TRUE,
			.depthCompareOp = VK_COMPARE_OP_LESS,
			.depthBoundsTestEnable = VK_FALSE,
			.stencilTestEnable = VK_FALSE,
		};

		VkPipelineLayoutCreateInfo layoutCI {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.setLayoutCount = 1,
			.pSetLayouts = &m_DescriptorSetLayout,
			.pushConstantRangeCount = 0,
			.pPushConstantRanges = nullptr
		};
		vkCheck(vkCreatePipelineLayout(m_Device, &layoutCI, nullptr, &m_PipelineLayout), "vkCreatePipelineLayout");

		VkPipelineRenderingCreateInfo renderingCI {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.pNext = nullptr,
			.viewMask = 0,
			.colorAttachmentCount = 1,
			.pColorAttachmentFormats = &m_SelectedFormat.format,
			.depthAttachmentFormat = m_DepthFormat,
			.stencilAttachmentFormat = VK_FORMAT_UNDEFINED
		};
		VkGraphicsPipelineCreateInfo graphicsPipelineCI {
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.pNext = &renderingCI,
			.flags = 0,
			.stageCount = 2,
			.pStages = stages.data(),
			.pVertexInputState = &vertexInputCI,
			.pInputAssemblyState = &inputAssemblyCI,
			.pTessellationState = nullptr,
			.pViewportState = &viewportStateCI,
			.pRasterizationState = &rasterizerCI,
			.pMultisampleState = &msaaCI,
			.pDepthStencilState = &depthStencilCI,
			.pColorBlendState = &blendStateCI,
			.pDynamicState = &dynamicStateCI,
			.layout = m_PipelineLayout,
			.renderPass = VK_NULL_HANDLE,
			.subpass = 0,
			.basePipelineHandle = VK_NULL_HANDLE,
			.basePipelineIndex = 0
		};
		vkCheck(vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &graphicsPipelineCI, nullptr, &m_GraphicsPipeline), "vkCreateGraphicsPipelines");

		CBK_DEBUG("Vulkan Graphics Pipeline created");

		vkDestroyShaderModule(m_Device, shaderModule, nullptr);
	}

	[[nodiscard]] VkShaderModule VulkanEngine::createShaderModule(const std::vector<char>& code) const {
		VkShaderModuleCreateInfo moduleCI{ .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			                               .pNext = nullptr,
			                               .flags = 0,
			                               .codeSize = code.size() * sizeof(char),
			                               .pCode = reinterpret_cast<const uint32_t*>(code.data()) };

		VkShaderModule module = VK_NULL_HANDLE;
		vkCheck(vkCreateShaderModule(m_Device, &moduleCI, nullptr, &module), "vkCreateShaderModule");
		return module;
	}

	void VulkanEngine::createCommandPools() {
		VkCommandPoolCreateInfo poolCI {
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.pNext = nullptr,
			.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = m_QueueFamilyIndex
		};
		vkCheck(vkCreateCommandPool(m_Device, &poolCI, nullptr, &m_CmdPool), "vkCreateCommandPool");
		CBK_DEBUG("Vulkan Command Pool created");

		poolCI.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
		vkCheck(vkCreateCommandPool(m_Device, &poolCI, nullptr, &m_SingleTimeCmdPool), "vkCreateCommandPool");
	}

	void VulkanEngine::createDepthResources() {
		m_DepthFormat = findSupportedFormat({ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
		                                           VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_2_DEPTH_STENCIL_ATTACHMENT_BIT);
		std::tie(m_DepthImage, m_DepthImageMemory) = createImage(m_Extent.width, m_Extent.height, m_DepthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		m_DepthImageView = createImageView(m_DepthImage, m_DepthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
	}

	VkFormat VulkanEngine::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
		for (const auto& format : candidates) {
			VkFormatProperties2 props{.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2};
			vkGetPhysicalDeviceFormatProperties2(m_PhysicalDevice, format, &props);
			if (((tiling == VK_IMAGE_TILING_LINEAR) && ((props.formatProperties.linearTilingFeatures & features) == features)) || ((tiling == VK_IMAGE_TILING_OPTIMAL) && ((props.formatProperties.optimalTilingFeatures & features) == features)))
				return format;
		}
		CBK_FATAL("Failed to find supported format!");
		return VK_FORMAT_MAX_ENUM;
	}

	void VulkanEngine::createTextureImage() {
		int width, height, channels;
		stbi_uc* pixels = stbi_load(k_TexturePath.data(), &width, &height, &channels, STBI_rgb_alpha);
		if (!pixels) {
			CBK_ERROR("Couldn't load texture {}", "assets/textures/statue.jpg");
			return;
		}

		auto textureSize = width * height * 4;
		auto [stagingBuffer, stagingBufferMemory] = createBuffer(
		    textureSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		void* dataStaging = nullptr;
		vkCheck(vkMapMemory(m_Device, stagingBufferMemory, 0, textureSize, 0, &dataStaging), "vkMapMemory");
		memcpy(dataStaging, pixels, textureSize);
		vkUnmapMemory(m_Device, stagingBufferMemory);

		stbi_image_free(pixels);

		std::tie(m_TextureImage, m_TextureImageMemory) =
		    createImage(static_cast<uint32_t>(width), static_cast<uint32_t>(height), VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
		                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VkCommandBuffer copyCmdBuffer = beginSingleTimeCommands();
		transitionImageLayout(copyCmdBuffer, m_TextureImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		                      VK_ACCESS_2_NONE, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
		                      VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
		copyBufferToImage(copyCmdBuffer, stagingBuffer, m_TextureImage, width, height);
		transitionImageLayout(copyCmdBuffer, m_TextureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		                      VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
		                      VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
		endSingleTimeCommands(copyCmdBuffer);

		CBK_DEBUG("Texture Image created");

		vkFreeMemory(m_Device, stagingBufferMemory, nullptr);
		vkDestroyBuffer(m_Device, stagingBuffer, nullptr);
	}

	std::pair<VkImage, VkDeviceMemory> VulkanEngine::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
		                                               VkImageUsageFlags usage, VkMemoryPropertyFlags properties) {
		VkImage texture = VK_NULL_HANDLE;
		VkDeviceMemory textureMemory = VK_NULL_HANDLE;
			VkImageCreateInfo textureCI{
			.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.imageType = VK_IMAGE_TYPE_2D,
			.format = format,
			.extent = { .width = width, .height = height, .depth = 1},
			.mipLevels = 1,
			.arrayLayers = 1,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.tiling = tiling,
			.usage = usage,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = VK_QUEUE_FAMILY_IGNORED,
			.pQueueFamilyIndices = nullptr,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
		};
		vkCheck(vkCreateImage(m_Device, &textureCI, nullptr, &texture), "vkCreateImage");
		VkMemoryRequirements memReq;
		vkGetImageMemoryRequirements(m_Device, texture, &memReq);
		VkMemoryAllocateInfo memAI{
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.pNext = nullptr,
			.allocationSize = memReq.size,
			.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, properties)
		};
		vkCheck(vkAllocateMemory(m_Device, &memAI, nullptr, &textureMemory), "vkAllocateMemory");
		vkCheck(vkBindImageMemory(m_Device, texture, textureMemory, 0), "vkBindImageMemory");
		return {texture, textureMemory};
	}

	void VulkanEngine::createTextureImageView() {
		m_TextureImageView = createImageView(m_TextureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
	}

	void VulkanEngine::createTextureSampler() {
		VkPhysicalDeviceProperties prop;
		vkGetPhysicalDeviceProperties(m_PhysicalDevice, &prop);

		VkSamplerCreateInfo samplerCI{
			.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.magFilter = VK_FILTER_LINEAR,
			.minFilter = VK_FILTER_LINEAR,
			.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
			.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.mipLodBias = 0.0f,
			.anisotropyEnable = VK_TRUE,
			.maxAnisotropy = prop.limits.maxSamplerAnisotropy,
			.compareEnable = VK_FALSE,
			.compareOp = VK_COMPARE_OP_ALWAYS,
			.minLod = 0.0f,
			.maxLod = 0.0f,
			.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
			.unnormalizedCoordinates = VK_FALSE
		};
		vkCheck(vkCreateSampler(m_Device, &samplerCI, nullptr, &m_TextureSampler), "vkCreateSampler");
	}

	void VulkanEngine::loadModel()
	{
			tinyobj::attrib_t                attrib;
			std::vector<tinyobj::shape_t>    shapes;
			std::vector<tinyobj::material_t> materials;
			std::string                      warn, err;

			if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, k_ModelPath.data()))
			{
				throw std::runtime_error(warn + err);
			}

			for (const auto& shape : shapes) {
				for (const auto& index : shape.mesh.indices) {
					Vertex vertex{
						.Position = {
							attrib.vertices[3 * index.vertex_index + 0],
							attrib.vertices[3 * index.vertex_index + 1],
							attrib.vertices[3 * index.vertex_index + 2]
						},
						.TexCoords = {
							attrib.texcoords[2 * index.texcoord_index + 0],
							1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
						}
					};
					
					m_Vertices.push_back(vertex);
					m_Indices.emplace_back(shape.mesh.indices.size());
				}
			}
	}

	void VulkanEngine::createVertexBuffer() {
		VkDeviceSize bufferSize = sizeof(Vertex) * m_Vertices.size();
		auto [stagingBuffer, stagingBufferMemory] = createBuffer(bufferSize, VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			                                                                                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		void* dataStaging = nullptr;
		vkCheck(vkMapMemory(m_Device, stagingBufferMemory, 0, sizeof(Vertex) * m_Vertices.size(), 0, &dataStaging), "vkMapMemory");
		memcpy(dataStaging, m_Vertices.data(), sizeof(Vertex) * m_Vertices.size());
		vkUnmapMemory(m_Device, stagingBufferMemory);

		std::tie(m_VertexBuffer, m_VertexBufferMemory) = createBuffer(
		    bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		copyBuffer(stagingBuffer, m_VertexBuffer, bufferSize);

		CBK_DEBUG("Vertex Buffer created");

		vkFreeMemory(m_Device, stagingBufferMemory, nullptr);
		vkDestroyBuffer(m_Device, stagingBuffer, nullptr);
	}

	void VulkanEngine::createIndexBuffer() {
		VkDeviceSize bufferSize = sizeof(Vertex) * m_Indices.size();
		auto [stagingBuffer, stagingBufferMemory] = createBuffer(bufferSize, VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			                                                                                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		void* dataStaging = nullptr;
		vkCheck(vkMapMemory(m_Device, stagingBufferMemory, 0, sizeof(uint16_t) * m_Indices.size(), 0, &dataStaging), "vkMapMemory");
		memcpy(dataStaging, m_Indices.data(), sizeof(uint16_t) * m_Indices.size());
		vkUnmapMemory(m_Device, stagingBufferMemory);

		std::tie(m_IndexBuffer, m_IndexBufferMemory) = createBuffer(
		    bufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		copyBuffer(stagingBuffer, m_IndexBuffer, bufferSize);

		CBK_DEBUG("Index Buffer created");

		vkFreeMemory(m_Device, stagingBufferMemory, nullptr);
		vkDestroyBuffer(m_Device, stagingBuffer, nullptr);
	}

	void VulkanEngine::createUniformBuffers() {
		for (size_t i = 0; i < k_MaxFramesInFlight; i++) {
			VkDeviceSize bufferSize = sizeof(UniformBufferObject);
			auto [buffer, bufferMem] = createBuffer(bufferSize, VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT,
			                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			m_UniformBuffers.push_back(buffer);
			m_UniformBuffersMemory.push_back(bufferMem);
			void* data = nullptr;
			vkCheck(vkMapMemory(m_Device, bufferMem, 0, bufferSize, 0, &data), "vkMapMemory");
			m_UniformBuffersMapped.push_back(data);
		}

		CBK_DEBUG("Uniform Buffers created");
	}

	std::pair<VkBuffer, VkDeviceMemory> VulkanEngine::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
	                                                               VkMemoryPropertyFlags properties) {
		VkBuffer buffer = VK_NULL_HANDLE;
		VkDeviceMemory bufferMemory = VK_NULL_HANDLE;

		VkBufferCreateInfo bufferCI{ .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			                         .pNext = nullptr,
			                         .flags = 0,
			                         .size = size,
			                         .usage = usage,
			                         .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
			                         .queueFamilyIndexCount = 0,
			                         .pQueueFamilyIndices = nullptr };

		vkCheck(vkCreateBuffer(m_Device, &bufferCI, nullptr, &buffer), "vkCreateBuffer");

		VkMemoryRequirements memReq;
		vkGetBufferMemoryRequirements(m_Device, buffer, &memReq);

		VkMemoryAllocateInfo memAI{ .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			                        .pNext = nullptr,
			                        .allocationSize = memReq.size,
			                        .memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, properties) };

		vkCheck(vkAllocateMemory(m_Device, &memAI, nullptr, &bufferMemory), "vkAllocateMemory");
		vkCheck(vkBindBufferMemory(m_Device, buffer, bufferMemory, 0), "vkBindBufferMemory");
		return { buffer, bufferMemory };
	}

	void VulkanEngine::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
		VkCommandBuffer cmdBuffer = beginSingleTimeCommands();

		VkBufferCopy region{
			.srcOffset = 0,
			.dstOffset = 0,
			.size = size
		};
		vkCmdCopyBuffer(cmdBuffer, srcBuffer, dstBuffer, 1, &region);

		endSingleTimeCommands(cmdBuffer);
	}

	uint32_t VulkanEngine::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
		VkPhysicalDeviceMemoryProperties availableProperties;
		vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &availableProperties);

		for (uint32_t i = 0; i < availableProperties.memoryTypeCount; i++) {
			if ((typeFilter & (1 << i)) && (availableProperties.memoryTypes[i].propertyFlags & properties) == properties)
				return i;
		}

		CBK_FATAL("Failed to find suitable memory type!");
		return UINT32_MAX;
	}

	VkCommandBuffer VulkanEngine::beginSingleTimeCommands() {
		VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
		VkCommandBufferAllocateInfo cmdBufferAI {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.pNext = nullptr,
			.commandPool = m_SingleTimeCmdPool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1
		};
		vkCheck(vkAllocateCommandBuffers(m_Device, &cmdBufferAI, &cmdBuffer), "vkAllocateCommandBuffers");

		VkCommandBufferBeginInfo cmdBufferBI {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.pNext = nullptr,
			.flags = 0,
			.pInheritanceInfo = nullptr
		};
		vkCheck(vkBeginCommandBuffer(cmdBuffer, &cmdBufferBI), "vkBeginCommandBuffer");
		return cmdBuffer;
	}

	void VulkanEngine::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
		vkCheck(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer");

		VkSubmitInfo submitInfo{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.pNext = nullptr,
			.waitSemaphoreCount = 0,
			.pWaitSemaphores = nullptr,
			.pWaitDstStageMask = nullptr,
			.commandBufferCount = 1,
			.pCommandBuffers = &commandBuffer,
			.signalSemaphoreCount = 0,
			.pSignalSemaphores = nullptr
		};
		vkCheck(vkQueueSubmit(m_Queue, 1, &submitInfo, nullptr), "vkQueueSubmit");
		vkCheck(vkQueueWaitIdle(m_Queue), "vkQueueWaitIdle");
	}

	void VulkanEngine::copyBufferToImage(VkCommandBuffer cmdBuffer, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
		VkBufferImageCopy2 region{
			.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
			.pNext = nullptr,
			.bufferOffset = 0,
			.bufferRowLength = 0,
			.bufferImageHeight = 0,
			.imageSubresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1 },
			.imageOffset = { .x = 0, .y = 0, .z = 0 },
			.imageExtent = { .width = width, .height = height, .depth = 1 }
		};
		VkCopyBufferToImageInfo2 copyInfo{
			.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2,
			.pNext = nullptr,
			.srcBuffer = buffer,
			.dstImage = image,
			.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.regionCount = 1,
			.pRegions = &region
		};
		vkCmdCopyBufferToImage2(cmdBuffer, &copyInfo);
	}

	void VulkanEngine::createDescriptorPool() {
		std::array<VkDescriptorPoolSize, 2> poolSize{
			{ { .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = k_MaxFramesInFlight },
			  { .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = k_MaxFramesInFlight } }
		};
		VkDescriptorPoolCreateInfo descPoolCI{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.maxSets = k_MaxFramesInFlight,
			.poolSizeCount = static_cast<uint32_t>(poolSize.size()),
			.pPoolSizes = poolSize.data(),
		};
		vkCheck(vkCreateDescriptorPool(m_Device, &descPoolCI, nullptr, &m_DescriptorPool), "vkCreateDescriptorPool");
	}

	void VulkanEngine::createDescriptorSets() {
		m_DescriptorSets.resize(k_MaxFramesInFlight);
		std::vector<VkDescriptorSetLayout> layouts(k_MaxFramesInFlight, m_DescriptorSetLayout);
		VkDescriptorSetAllocateInfo descriptorSetAI{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.pNext = nullptr,
			.descriptorPool = m_DescriptorPool,
			.descriptorSetCount = k_MaxFramesInFlight,
			.pSetLayouts = layouts.data()
		};
		vkCheck(vkAllocateDescriptorSets(m_Device, &descriptorSetAI, m_DescriptorSets.data()), "vkAllocateDescriptorSets");

		for (size_t i = 0; i < k_MaxFramesInFlight; i++) {
			VkDescriptorBufferInfo bufferInfo{ .buffer = m_UniformBuffers[i], .offset = 0, .range = sizeof(UniformBufferObject) };
			VkDescriptorImageInfo imageInfo{ .sampler = m_TextureSampler,
				                             .imageView = m_TextureImageView,
				                             .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
			std::array<VkWriteDescriptorSet, 2> descriptorWrites{ { { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				                                                      .pNext = nullptr,
				                                                      .dstSet = m_DescriptorSets[i],
				                                                      .dstBinding = 0,
				                                                      .dstArrayElement = 0,
				                                                      .descriptorCount = 1,
				                                                      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				                                                      .pImageInfo = nullptr,
				                                                      .pBufferInfo = &bufferInfo,
				                                                      .pTexelBufferView = nullptr },
				                                                    { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				                                                      .pNext = nullptr,
				                                                      .dstSet = m_DescriptorSets[i],
				                                                      .dstBinding = 1,
				                                                      .dstArrayElement = 0,
				                                                      .descriptorCount = 1,
				                                                      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				                                                      .pImageInfo = &imageInfo,
				                                                      .pBufferInfo = nullptr,
				                                                      .pTexelBufferView = nullptr } } };
			vkUpdateDescriptorSets(m_Device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
		}
	}

	void VulkanEngine::createCommandBuffers() {
		m_CmdBuffers.resize(k_MaxFramesInFlight);
		VkCommandBufferAllocateInfo cmdBufferAI {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.pNext = nullptr,
			.commandPool = m_CmdPool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = k_MaxFramesInFlight
		};
		vkCheck(vkAllocateCommandBuffers(m_Device, &cmdBufferAI, m_CmdBuffers.data()), "vkAllocateCommandBuffers");
	}

	void VulkanEngine::recordCommandBuffer(uint32_t frameIndex) {
		VkCommandBufferBeginInfo cmdBufferBI {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.pNext = nullptr,
			.flags = 0,
			.pInheritanceInfo = nullptr
		};
		vkCheck(vkBeginCommandBuffer(m_CmdBuffers[frameIndex], &cmdBufferBI), "vkBeginCommandBuffer");
	}

	void VulkanEngine::transitionImageLayout(VkCommandBuffer buffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
	                                         VkAccessFlags2 srcAccessMask, VkAccessFlags2 dstAccessMask, VkPipelineStageFlags2 srcStageMask,
	                                         VkPipelineStageFlags2 dstStageMask, VkImageAspectFlags aspectFlags) {
		VkImageMemoryBarrier2 barrier{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			                           .pNext = nullptr,
			                           .srcStageMask = srcStageMask,
			                           .srcAccessMask = srcAccessMask,
			                           .dstStageMask = dstStageMask,
			                           .dstAccessMask = dstAccessMask,
			                           .oldLayout = oldLayout,
			                           .newLayout = newLayout,
			                           .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			                           .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			                           .image = image,
			                           .subresourceRange = { .aspectMask = aspectFlags,
			                                                 .baseMipLevel = 0,
			                                                 .levelCount = 1,
			                                                 .baseArrayLayer = 0,
			                                                 .layerCount = 1 } };

		VkDependencyInfo dependencyInfo{
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.pNext = nullptr,
			.dependencyFlags = 0,
			.memoryBarrierCount = 0,
			.pMemoryBarriers = nullptr,
			.bufferMemoryBarrierCount = 0,
			.pBufferMemoryBarriers = nullptr,
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &barrier
		};

		vkCmdPipelineBarrier2(buffer, &dependencyInfo);
	}

	void VulkanEngine::createSyncObjects() {
		m_PresentCompleteSemaphores.resize(k_MaxFramesInFlight);
		m_DrawFences.resize(k_MaxFramesInFlight);

		VkSemaphoreCreateInfo semaphoreCI {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0
		};

		VkFenceCreateInfo fenceCI {
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.pNext = nullptr,
			.flags = VK_FENCE_CREATE_SIGNALED_BIT
		};

		createRenderFinishedSemaphores();

		for (uint32_t i = 0; i < k_MaxFramesInFlight; i++) {
			vkCheck(vkCreateSemaphore(m_Device, &semaphoreCI, nullptr, &m_PresentCompleteSemaphores[i]), "vkCreateSemaphore");
			vkCheck(vkCreateFence(m_Device, &fenceCI, nullptr, &m_DrawFences[i]), "vkCreateFence");
		}
	}

	void VulkanEngine::createRenderFinishedSemaphores() {
		for (const auto& semaphore : m_RenderFinishedSemaphores) {
			vkDestroySemaphore(m_Device, semaphore, nullptr);
		}
		m_RenderFinishedSemaphores.assign(m_Images.size(), VK_NULL_HANDLE);

		VkSemaphoreCreateInfo semaphoreCI {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0
		};

		for (auto& semaphore : m_RenderFinishedSemaphores) {
			vkCheck(vkCreateSemaphore(m_Device, &semaphoreCI, nullptr, &semaphore), "vkCreateSemaphore");
		}
	}

	void VulkanEngine::updateUniformBuffer(uint32_t currentImage) {
		static auto startTime = std::chrono::high_resolution_clock::now();
		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

		UniformBufferObject ubo{ .Model = math::rotateZ(10.0f * time),
			                     .View = math::lookAt({ 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }),
			                     .Proj = math::perspective(45.0f, static_cast<float>(m_Extent.width) / static_cast<float>(m_Extent.height),
			                                               0.1f, 10.0f) };

		ubo.Proj[1][1] *= -1;

		memcpy(m_UniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
	}

	void VulkanEngine::recreateSwapchain() {
		vkDeviceWaitIdle(m_Device);
		cleanupSwapchain();
		createImageViews();
		createRenderFinishedSemaphores();
		createDepthResources();
	}

	void VulkanEngine::cleanupSwapchain() {
		if (m_DepthImageView != VK_NULL_HANDLE) {
			vkDestroyImageView(m_Device, m_DepthImageView, nullptr);
			m_DepthImageView = VK_NULL_HANDLE;
			CBK_DEBUG("Texture Image View destroyed");
		}

		if (m_DepthImageMemory != VK_NULL_HANDLE) {
			vkFreeMemory(m_Device, m_DepthImageMemory, nullptr);
			m_DepthImageMemory = VK_NULL_HANDLE;
			CBK_DEBUG("Texture deallocated");
		}

		if (m_DepthImage != VK_NULL_HANDLE) {
			vkDestroyImage(m_Device, m_DepthImage, nullptr);
			m_DepthImage = VK_NULL_HANDLE;
			CBK_DEBUG("Index Buffer destroyed");
		}
		
		for (const auto& view: m_ImageViews) {
			vkDestroyImageView(m_Device, view, nullptr);
		}
		m_ImageViews.clear();
		VkSwapchainKHR oldSwapchain = m_Swapchain;
		createSwapchain(oldSwapchain);
		vkDestroySwapchainKHR(m_Device, oldSwapchain, nullptr);
	}
} // namespace lab::vk
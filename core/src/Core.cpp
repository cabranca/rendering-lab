#include "Core.h"

#include <SDL3/SDL_vulkan.h>
#include <array>
#include <print>
#include <volk/volk.h>

#include "Check.h"
#include "Utils.h"
#include "vulkan/vulkan_core.h"

namespace lab {

	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT Severity,
	                                                    VkDebugUtilsMessageTypeFlagsEXT Type,
	                                                    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
		std::println("Debug callback: {}", pCallbackData->pMessage);
		std::println("  Severity {}", GetDebugSeverityStr(Severity));
		std::println("  Type {}", GetDebugType(Type));
		std::println("  Objects ");

		for (uint32_t i = 0; i < pCallbackData->objectCount; i++) {
			std::println("{}", pCallbackData->pObjects[i].objectHandle);
		}

		std::println("");

		return VK_FALSE; // The calling function should not be aborted
	}

	Core::~Core() {
		shutdown();
	}

	void Core::init(std::string_view appName, SDL_Window* window) {
		m_Instance.init(appName);
		createDebugCallback();
		createSurface(window);
		m_DeviceManager.init(m_Instance.getInstance(), m_Surface);
		m_QueueFamily = m_DeviceManager.selectDevice(VK_QUEUE_GRAPHICS_BIT, true);
		createDevice();
		m_Swapchain.init(m_Device, m_DeviceManager.getSelectedDevice(), m_Surface, m_QueueFamily);
		createCommandPool();
		m_Queue.init(m_Device, m_Swapchain.getSwapchain(), m_QueueFamily, 0, m_Swapchain.getNumImages());
		createCommandBuffers(1, &m_CopyCmdBuffer);
	}

	void Core::shutdown() {
		vkFreeCommandBuffers(m_Device, m_CmdPool, 1, &m_CopyCmdBuffer);

		m_Queue.shutdown();

		vkDestroyCommandPool(m_Device, m_CmdPool, nullptr);
		std::println("Command Pool destroyed");

		m_Swapchain.shutdown(m_Device);

		vkDestroyDevice(m_Device, nullptr);
		std::println("Vulkan Device destroyed");

		vkDestroySurfaceKHR(m_Instance.getInstance(), m_Surface, nullptr);
		std::println("SDL Surface destroyed");

		vkDestroyDebugUtilsMessengerEXT(m_Instance.getInstance(), m_DebugMessenger, nullptr);
		std::println("Debug messenger destroyed");

		m_Instance.shutdown();
	}

	VkDevice Core::getDevice() const {
		return m_Device;
	}

	VkExtent2D Core::getExtent() const {
		return m_Swapchain.getExtent();
	}

	uint32_t Core::getNumImages() const {
		return m_Swapchain.getNumImages();
	}

	VkImage Core::getImage(uint32_t index) const {
		return m_Swapchain.getImage(index);
	}

	void Core::createCommandBuffers(uint32_t numImages, VkCommandBuffer* cmdBuffers) {
		VkCommandBufferAllocateInfo cmdBufferAI {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = m_CmdPool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = numImages
		};

		chk(vkAllocateCommandBuffers(m_Device, &cmdBufferAI, cmdBuffers));
		std::println("Command Buffers allocated");
	}

	void Core::freeCommandBuffers(uint32_t bufferCount, const VkCommandBuffer* cmdBuffers) {
		vkFreeCommandBuffers(m_Device, m_CmdPool, bufferCount, cmdBuffers);
	}

	Queue* Core::getQueue() {
		return &m_Queue;
	}

	BufferAndMemory Core::createBuffer(VkDeviceSize size, VkBufferUsageFlags flags, VkMemoryPropertyFlags properties) {
		VkBufferCreateInfo bufferCI{
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = size,
			.usage = flags,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE
		};

		BufferAndMemory res;

		chk(vkCreateBuffer(m_Device, &bufferCI, nullptr, &res.Buffer));
		
		VkMemoryRequirements memReqs{};
		vkGetBufferMemoryRequirements(m_Device, res.Buffer, &memReqs);

		res.AllocationSize = memReqs.size;

		uint32_t memoryTypeIndex{ UINT32_MAX };
		const auto& memProps = m_DeviceManager.getSelectedDevice().MemProps;
		for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
			const auto& memType = memProps.memoryTypes[i];
			uint32_t currBitmask = (1 << i);
			bool isCurrMemTypeSupported = ((memReqs.memoryTypeBits & properties));
			bool hasRequiredMemProps = ((memType.propertyFlags & properties) == properties);
			if (isCurrMemTypeSupported && hasRequiredMemProps) {
				memoryTypeIndex = i;
				break;
			}
		}
		
		assert(memoryTypeIndex != UINT32_MAX);

		VkMemoryAllocateInfo allocateInfo{
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.allocationSize = memReqs.size,
			.memoryTypeIndex = memoryTypeIndex
		};
		chk(vkAllocateMemory(m_Device, &allocateInfo, nullptr, &res.Mem));

		chk(vkBindBufferMemory(m_Device, res.Buffer, res.Mem, 0));

		return res;
	}

	BufferAndMemory Core::createVertexBuffer(const void* vertexData, size_t vertexDataSize) {
		VkBufferUsageFlags flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		VkMemoryPropertyFlags memProps = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		
		
		BufferAndMemory stagingVB = createBuffer(vertexDataSize, flags, memProps);

		void* pMem = nullptr;
		VkDeviceSize offset = 0;
		VkMemoryMapFlags memFlags = 0;
		chk(vkMapMemory(m_Device, stagingVB.Mem, offset, stagingVB.AllocationSize, memFlags, &pMem));

		memcpy(pMem, vertexData, vertexDataSize);

		vkUnmapMemory(m_Device, stagingVB.Mem);

		flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		memProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		BufferAndMemory vb = createBuffer(vertexDataSize, flags, memProps);

		copyBuffer(vb.Buffer, stagingVB.Buffer, vertexDataSize);

		stagingVB.destroy(m_Device);

		return vb;
	}

	void Core::copyBuffer(VkBuffer dst, VkBuffer src, size_t size) {
		beginCommandBuffer(m_CopyCmdBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

		VkBufferCopy bufferCopy{
			.srcOffset = 0,
			.dstOffset = 0,
			.size = size
		};

		vkCmdCopyBuffer(m_CopyCmdBuffer, src, dst, 1, &bufferCopy);
		vkEndCommandBuffer(m_CopyCmdBuffer);
		m_Queue.submitSync(m_CopyCmdBuffer);
		m_Queue.waitIdle();
	}

	VkRenderPass Core::createSimpleRenderPass() {
		VkAttachmentDescription attachmentDesc{
			.flags = 0,
			.format = m_Swapchain.getSurfaceFormat().format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
		};

		VkAttachmentReference attachmentRef{
			.attachment = 0,
			.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		};

		VkSubpassDescription subpassDesc{
			.flags = 0,
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.inputAttachmentCount = 0,
			.pInputAttachments = nullptr,
			.colorAttachmentCount = 1,
			.pColorAttachments = &attachmentRef,
			.pResolveAttachments = nullptr,
			.pDepthStencilAttachment = nullptr,
			.preserveAttachmentCount = 0,
			.pPreserveAttachments = nullptr
		};


		VkRenderPassCreateInfo renderPassCI{
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.flags = 0,
			.attachmentCount = 1,
			.pAttachments = &attachmentDesc,
			.subpassCount = 1,
			.pSubpasses = &subpassDesc,
			.dependencyCount = 0,
			.pDependencies = nullptr
		};

		VkRenderPass res;

		chk(vkCreateRenderPass(m_Device, &renderPassCI, nullptr, &res));

		return res;
	}
	
	std::vector<VkFramebuffer> Core::createFrameBuffers(VkRenderPass renderPass) {
		std::vector<VkFramebuffer> res{m_Swapchain.getNumImages()};
		VkExtent2D extent = m_Swapchain.getExtent();

		for (uint32_t i = 0; i < m_Swapchain.getNumImages(); i++) {
			VkImageView imageView = m_Swapchain.getImageView(i);
			VkFramebufferCreateInfo frameBufferCI{
				.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
				.flags = 0,
				.renderPass = renderPass,
				.attachmentCount = 1,
				.pAttachments = &imageView,
				.width = extent.width,
				.height = extent.height,
				.layers = 1
			};

			chk(vkCreateFramebuffer(m_Device, &frameBufferCI, nullptr, &res.at(i)));
		}

		return res;
	}

	void Core::destroyFrameBuffers(const std::vector<VkFramebuffer>& frameBuffers) {
		for (VkFramebuffer frameBuffer : frameBuffers)
			vkDestroyFramebuffer(m_Device, frameBuffer, nullptr);
	}

	bool Core::recreateSwapchain() {
		m_Queue.waitIdle();

		m_DeviceManager.refreshSurfaceCaps(m_Surface);
		VkExtent2D extent = m_DeviceManager.getSelectedDevice().SurfaceCaps.currentExtent;
		if (extent.width == 0 || extent.height == 0)
			return false; // Minimized: nothing to render to, skip.

		m_Swapchain.shutdown(m_Device);
		m_Swapchain.init(m_Device, m_DeviceManager.getSelectedDevice(), m_Surface, m_QueueFamily);
		m_Queue.setSwapchain(m_Swapchain.getSwapchain());

		return true;
	}

	void Core::beginCommandBuffer(VkCommandBuffer buffer, VkCommandBufferUsageFlags flags) {
		VkCommandBufferBeginInfo bufferBI{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = flags,
			.pInheritanceInfo = nullptr
		};

		chk(vkBeginCommandBuffer(buffer, &bufferBI));
	}

	void Core::createDebugCallback() {
		VkDebugUtilsMessengerCreateInfoEXT MessengerCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
			.pNext = NULL,
			.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
			.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			               VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
			.pfnUserCallback = DebugCallback,
			.pUserData = NULL
		};

		chk(vkCreateDebugUtilsMessengerEXT(m_Instance.getInstance(), &MessengerCreateInfo, nullptr, &m_DebugMessenger));

		std::println("Debug utils created");
	}

	void Core::createSurface(SDL_Window* window) {
		if (!SDL_Vulkan_CreateSurface(window, m_Instance.getInstance(), nullptr, &m_Surface)) {
			std::println("SDL_Vulkan_CreateSurface failed: {}", SDL_GetError());
			throw std::runtime_error("Failed to create Vulkan surface");
		}
		std::println("SDL Surface created");
	}

	void Core::createDevice() {
		float qPriorities{ 0.f };
		VkQueue queue;
		VkDeviceQueueCreateInfo queueCI{ .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			                             .queueFamilyIndex = m_QueueFamily,
			                             .queueCount = 1,
			                             .pQueuePriorities = &qPriorities };

		std::array<const char*, 2> ext{ VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME };
		VkPhysicalDeviceFeatures feat{ /*.geometryShader = true, */.tessellationShader = true }; // Geometry shader not available in Macbook Air M5

		const auto& physicalDevice = m_DeviceManager.getSelectedDevice();
		// if (physicalDevice.Features.geometryShader != VK_TRUE) {
		// 	std::println("Geometry Shader not supported!");
		// 	exit(1);
		// }
		if (physicalDevice.Features.tessellationShader != VK_TRUE) {
			std::println("Tesselation Shader not supported!");
			exit(1);
		}

		VkDeviceCreateInfo deviceCI{ .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			                         .queueCreateInfoCount = 1,
			                         .pQueueCreateInfos = &queueCI,
			                         .enabledExtensionCount = ext.size(),
			                         .ppEnabledExtensionNames = ext.data(),
			                         .pEnabledFeatures = &feat };

		chk(vkCreateDevice(physicalDevice.Device, &deviceCI, nullptr, &m_Device));
		volkLoadDevice(m_Device);
		std::println("Vulkan Device created");
	}

	void Core::createCommandPool() {
		VkCommandPoolCreateInfo poolCI {
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // allow re-recording on resize
			.queueFamilyIndex = m_QueueFamily
		};

		chk(vkCreateCommandPool(m_Device, &poolCI, nullptr, &m_CmdPool));
		std::println("Command Pool created");
	}
} // namespace lab
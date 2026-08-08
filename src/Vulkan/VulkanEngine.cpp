#include <string_view>

#include <vulkan/vk_enum_string_helper.h>
#include <volk/volk.h>

#include "VulkanCommands.h"
#include "VulkanEngine.h"
#include "Utils.h"

namespace lab::vk {

	VulkanEngine::VulkanEngine(const Window& window)
	    : m_Instance(window), m_Device(m_Instance), m_Swapchain(&m_Device, window.getWindowHandle()),
	      m_Pipeline(&m_Device, m_Swapchain, k_MaxFramesInFlight), m_Model(VulkanModel::loadOBJ(m_Device, k_ModelPath)) {
		
		createCommandBuffers();
		createSyncObjects();
	}

	void VulkanEngine::shutdown() {
		m_Device.waitIdle();

		auto vulkanDevice = m_Device.getDevice();

		for (const auto& semaphore: m_PresentCompleteSemaphores) {
			vkDestroySemaphore(vulkanDevice, semaphore, nullptr);
		}
		m_PresentCompleteSemaphores.clear();

		for (const auto& fence: m_DrawFences) {
			vkDestroyFence(vulkanDevice, fence, nullptr);
		}
		m_DrawFences.clear();
	}

	void VulkanEngine::drawFrame() {
		auto vulkanDevice = m_Device.getDevice();

		// While the window is minimized the swapchain extent collapses to zero; there is nothing to
		// render, so skip the frame rather than build zero-area attachments.
		VkExtent2D extent = m_Swapchain.getExtent();
		if (extent.width == 0 || extent.height == 0)
			return;

		m_Pipeline.updateUniformBuffer(m_FrameIndex, extent);

		vkCheck(vkWaitForFences(vulkanDevice, 1, &m_DrawFences[m_FrameIndex], VK_TRUE, UINT64_MAX), "vkWaitForFences");

		auto imageIndex = m_Swapchain.acquireImage(m_PresentCompleteSemaphores[m_FrameIndex]);

		// The swapchain was out of date and got rebuilt; the fence stays signalled and the semaphore
		// unsignalled, so simply retry on the next frame.
		if (imageIndex == UINT32_MAX)
			return;

		vkCheck(vkResetFences(vulkanDevice, 1, &m_DrawFences[m_FrameIndex]), "vkResetFences");

		vkCheck(vkResetCommandBuffer(m_CmdBuffers[m_FrameIndex], 0), "vkResetCommandBuffer");
		recordCommandBuffer(m_FrameIndex);

		VulkanCommands::transitionImageLayout(m_CmdBuffers[m_FrameIndex], m_Swapchain.getColorImage().getImage(), VK_IMAGE_LAYOUT_UNDEFINED,
		                                      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_NONE,
		                                      VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		                                      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_IMAGE_ASPECT_COLOR_BIT, 1);
		VulkanCommands::transitionImageLayout(m_CmdBuffers[m_FrameIndex], m_Swapchain.getImage(imageIndex), VK_IMAGE_LAYOUT_UNDEFINED,
		                                      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_2_NONE,
		                                      VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		                                      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_IMAGE_ASPECT_COLOR_BIT, 1);
		VulkanCommands::transitionImageLayout(m_CmdBuffers[m_FrameIndex], m_Swapchain.getDepthImage().getImage(), VK_IMAGE_LAYOUT_UNDEFINED,
		                                      VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		                                      VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		                                      VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
		                                      VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
		                                      VK_IMAGE_ASPECT_DEPTH_BIT, 1);

		VkRenderingAttachmentInfo colorAttachmentInfo{ .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			                                           .pNext = nullptr,
			                                           .imageView = m_Swapchain.getColorImage().getView(),
			                                           .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			                                           .resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT,
			                                           .resolveImageView = m_Swapchain.getView(imageIndex),
			                                           .resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			                                           .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			                                           .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			                                           .clearValue = {
			                                               .color = { 0.0f, 0.0f, 0.0f, 1.0f },
			                                           } };

		VkRenderingAttachmentInfo depthAttachmentInfo{ .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			                                           .pNext = nullptr,
			                                           .imageView = m_Swapchain.getDepthImage().getView(),
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
			                           .renderArea = { .offset = { 0, 0 }, .extent = m_Swapchain.getExtent() },
			                           .layerCount = 1,
			                           .colorAttachmentCount = 1,
			                           .pColorAttachments = &colorAttachmentInfo,
			                           .pDepthAttachment = &depthAttachmentInfo };

		vkCmdBeginRendering(m_CmdBuffers[m_FrameIndex], &renderingInfo);

		m_Pipeline.bind(m_CmdBuffers[m_FrameIndex], m_FrameIndex);

		VkViewport viewport{ .x = 0.0f,
			                 .y = 0.0f,
			                 .width = static_cast<float>(m_Swapchain.getExtent().width),
			                 .height = static_cast<float>(m_Swapchain.getExtent().height),
			                 .minDepth = 0.0f,
			                 .maxDepth = 1.0f };
		VkRect2D scissor{
			.offset = { .x = 0, .y = 0 },
			.extent = m_Swapchain.getExtent(),
		};

		vkCmdSetViewport(m_CmdBuffers[m_FrameIndex], 0, 1, &viewport);
		vkCmdSetScissor(m_CmdBuffers[m_FrameIndex], 0, 1, &scissor);

		m_Model.draw(m_CmdBuffers[m_FrameIndex]);

		vkCmdEndRendering(m_CmdBuffers[m_FrameIndex]);

		VulkanCommands::transitionImageLayout(
		    m_CmdBuffers[m_FrameIndex], m_Swapchain.getImage(imageIndex), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_2_NONE,
		    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, 1);

		vkCheck(vkEndCommandBuffer(m_CmdBuffers[m_FrameIndex]), "vkEndCommandBuffer");

		std::vector<VkPipelineStageFlags> waitDstStageMask = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

		m_Device.getQueue().submitCommands(m_CmdBuffers[m_FrameIndex], m_DrawFences[m_FrameIndex],
		                                   m_PresentCompleteSemaphores[m_FrameIndex], m_Swapchain.getSemaphore(imageIndex),
		                                   waitDstStageMask);
		auto result = m_Device.getQueue().present(m_Swapchain.getSemaphore(imageIndex), m_Swapchain.getSwapchain(), imageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
			m_Swapchain.recreateSwapchain();

		m_FrameIndex = (m_FrameIndex + 1) % k_MaxFramesInFlight;
	}

	void VulkanEngine::createCommandBuffers() {
		m_CmdBuffers = m_Device.getQueue().allocateCommandBuffers(k_MaxFramesInFlight);
	}

	void VulkanEngine::recordCommandBuffer(uint32_t frameIndex) {
		VkCommandBufferBeginInfo cmdBufferBI{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .pNext = nullptr, .flags = 0, .pInheritanceInfo = nullptr
		};
		vkCheck(vkBeginCommandBuffer(m_CmdBuffers[frameIndex], &cmdBufferBI), "vkBeginCommandBuffer");
	}

	void VulkanEngine::createSyncObjects() {
		m_PresentCompleteSemaphores.resize(k_MaxFramesInFlight);
		m_DrawFences.resize(k_MaxFramesInFlight);

		VkSemaphoreCreateInfo semaphoreCI{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = nullptr, .flags = 0 };

		VkFenceCreateInfo fenceCI{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .pNext = nullptr, .flags = VK_FENCE_CREATE_SIGNALED_BIT };

		for (uint32_t i = 0; i < k_MaxFramesInFlight; i++) {
			vkCheck(vkCreateSemaphore(m_Device.getDevice(), &semaphoreCI, nullptr, &m_PresentCompleteSemaphores[i]), "vkCreateSemaphore");
			vkCheck(vkCreateFence(m_Device.getDevice(), &fenceCI, nullptr, &m_DrawFences[i]), "vkCreateFence");
		}
	}
} // namespace lab::vk
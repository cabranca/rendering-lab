#include "VulkanCore.h"

#include <volk/volk.h>

#include "Check.h"
#include "ModelImporter.h"

namespace lab {

	void VulkanCore::init(std::string_view applicationName) {
		m_VulkanCtx.init(applicationName);
		m_Window.init("rendering-lab", 1600, 900);
		m_Swapchain.init(m_VulkanCtx, m_Window);

		auto device = m_VulkanCtx.getDevice();

		// Command pool + one command buffer per frame in flight.
		VkCommandPoolCreateInfo commandPoolCI{ .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			                                   .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			                                   .queueFamilyIndex = m_VulkanCtx.getQueueFamily() };
		chk(vkCreateCommandPool(device, &commandPoolCI, nullptr, &m_CommandPool));
		VkCommandBufferAllocateInfo cbAI{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			                              .commandPool = m_CommandPool,
			                              .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			                              .commandBufferCount = maxFramesInFlight };
		chk(vkAllocateCommandBuffers(device, &cbAI, m_CommandBuffers.data()));

		// Per-frame fences + image-acquired semaphores.
		VkSemaphoreCreateInfo semaphoreCI{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		VkFenceCreateInfo fenceCI{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT };
		for (uint32_t i = 0; i < maxFramesInFlight; i++) {
			chk(vkCreateFence(device, &fenceCI, nullptr, &m_Fences[i]));
			chk(vkCreateSemaphore(device, &semaphoreCI, nullptr, &m_ImageAcquiredSemaphores[i]));
		}
		// Render-complete semaphores are per swapchain image (signalled at present).
		m_RenderCompleteSemaphores.resize(m_Swapchain.getImageCount());
		for (auto& semaphore: m_RenderCompleteSemaphores) {
			chk(vkCreateSemaphore(device, &semaphoreCI, nullptr, &semaphore));
		}
	}

	VertexBuffer VulkanCore::loadModel(std::string_view path) {
		ModelImporter importer;
		importer.import(path);
		VertexBuffer mesh;
		mesh.init(m_VulkanCtx, importer.getVertices(), importer.getIndices());
		return mesh;
	}

	VkCommandBuffer VulkanCore::beginFrame() {
		auto device = m_VulkanCtx.getDevice();

		chk(vkWaitForFences(device, 1, &m_Fences[m_FrameIndex], VK_TRUE, UINT64_MAX));

		VkResult acquire = vkAcquireNextImageKHR(device, m_Swapchain.getSwapchain(), UINT64_MAX, m_ImageAcquiredSemaphores[m_FrameIndex],
		                                         VK_NULL_HANDLE, &m_ImageIndex);
		if (acquire == VK_ERROR_OUT_OF_DATE_KHR) {
			recreateSwapchain();
			return VK_NULL_HANDLE;
		}
		if (acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR) {
			chk(acquire);
		}

		chk(vkResetFences(device, 1, &m_Fences[m_FrameIndex]));

		auto cb = m_CommandBuffers[m_FrameIndex];
		chk(vkResetCommandBuffer(cb, 0));
		VkCommandBufferBeginInfo cbBI{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			                           .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
		chk(vkBeginCommandBuffer(cb, &cbBI));

		VkExtent2D extent = m_Swapchain.getExtent();

		// Transition color + depth to attachment layout.
		std::array<VkImageMemoryBarrier2, 2> toAttachment{
			VkImageMemoryBarrier2{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			                       .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			                       .srcAccessMask = 0,
			                       .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			                       .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
			                       .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			                       .newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
			                       .image = m_Swapchain.getImages()[m_ImageIndex],
			                       .subresourceRange{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 } },
			VkImageMemoryBarrier2{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			                       .srcStageMask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
			                       .srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			                       .dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
			                       .dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			                       .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			                       .newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
			                       .image = m_Swapchain.getDepthImage(),
			                       .subresourceRange{ .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
			                                          .levelCount = 1,
			                                          .layerCount = 1 } }
		};
		VkDependencyInfo dep{ .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			                  .imageMemoryBarrierCount = 2,
			                  .pImageMemoryBarriers = toAttachment.data() };
		vkCmdPipelineBarrier2(cb, &dep);

		VkRenderingAttachmentInfo colorAttachment{ .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			                                       .imageView = m_Swapchain.getImageViews()[m_ImageIndex],
			                                       .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
			                                       .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			                                       .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			                                       .clearValue{ .color{ 0.0f, 0.0f, 0.0f, 1.0f } } };
		VkRenderingAttachmentInfo depthAttachment{ .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			                                       .imageView = m_Swapchain.getDepthImageView(),
			                                       .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
			                                       .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			                                       .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			                                       .clearValue{ .depthStencil = { 1.0f, 0 } } };
		VkRenderingInfo renderingInfo{ .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
			                           .renderArea{ .extent = extent },
			                           .layerCount = 1,
			                           .colorAttachmentCount = 1,
			                           .pColorAttachments = &colorAttachment,
			                           .pDepthAttachment = &depthAttachment };
		vkCmdBeginRendering(cb, &renderingInfo);

		VkViewport vp{
			.width = static_cast<float>(extent.width), .height = static_cast<float>(extent.height), .minDepth = 0.0f, .maxDepth = 1.0f
		};
		vkCmdSetViewport(cb, 0, 1, &vp);
		VkRect2D scissor{ .extent = extent };
		vkCmdSetScissor(cb, 0, 1, &scissor);

		return cb;
	}

	void VulkanCore::endFrame() {
		auto cb = m_CommandBuffers[m_FrameIndex];

		vkCmdEndRendering(cb);

		VkImageMemoryBarrier2 toPresent{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			                             .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			                             .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
			                             .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			                             .dstAccessMask = 0,
			                             .oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
			                             .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			                             .image = m_Swapchain.getImages()[m_ImageIndex],
			                             .subresourceRange{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 } };
		VkDependencyInfo dep{ .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			                  .imageMemoryBarrierCount = 1,
			                  .pImageMemoryBarriers = &toPresent };
		vkCmdPipelineBarrier2(cb, &dep);

		chk(vkEndCommandBuffer(cb));

		VkPipelineStageFlags waitStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VkSubmitInfo submitInfo{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &m_ImageAcquiredSemaphores[m_FrameIndex],
			.pWaitDstStageMask = &waitStages,
			.commandBufferCount = 1,
			.pCommandBuffers = &cb,
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = &m_RenderCompleteSemaphores[m_ImageIndex],
		};
		chk(vkQueueSubmit(m_VulkanCtx.getQueue(), 1, &submitInfo, m_Fences[m_FrameIndex]));

		VkSwapchainKHR swapchain = m_Swapchain.getSwapchain();
		VkPresentInfoKHR presentInfo{ .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			                          .waitSemaphoreCount = 1,
			                          .pWaitSemaphores = &m_RenderCompleteSemaphores[m_ImageIndex],
			                          .swapchainCount = 1,
			                          .pSwapchains = &swapchain,
			                          .pImageIndices = &m_ImageIndex };
		VkResult present = vkQueuePresentKHR(m_VulkanCtx.getQueue(), &presentInfo);

		m_FrameIndex = (m_FrameIndex + 1) % maxFramesInFlight;

		if (present == VK_ERROR_OUT_OF_DATE_KHR || present == VK_SUBOPTIMAL_KHR || m_Resized) {
			m_Resized = false;
			recreateSwapchain();
		} else {
			chk(present);
		}
	}

	void VulkanCore::recreateSwapchain() {
		auto device = m_VulkanCtx.getDevice();
		chk(vkDeviceWaitIdle(device));

		m_Swapchain.recreate(m_VulkanCtx, m_Window);

		// The image count may have changed, so rebuild the per-image semaphores.
		for (auto& semaphore: m_RenderCompleteSemaphores) {
			vkDestroySemaphore(device, semaphore, nullptr);
		}
		VkSemaphoreCreateInfo semaphoreCI{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		m_RenderCompleteSemaphores.resize(m_Swapchain.getImageCount());
		for (auto& semaphore: m_RenderCompleteSemaphores) {
			chk(vkCreateSemaphore(device, &semaphoreCI, nullptr, &semaphore));
		}
	}

	void VulkanCore::shutdown() {
		auto device = m_VulkanCtx.getDevice();
		chk(vkDeviceWaitIdle(device));

		for (uint32_t i = 0; i < maxFramesInFlight; i++) {
			vkDestroyFence(device, m_Fences[i], nullptr);
			vkDestroySemaphore(device, m_ImageAcquiredSemaphores[i], nullptr);
		}
		for (auto& semaphore: m_RenderCompleteSemaphores) {
			vkDestroySemaphore(device, semaphore, nullptr);
		}
		vkDestroyCommandPool(device, m_CommandPool, nullptr);

		m_Swapchain.shutdown(m_VulkanCtx);
		m_Window.shutdown();
		m_VulkanCtx.shutdown();
	}
} // namespace lab

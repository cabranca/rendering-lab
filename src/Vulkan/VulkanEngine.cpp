#include <cstring>
#include <string>
#include <string_view>

#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>


#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include "VulkanCommands.h"
#include "VulkanEngine.h"
#include "Utils.h"

namespace lab::vk {

	VulkanEngine::VulkanEngine(const Window& window) : m_Device(window), m_Swapchain(&m_Device, window.getWindowHandle()) {
		m_Texture = VulkanTexture(k_TexturePath, &m_Device);
	}

	void VulkanEngine::init(const Window& window) {
		createDescriptorSetLayout();
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
		m_Device.waitIdle();

		auto vulkanDevice = m_Device.getDevice();

		for (const auto& semaphore : m_PresentCompleteSemaphores) {
			vkDestroySemaphore(vulkanDevice, semaphore, nullptr);
		}
		m_PresentCompleteSemaphores.clear();

		for (const auto& fence : m_DrawFences) {
			vkDestroyFence(vulkanDevice, fence, nullptr);
		}
		m_DrawFences.clear();

		if (m_DescriptorPool != VK_NULL_HANDLE) {
			vkDestroyDescriptorPool(vulkanDevice, m_DescriptorPool, nullptr);
			m_DescriptorPool = VK_NULL_HANDLE;
			CBK_DEBUG("Desriptor Pool destroyed");
		}

		for (size_t i = 0; i < k_MaxFramesInFlight; i++) {
			m_UniformBuffers[i].unmap();
		}

		if (m_GraphicsPipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(vulkanDevice, m_GraphicsPipeline, nullptr);
			m_GraphicsPipeline = VK_NULL_HANDLE;
			CBK_DEBUG("Vulkan Graphics Pipeline destroyed");
		}

		if (m_PipelineLayout != VK_NULL_HANDLE) {
			vkDestroyPipelineLayout(vulkanDevice, m_PipelineLayout, nullptr);
			m_PipelineLayout = VK_NULL_HANDLE;
			CBK_DEBUG("Vulkan Pipeline Layout destroyed");
		}

		if (m_DescriptorSetLayout != VK_NULL_HANDLE) {
			vkDestroyDescriptorSetLayout(vulkanDevice, m_DescriptorSetLayout, nullptr);
			m_DescriptorSetLayout = VK_NULL_HANDLE;
			CBK_DEBUG("Descriptor Set Layout destroyed");
		}
	}

	void VulkanEngine::drawFrame() {
		auto vulkanDevice = m_Device.getDevice();
		updateUniformBuffer(m_FrameIndex);

		vkCheck(vkWaitForFences(vulkanDevice, 1, &m_DrawFences[m_FrameIndex], VK_TRUE, UINT64_MAX), "vkWaitForFences");
		
		auto imageIndex = m_Swapchain.acquireImage(m_PresentCompleteSemaphores[m_FrameIndex]);

		vkCheck(vkResetFences(vulkanDevice, 1, &m_DrawFences[m_FrameIndex]), "vkResetFences");

		vkCheck(vkResetCommandBuffer(m_CmdBuffers[m_FrameIndex], 0), "vkResetCommandBuffer");
		recordCommandBuffer(m_FrameIndex);

		VulkanCommands::transitionImageLayout(m_CmdBuffers[m_FrameIndex], m_Swapchain.getColorImage().getImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		                      VK_ACCESS_NONE, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		                      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_IMAGE_ASPECT_COLOR_BIT, 1);
		VulkanCommands::transitionImageLayout(m_CmdBuffers[m_FrameIndex], m_Swapchain.getImage(imageIndex), VK_IMAGE_LAYOUT_UNDEFINED,
		                      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_2_NONE, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		                      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		                      VK_IMAGE_ASPECT_COLOR_BIT, 1);
		VulkanCommands::transitionImageLayout(m_CmdBuffers[m_FrameIndex], m_Swapchain.getDepthImage().getImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
		                      VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
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
			                                      .clearValue = { .color = { 0.0f, 0.0f, 0.0f, 1.0f }, } };

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

		vkCmdBindPipeline(m_CmdBuffers[m_FrameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipeline);

		VkDeviceSize offsets = 0;
		auto vBuffer = m_VertexBuffer.getBuffer();
		vkCmdBindVertexBuffers(m_CmdBuffers[m_FrameIndex], 0, 1, &vBuffer, &offsets);
		vkCmdBindIndexBuffer(m_CmdBuffers[m_FrameIndex], m_IndexBuffer.getBuffer(), {}, VK_INDEX_TYPE_UINT32);
		vkCmdBindDescriptorSets(m_CmdBuffers[m_FrameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 0, 1,
		                        &m_DescriptorSets[m_FrameIndex], 0, nullptr);

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

		vkCmdDrawIndexed(m_CmdBuffers[m_FrameIndex], static_cast<uint32_t>(m_Indices.size()), 1, 0, 0, 0);

		vkCmdEndRendering(m_CmdBuffers[m_FrameIndex]);

		VulkanCommands::transitionImageLayout(m_CmdBuffers[m_FrameIndex], m_Swapchain.getImage(imageIndex), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		                      VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_2_NONE,
		                      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, 1);

		vkCheck(vkEndCommandBuffer(m_CmdBuffers[m_FrameIndex]), "vkEndCommandBuffer");

		std::vector<VkPipelineStageFlags> waitDstStageMask = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

		m_Device.getQueue().submitCommands(m_CmdBuffers[m_FrameIndex], m_DrawFences[m_FrameIndex],
		                                   m_PresentCompleteSemaphores[m_FrameIndex], m_Swapchain.getSemaphore(imageIndex),
		                                   waitDstStageMask);
		auto result = m_Device.getQueue().present(m_Swapchain.getSemaphore(imageIndex), m_Swapchain.getSwapchain(), imageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
			m_Swapchain.recreateSwapchain();

		m_FrameIndex = (m_FrameIndex + 1) % k_MaxFramesInFlight;
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
		vkCheck(vkCreateDescriptorSetLayout(m_Device.getDevice(), &setLayoutCI, nullptr, &m_DescriptorSetLayout), "vkCreateDescriptorSetLayout");
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
			.rasterizationSamples = m_Device.getMSAA(),
			.sampleShadingEnable = VK_TRUE,
			.minSampleShading = 0.2f,
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
		vkCheck(vkCreatePipelineLayout(m_Device.getDevice(), &layoutCI, nullptr, &m_PipelineLayout), "vkCreatePipelineLayout");

		auto format = m_Swapchain.getFormat().format;
		VkPipelineRenderingCreateInfo renderingCI {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.pNext = nullptr,
			.viewMask = 0,
			.colorAttachmentCount = 1,
			.pColorAttachmentFormats = &format,
			.depthAttachmentFormat = m_Swapchain.getDepthFormat(),
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
		vkCheck(vkCreateGraphicsPipelines(m_Device.getDevice(), VK_NULL_HANDLE, 1, &graphicsPipelineCI, nullptr, &m_GraphicsPipeline), "vkCreateGraphicsPipelines");

		CBK_DEBUG("Vulkan Graphics Pipeline created");

		vkDestroyShaderModule(m_Device.getDevice(), shaderModule, nullptr);
	}

	[[nodiscard]] VkShaderModule VulkanEngine::createShaderModule(const std::vector<char>& code) const {
		VkShaderModuleCreateInfo moduleCI{ .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			                               .pNext = nullptr,
			                               .flags = 0,
			                               .codeSize = code.size() * sizeof(char),
			                               .pCode = reinterpret_cast<const uint32_t*>(code.data()) };

		VkShaderModule module = VK_NULL_HANDLE;
		vkCheck(vkCreateShaderModule(m_Device.getDevice(), &moduleCI, nullptr, &module), "vkCreateShaderModule");
		return module;
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
					
					auto [it, inserted] = m_UniqueVertices.insert({vertex, static_cast<uint32_t>(m_Vertices.size())});
					if (inserted)
						m_Vertices.push_back(vertex);
					m_Indices.emplace_back(it->second);
				}
			}
	}

	void VulkanEngine::createVertexBuffer() {
		VkDeviceSize bufferSize = sizeof(Vertex) * m_Vertices.size();

		VulkanBuffer stagingBuffer(m_Device, bufferSize, VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT,
		                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		stagingBuffer.setData(m_Vertices.data());

		m_VertexBuffer = VulkanBuffer(m_Device, bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT,
		                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		auto queue = m_Device.getQueue();
		auto cmdBuffer = queue.beginSingleTimeCommands();
		VulkanCommands::copyBuffer(cmdBuffer, stagingBuffer.getBuffer(), m_VertexBuffer.getBuffer(), bufferSize);
		queue.endSingleTimeCommands(cmdBuffer);

		CBK_DEBUG("Vertex Buffer created");
	}

	void VulkanEngine::createIndexBuffer() {
		VkDeviceSize bufferSize = sizeof(m_Indices[0]) * m_Indices.size();
		VulkanBuffer stagingBuffer(m_Device, bufferSize, VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT,
		                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		stagingBuffer.setData(m_Indices.data());

		m_VertexBuffer = VulkanBuffer(m_Device, bufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT,
		                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		auto queue = m_Device.getQueue();
		auto cmdBuffer = queue.beginSingleTimeCommands();
		VulkanCommands::copyBuffer(cmdBuffer, stagingBuffer.getBuffer(), m_IndexBuffer.getBuffer(), bufferSize);
		queue.endSingleTimeCommands(cmdBuffer);

		CBK_DEBUG("Index Buffer created");
	}

	void VulkanEngine::createUniformBuffers() {
		for (size_t i = 0; i < k_MaxFramesInFlight; i++) {
			VkDeviceSize bufferSize = sizeof(UniformBufferObject);
			m_UniformBuffers.emplace_back(m_Device, bufferSize, VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT,
			                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			void* data = nullptr;
			m_UniformBuffers[i].map(data);
			m_UniformBuffersMapped.push_back(data);
		}

		CBK_DEBUG("Uniform Buffers created");
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
		vkCheck(vkCreateDescriptorPool(m_Device.getDevice(), &descPoolCI, nullptr, &m_DescriptorPool), "vkCreateDescriptorPool");
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
		vkCheck(vkAllocateDescriptorSets(m_Device.getDevice(), &descriptorSetAI, m_DescriptorSets.data()), "vkAllocateDescriptorSets");

		for (size_t i = 0; i < k_MaxFramesInFlight; i++) {
			VkDescriptorBufferInfo bufferInfo{ .buffer = m_UniformBuffers[i].getBuffer(), .offset = 0, .range = sizeof(UniformBufferObject) };
			VkDescriptorImageInfo imageInfo{ .sampler = m_Texture.getSampler(),
				                             .imageView = m_Texture.getView(),
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
			vkUpdateDescriptorSets(m_Device.getDevice(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
		}
	}

	void VulkanEngine::createCommandBuffers() {
		m_CmdBuffers.resize(k_MaxFramesInFlight);
		
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

		for (uint32_t i = 0; i < k_MaxFramesInFlight; i++) {
			vkCheck(vkCreateSemaphore(m_Device.getDevice(), &semaphoreCI, nullptr, &m_PresentCompleteSemaphores[i]), "vkCreateSemaphore");
			vkCheck(vkCreateFence(m_Device.getDevice(), &fenceCI, nullptr, &m_DrawFences[i]), "vkCreateFence");
		}
	}

	void VulkanEngine::updateUniformBuffer(uint32_t currentImage) {
		static auto startTime = std::chrono::high_resolution_clock::now();
		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

		UniformBufferObject ubo{
			.Model = math::rotateZ(/*10.0f * time*/ 0.f),
			.View = math::lookAt({ 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }),
			.Proj = math::perspective(
			    45.0f, static_cast<float>(m_Swapchain.getExtent().width) / static_cast<float>(m_Swapchain.getExtent().height), 0.1f, 10.0f)
		};

		ubo.Proj[1][1] *= -1;

		memcpy(m_UniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
	}
} // namespace lab::vk
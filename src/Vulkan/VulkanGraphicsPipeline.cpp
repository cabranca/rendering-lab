#include "VulkanGraphicsPipeline.h"

#include "Logger.h"
#include "Utils.h"

namespace lab::vk {

	VulkanGraphicsPipeline::VulkanGraphicsPipeline(VulkanDevice* device, const VulkanSwapchain& swapchain, uint32_t maxFramesInFlight)
	    : m_Device(device->getDevice()), m_Texture(k_TexturePath, *device) {
		createUniformBuffers(device, maxFramesInFlight);
		createDescriptorSetLayout();
		createGraphicsPipeline(device->getMSAA(), swapchain.getFormat().format, swapchain.getDepthFormat());
		createDescriptorPool(maxFramesInFlight);
		createDescriptorSets(maxFramesInFlight);
	}

	VulkanGraphicsPipeline::~VulkanGraphicsPipeline() {
		if (m_DescriptorPool != VK_NULL_HANDLE) {
			vkDestroyDescriptorPool(m_Device, m_DescriptorPool, nullptr);
			m_DescriptorPool = VK_NULL_HANDLE;
			CBK_DEBUG("Desriptor Pool destroyed");
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

        for (auto& buffer : m_UniformBuffers) {
			buffer.unmap();
		}
	}

    void VulkanGraphicsPipeline::updateUniformBuffer(uint32_t currentImage, VkExtent2D extent) {
		static auto startTime = std::chrono::high_resolution_clock::now();
		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

		UniformBufferObject ubo{
			.Model = math::rotateZ(/*10.0f * time*/ 0.f),
			.View = math::lookAt({ 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }),
			.Proj = math::perspective(
			    45.0f, static_cast<float>(extent.width) / static_cast<float>(extent.height), 0.1f, 10.0f)
		};

		ubo.Proj[1][1] *= -1;

		memcpy(m_UniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
	}

	void VulkanGraphicsPipeline::bind(VkCommandBuffer cb, uint32_t frameIndex) {
		vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipeline);
		vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 0, 1, &m_DescriptorSets[frameIndex], 0, nullptr);
	}

    void VulkanGraphicsPipeline::createUniformBuffers(VulkanDevice* device, uint32_t maxFramesInFlight) {
		for (size_t i = 0; i < maxFramesInFlight; i++) {
			VkDeviceSize bufferSize = sizeof(UniformBufferObject);
			m_UniformBuffers.emplace_back(*device, bufferSize, VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT,
			                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			m_UniformBuffersMapped.push_back(m_UniformBuffers[i].map());
		}

		CBK_DEBUG("Uniform Buffers created");
	}

	void VulkanGraphicsPipeline::createDescriptorSetLayout() {
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
		VkDescriptorSetLayoutCreateInfo setLayoutCI{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			                                         .pNext = nullptr,
			                                         .flags = 0,
			                                         .bindingCount = static_cast<uint32_t>(bindings.size()),
			                                         .pBindings = bindings.data() };
		vkCheck(vkCreateDescriptorSetLayout(m_Device, &setLayoutCI, nullptr, &m_DescriptorSetLayout), "vkCreateDescriptorSetLayout");
		CBK_DEBUG("MVP Uniform Buffer layout created");
	}

	void VulkanGraphicsPipeline::createGraphicsPipeline(VkSampleCountFlagBits sampleCount, VkFormat colorFormat, VkFormat depthFormat) {
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

		VkPipelineViewportStateCreateInfo viewportStateCI{ .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			                                               .pNext = nullptr,
			                                               .flags = 0,
			                                               .viewportCount = 1,
			                                               .pViewports = nullptr,
			                                               .scissorCount = 1,
			                                               .pScissors = nullptr };

		VkPipelineRasterizationStateCreateInfo rasterizerCI{ .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
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
			                                                 .lineWidth = 1.0f };

		VkPipelineMultisampleStateCreateInfo msaaCI{ .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			                                         .pNext = nullptr,
			                                         .flags = 0,
			                                         .rasterizationSamples = sampleCount,
			                                         .sampleShadingEnable = VK_TRUE,
			                                         .minSampleShading = 0.2f,
			                                         .pSampleMask = nullptr,
			                                         .alphaToCoverageEnable = VK_FALSE,
			                                         .alphaToOneEnable = VK_FALSE };

		VkPipelineColorBlendAttachmentState colorBlendAttachment{ .blendEnable = VK_FALSE,
			                                                      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
			                                                                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT };

		VkPipelineColorBlendStateCreateInfo blendStateCI{ .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			                                              .pNext = nullptr,
			                                              .flags = 0,
			                                              .logicOpEnable = VK_FALSE,
			                                              .logicOp = VK_LOGIC_OP_AND,
			                                              .attachmentCount = 1,
			                                              .pAttachments = &colorBlendAttachment,
			                                              .blendConstants = { 0.0f, 0.0f, 0.0f, 1.0f } };

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

		VkPipelineLayoutCreateInfo layoutCI{ .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			                                 .pNext = nullptr,
			                                 .flags = 0,
			                                 .setLayoutCount = 1,
			                                 .pSetLayouts = &m_DescriptorSetLayout,
			                                 .pushConstantRangeCount = 0,
			                                 .pPushConstantRanges = nullptr };
		vkCheck(vkCreatePipelineLayout(m_Device, &layoutCI, nullptr, &m_PipelineLayout), "vkCreatePipelineLayout");

		auto format = colorFormat;
		VkPipelineRenderingCreateInfo renderingCI{ .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			                                       .pNext = nullptr,
			                                       .viewMask = 0,
			                                       .colorAttachmentCount = 1,
			                                       .pColorAttachmentFormats = &format,
			                                       .depthAttachmentFormat = depthFormat,
			                                       .stencilAttachmentFormat = VK_FORMAT_UNDEFINED };
		VkGraphicsPipelineCreateInfo graphicsPipelineCI{ .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
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
			                                             .basePipelineIndex = 0 };
		vkCheck(vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &graphicsPipelineCI, nullptr, &m_GraphicsPipeline),
		        "vkCreateGraphicsPipelines");

		CBK_DEBUG("Vulkan Graphics Pipeline created");

		vkDestroyShaderModule(m_Device, shaderModule, nullptr);
	}

	VkShaderModule VulkanGraphicsPipeline::createShaderModule(const std::vector<char>& code) const {
		VkShaderModuleCreateInfo moduleCI{ .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			                               .pNext = nullptr,
			                               .flags = 0,
			                               .codeSize = code.size() * sizeof(char),
			                               .pCode = reinterpret_cast<const uint32_t*>(code.data()) };

		VkShaderModule module = VK_NULL_HANDLE;
		vkCheck(vkCreateShaderModule(m_Device, &moduleCI, nullptr, &module), "vkCreateShaderModule");
		return module;
	}

    void VulkanGraphicsPipeline::createDescriptorPool(uint32_t maxFramesInFlight) {
		std::array<VkDescriptorPoolSize, 2> poolSize{
			{ { .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = maxFramesInFlight },
			  { .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = maxFramesInFlight } }
		};
		VkDescriptorPoolCreateInfo descPoolCI{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.maxSets = maxFramesInFlight,
			.poolSizeCount = static_cast<uint32_t>(poolSize.size()),
			.pPoolSizes = poolSize.data(),
		};
		vkCheck(vkCreateDescriptorPool(m_Device, &descPoolCI, nullptr, &m_DescriptorPool), "vkCreateDescriptorPool");
	}

	void VulkanGraphicsPipeline::createDescriptorSets(uint32_t maxFramesInFlight) {
		m_DescriptorSets.resize(maxFramesInFlight);
		std::vector<VkDescriptorSetLayout> layouts(maxFramesInFlight, m_DescriptorSetLayout);
		VkDescriptorSetAllocateInfo descriptorSetAI{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.pNext = nullptr,
			.descriptorPool = m_DescriptorPool,
			.descriptorSetCount = maxFramesInFlight,
			.pSetLayouts = layouts.data()
		};
		vkCheck(vkAllocateDescriptorSets(m_Device, &descriptorSetAI, m_DescriptorSets.data()), "vkAllocateDescriptorSets");

		for (size_t i = 0; i < maxFramesInFlight; i++) {
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
			vkUpdateDescriptorSets(m_Device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
		}
	}
} // namespace lab::vk
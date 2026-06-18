#include "GraphicsPipeline.h"
#include "Check.h"
#include <volk/volk.h>

namespace lab {

	void GraphicsPipeline::init(VkDevice device, VkRenderPass renderPass, VkShaderModule vs, VkShaderModule fs, const SimpleMesh* mesh,
	                            uint32_t numImages) {
		m_Device = device;

		if (mesh)
			createDescriptorSets(numImages, mesh);

		VkPipelineShaderStageCreateInfo shaderStageCI[2] = { { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			                                                   .flags = 0,
			                                                   .stage = VK_SHADER_STAGE_VERTEX_BIT,
			                                                   .module = vs,
			                                                   .pName = "main" },
			                                                 { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			                                                   .flags = 0,
			                                                   .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			                                                   .module = fs,
			                                                   .pName = "main" } };

		VkPipelineVertexInputStateCreateInfo vertexInputInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

		VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyCI{ .sType =
			                                                                VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			                                                            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			                                                            .primitiveRestartEnable = VK_FALSE };

		// Viewport and scissor are dynamic: set per command buffer via vkCmdSetViewport/Scissor.
		// This keeps the pipeline valid across window resizes (no recreation needed).
		VkPipelineViewportStateCreateInfo vpCreateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			                                            .viewportCount = 1,
			                                            .scissorCount = 1 };

		VkDynamicState dynamicStates[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicCreateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			                                                .dynamicStateCount = 2,
			                                                .pDynamicStates = dynamicStates };

		VkPipelineRasterizationStateCreateInfo rasterizationCI{ .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			                                                    .polygonMode = VK_POLYGON_MODE_FILL,
			                                                    .cullMode = VK_CULL_MODE_NONE,
			                                                    .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
			                                                    .lineWidth = 1.f };

		VkPipelineMultisampleStateCreateInfo msCI{ .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			                                       .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
			                                       .sampleShadingEnable = VK_FALSE,
			                                       .minSampleShading = 1.f };

		VkPipelineColorBlendAttachmentState blendAttachState{ .blendEnable = VK_FALSE,
			                                                  .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
			                                                                    VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT };

		VkPipelineColorBlendStateCreateInfo blendCI{ .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			                                         .logicOpEnable = VK_FALSE,
			                                         .logicOp = VK_LOGIC_OP_COPY,
			                                         .attachmentCount = 1,
			                                         .pAttachments = &blendAttachState };

		VkPipelineLayoutCreateInfo layoutCI{ .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };

		// Can I have a mesh without VB?
		if (mesh) {
			layoutCI.setLayoutCount = 1;
			layoutCI.pSetLayouts = &m_DescriptorSetLayout;
		} else {
			layoutCI.setLayoutCount = 0;
			layoutCI.pSetLayouts = nullptr;
		}

		chk(vkCreatePipelineLayout(m_Device, &layoutCI, nullptr, &m_PipelineLayout));

		VkGraphicsPipelineCreateInfo pipelineCI{ .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			                                     .flags = 0,
			                                     .stageCount = 2,
			                                     .pStages = shaderStageCI,
			                                     .pVertexInputState = &vertexInputInfo,
			                                     .pInputAssemblyState = &pipelineInputAssemblyCI,
			                                     .pViewportState = &vpCreateInfo,
			                                     .pRasterizationState = &rasterizationCI,
			                                     .pMultisampleState = &msCI,
			                                     .pColorBlendState = &blendCI,
			                                     .pDynamicState = &dynamicCreateInfo,
			                                     .layout = m_PipelineLayout,
			                                     .renderPass = renderPass,
			                                     .subpass = 0 };

		chk(vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &m_Pipeline));
	}

	void GraphicsPipeline::shutdown() {
		vkDestroyPipelineLayout(m_Device, m_PipelineLayout, NULL);
		vkDestroyPipeline(m_Device, m_Pipeline, NULL);
	}

	void GraphicsPipeline::bind(VkCommandBuffer cmdBuffer, int imageIndex) {
		vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline);

		if (m_DescriptorSets.size() > 0) {
			vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 0, 1, &m_DescriptorSets.at(imageIndex), 0,
			                        nullptr);
		}
	}

	void GraphicsPipeline::createDescriptorSets(uint32_t numImages, const SimpleMesh* mesh) {
		createDescriptorPool(numImages);
		createDescriptorSetLayout();
		allocateDescriptorSets(numImages);
		updateDescriptorSets(numImages, mesh);
	}

	void GraphicsPipeline::createDescriptorPool(uint32_t numImages) {
		VkDescriptorPoolCreateInfo poolCI{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .flags = 0, .maxSets = numImages, .pPoolSizes = nullptr
		};

		chk(vkCreateDescriptorPool(m_Device, &poolCI, nullptr, &m_DescriptorPool));
	}

	void GraphicsPipeline::createDescriptorSetLayout() {
		std::vector<VkDescriptorSetLayoutBinding> layoutBindings{ { .binding = 0,
			                                                        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			                                                        .descriptorCount = 1,
			                                                        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT } };

		VkDescriptorSetLayoutCreateInfo descSetLayoutCI{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			                                             .pNext = nullptr,
			                                             .flags = 0,
			                                             .bindingCount = (uint32_t)layoutBindings.size(),
			                                             .pBindings = layoutBindings.data() };

		chk(vkCreateDescriptorSetLayout(m_Device, &descSetLayoutCI, nullptr, &m_DescriptorSetLayout));
	}

	void GraphicsPipeline::allocateDescriptorSets(uint32_t numImages) {
		std::vector<VkDescriptorSetLayout> layouts(numImages, m_DescriptorSetLayout);

		VkDescriptorSetAllocateInfo allocInfo{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			                                   .pNext = nullptr,
			                                   .descriptorPool = m_DescriptorPool,
			                                   .descriptorSetCount = numImages,
			                                   .pSetLayouts = layouts.data() };

		m_DescriptorSets.resize(numImages);

		chk(vkAllocateDescriptorSets(m_Device, &allocInfo, m_DescriptorSets.data()));
	}

	void GraphicsPipeline::updateDescriptorSets(uint32_t numImages, const SimpleMesh* mesh) {
		VkDescriptorBufferInfo descBI{ .buffer = mesh->VB.Buffer, .offset = 0, .range = mesh->VertexBufferSize };

		std::vector<VkWriteDescriptorSet> writeDescriptorSet;

		for (uint32_t i = 0; i < numImages; i++) {
			writeDescriptorSet.push_back(VkWriteDescriptorSet{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			                                                   .dstSet = m_DescriptorSets.at(i),
			                                                   .dstBinding = 0,
			                                                   .dstArrayElement = 0,
			                                                   .descriptorCount = 1,
			                                                   .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			                                                   .pBufferInfo = &descBI });
		}

		vkUpdateDescriptorSets(m_Device, writeDescriptorSet.size(), writeDescriptorSet.data(), 0, nullptr);
	}
} // namespace lab

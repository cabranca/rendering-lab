/* Copyright (c) 2025-2026, Sascha Willems
 * SPDX-License-Identifier: MIT
 *
 * BindlessPhong: three Suzanne heads, each sampling its own texture through a
 * descriptor-indexed (bindless) array, lit with simple Phong shading. Serves as
 * the reference demo for the rendering_core library — copy it as a starting
 * point for new experiments.
 */

#include <array>
#include <cstring>
#include <span>
#include <string>
#include <vector>

#include <volk/volk.h>
#include <SDL3/SDL.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "Buffer.h"
#include "Check.h"
#include "Common.h"
#include "ShaderCompiler.h"
#include "Texture.h"
#include "VertexBuffer.h"
#include "VulkanCore.h"

using namespace lab;

// Matches the ShaderData layout in assets/shader.slang. The vertex shader reads
// it through a buffer device address pushed as a push constant.
struct ShaderData {
	glm::mat4 projection;
	glm::mat4 view;
	glm::mat4 model[3];
	glm::vec4 lightPos{ 0.0f, -10.0f, 10.0f, 0.0f };
	uint32_t selected{ 1 };
};

// ---------------------------------------------------------------------------
// Resource creation
// ---------------------------------------------------------------------------

// Loads one texture per instance (assets/suzanne<i>.ktx) and returns their
// descriptors for the bindless array.
static std::vector<VkDescriptorImageInfo> loadInstanceTextures(const VulkanContext& ctx, std::span<Texture> textures) {
	std::vector<VkDescriptorImageInfo> descriptors{};
	for (size_t i = 0; i < textures.size(); i++) {
		textures[i].init(ctx, "assets/suzanne" + std::to_string(i) + ".ktx");
		descriptors.push_back(textures[i].getDescriptorInfo());
	}
	return descriptors;
}

// A single binding holding a variable-count combined-image-sampler array, the
// core of the bindless texturing approach.
static VkDescriptorSetLayout createBindlessTextureLayout(VkDevice device, uint32_t textureCount) {
	VkDescriptorBindingFlags variableFlag{ VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT };
	VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
		                                                     .bindingCount = 1,
		                                                     .pBindingFlags = &variableFlag };
	VkDescriptorSetLayoutBinding binding{ .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		                                  .descriptorCount = textureCount,
		                                  .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT };
	VkDescriptorSetLayoutCreateInfo layoutCI{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .pNext = &bindingFlags, .bindingCount = 1, .pBindings = &binding
	};
	VkDescriptorSetLayout layout{ VK_NULL_HANDLE };
	chk(vkCreateDescriptorSetLayout(device, &layoutCI, nullptr, &layout));
	return layout;
}

static VkDescriptorPool createTextureDescriptorPool(VkDevice device, uint32_t textureCount) {
	VkDescriptorPoolSize poolSize{ .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = textureCount };
	VkDescriptorPoolCreateInfo poolCI{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .maxSets = 1, .poolSizeCount = 1, .pPoolSizes = &poolSize
	};
	VkDescriptorPool pool{ VK_NULL_HANDLE };
	chk(vkCreateDescriptorPool(device, &poolCI, nullptr, &pool));
	return pool;
}

// Allocates the variable-count set and writes the texture descriptors into it.
static VkDescriptorSet allocateTextureDescriptorSet(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout layout,
                                                    const std::vector<VkDescriptorImageInfo>& descriptors) {
	uint32_t descriptorCount{ static_cast<uint32_t>(descriptors.size()) };
	VkDescriptorSetVariableDescriptorCountAllocateInfo variableCountAI{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT,
		.descriptorSetCount = 1,
		.pDescriptorCounts = &descriptorCount
	};
	VkDescriptorSetAllocateInfo allocInfo{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		                                   .pNext = &variableCountAI,
		                                   .descriptorPool = pool,
		                                   .descriptorSetCount = 1,
		                                   .pSetLayouts = &layout };
	VkDescriptorSet set{ VK_NULL_HANDLE };
	chk(vkAllocateDescriptorSets(device, &allocInfo, &set));

	VkWriteDescriptorSet write{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		                        .dstSet = set,
		                        .dstBinding = 0,
		                        .descriptorCount = descriptorCount,
		                        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		                        .pImageInfo = descriptors.data() };
	vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
	return set;
}

// Push constant carries the buffer device address of the per-frame ShaderData.
static VkPipelineLayout createPipelineLayout(VkDevice device, VkDescriptorSetLayout setLayout) {
	VkPushConstantRange pushConstantRange{ .stageFlags = VK_SHADER_STAGE_VERTEX_BIT, .size = sizeof(VkDeviceAddress) };
	VkPipelineLayoutCreateInfo layoutCI{ .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		                                 .setLayoutCount = 1,
		                                 .pSetLayouts = &setLayout,
		                                 .pushConstantRangeCount = 1,
		                                 .pPushConstantRanges = &pushConstantRange };
	VkPipelineLayout layout{ VK_NULL_HANDLE };
	chk(vkCreatePipelineLayout(device, &layoutCI, nullptr, &layout));
	return layout;
}

static VkPipeline createGraphicsPipeline(VkDevice device, VkPipelineLayout layout, VkShaderModule shaderModule, VkFormat colorFormat,
                                         VkFormat depthFormat) {
	std::vector<VkPipelineShaderStageCreateInfo> shaderStages{ { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		                                                         .stage = VK_SHADER_STAGE_VERTEX_BIT,
		                                                         .module = shaderModule,
		                                                         .pName = "main" },
		                                                       { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		                                                         .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
		                                                         .module = shaderModule,
		                                                         .pName = "main" } };
	VkVertexInputBindingDescription vertexBinding{ .binding = 0, .stride = sizeof(Vertex), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX };
	std::vector<VkVertexInputAttributeDescription> vertexAttributes{
		{ .location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, pos) },
		{ .location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, normal) },
		{ .location = 2, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(Vertex, uv) },
	};
	VkPipelineVertexInputStateCreateInfo vertexInputState{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &vertexBinding,
		.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributes.size()),
		.pVertexAttributeDescriptions = vertexAttributes.data(),
	};
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{ .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		                                                       .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST };
	std::vector<VkDynamicState> dynamicStates{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamicState{ .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		                                           .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
		                                           .pDynamicStates = dynamicStates.data() };
	VkPipelineViewportStateCreateInfo viewportState{ .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		                                             .viewportCount = 1,
		                                             .scissorCount = 1 };
	VkPipelineRasterizationStateCreateInfo rasterizationState{ .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		                                                       .lineWidth = 1.0f };
	VkPipelineMultisampleStateCreateInfo multisampleState{ .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		                                                   .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT };
	VkPipelineDepthStencilStateCreateInfo depthStencilState{ .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		                                                     .depthTestEnable = VK_TRUE,
		                                                     .depthWriteEnable = VK_TRUE,
		                                                     .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL };
	VkPipelineColorBlendAttachmentState blendAttachment{ .colorWriteMask = 0xF };
	VkPipelineColorBlendStateCreateInfo colorBlendState{ .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		                                                 .attachmentCount = 1,
		                                                 .pAttachments = &blendAttachment };
	VkPipelineRenderingCreateInfo renderingCI{ .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		                                       .colorAttachmentCount = 1,
		                                       .pColorAttachmentFormats = &colorFormat,
		                                       .depthAttachmentFormat = depthFormat };
	VkGraphicsPipelineCreateInfo pipelineCI{ .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		                                     .pNext = &renderingCI,
		                                     .stageCount = static_cast<uint32_t>(shaderStages.size()),
		                                     .pStages = shaderStages.data(),
		                                     .pVertexInputState = &vertexInputState,
		                                     .pInputAssemblyState = &inputAssemblyState,
		                                     .pViewportState = &viewportState,
		                                     .pRasterizationState = &rasterizationState,
		                                     .pMultisampleState = &multisampleState,
		                                     .pDepthStencilState = &depthStencilState,
		                                     .pColorBlendState = &colorBlendState,
		                                     .pDynamicState = &dynamicState,
		                                     .layout = layout };
	VkPipeline pipeline{ VK_NULL_HANDLE };
	chk(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &pipeline));
	return pipeline;
}

// ---------------------------------------------------------------------------
// Per-frame
// ---------------------------------------------------------------------------

// Handles window/input events for one frame; returns true when the user quits.
static bool processEvents(VulkanCore& core, ShaderData& shaderData, glm::vec3& camPos, glm::vec3 (&objectRotations)[3], float elapsedTime) {
	for (SDL_Event event; SDL_PollEvent(&event);) {
		if (event.type == SDL_EVENT_QUIT) {
			return true;
		}
		if (event.type == SDL_EVENT_MOUSE_MOTION && (event.motion.state & SDL_BUTTON_LMASK)) {
			objectRotations[shaderData.selected].x -= event.motion.yrel * elapsedTime;
			objectRotations[shaderData.selected].y += event.motion.xrel * elapsedTime;
		}
		if (event.type == SDL_EVENT_MOUSE_WHEEL) {
			camPos.z += event.wheel.y * elapsedTime * 10.0f;
		}
		if (event.type == SDL_EVENT_KEY_DOWN) {
			if (event.key.key == SDLK_PLUS || event.key.key == SDLK_KP_PLUS) {
				shaderData.selected = (shaderData.selected < 2) ? shaderData.selected + 1 : 0;
			}
			if (event.key.key == SDLK_MINUS || event.key.key == SDLK_KP_MINUS) {
				shaderData.selected = (shaderData.selected > 0) ? shaderData.selected - 1 : 2;
			}
		}
		if (event.type == SDL_EVENT_WINDOW_RESIZED) {
			core.requestResize();
		}
	}
	return false;
}

// Recomputes the camera and the per-instance model matrices into shaderData.
static void updateShaderData(ShaderData& shaderData, const glm::vec3& camPos, const glm::vec3 (&objectRotations)[3], VkExtent2D extent) {
	shaderData.projection =
	    glm::perspective(glm::radians(45.0f), static_cast<float>(extent.width) / static_cast<float>(extent.height), 0.1f, 32.0f);
	shaderData.view = glm::translate(glm::mat4(1.0f), camPos);
	for (int i = 0; i < 3; i++) {
		auto instancePos = glm::vec3(static_cast<float>(i - 1) * 3.0f, 0.0f, 0.0f);
		shaderData.model[i] = glm::translate(glm::mat4(1.0f), instancePos) * glm::mat4_cast(glm::quat(objectRotations[i]));
	}
}

// Records the bindings and the instanced draw for the three heads.
static void recordDraw(VkCommandBuffer cb, VkPipeline pipeline, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                       const VertexBuffer& mesh, VkDeviceAddress shaderDataAddress) {
	vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
	vkCmdPushConstants(cb, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(VkDeviceAddress), &shaderDataAddress);
	VkBuffer vertexBuffer = mesh.getBuffer();
	VkDeviceSize vertexOffset{ 0 };
	vkCmdBindVertexBuffers(cb, 0, 1, &vertexBuffer, &vertexOffset);
	vkCmdBindIndexBuffer(cb, mesh.getBuffer(), mesh.getIndexOffset(), VK_INDEX_TYPE_UINT16);
	vkCmdDrawIndexed(cb, mesh.getIndexCount(), 3, 0, 0, 0);
}

int main() {
	VulkanCore core;
	core.init("Bindless Phong");

	const VulkanContext& ctx = core.getContext();
	VkDevice device = core.getDevice();

	VertexBuffer mesh = core.loadModel("assets/suzanne.obj");

	// Textures + bindless descriptor set (one texture per instance).
	constexpr uint32_t textureCount{ 3 };
	std::array<Texture, textureCount> textures{};
	std::vector<VkDescriptorImageInfo> textureDescriptors = loadInstanceTextures(ctx, textures);
	VkDescriptorSetLayout descriptorSetLayout = createBindlessTextureLayout(device, textureCount);
	VkDescriptorPool descriptorPool = createTextureDescriptorPool(device, textureCount);
	VkDescriptorSet descriptorSet = allocateTextureDescriptorSet(device, descriptorPool, descriptorSetLayout, textureDescriptors);

	// Shader + graphics pipeline.
	VkShaderModule shaderModule = loadSlangShader(ctx, "assets/shader.slang");
	VkPipelineLayout pipelineLayout = createPipelineLayout(device, descriptorSetLayout);
	VkPipeline pipeline = createGraphicsPipeline(device, pipelineLayout, shaderModule, core.getColorFormat(), core.getDepthFormat());

	// Per-frame shader data buffers (host visible, addressed via BDA).
	std::array<Buffer, VulkanCore::maxFramesInFlight> shaderDataBuffers{};
	for (auto& buffer: shaderDataBuffers) {
		buffer.init(ctx, sizeof(ShaderData), VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, true);
	}

	// Scene state
	ShaderData shaderData{};
	glm::vec3 camPos{ 0.0f, 0.0f, -6.0f };
	glm::vec3 objectRotations[3]{};

	// Render loop
	uint64_t lastTime{ SDL_GetTicks() };
	bool quit{ false };
	while (!quit) {
		float elapsedTime{ (SDL_GetTicks() - lastTime) / 1000.0f };
		lastTime = SDL_GetTicks();

		quit = processEvents(core, shaderData, camPos, objectRotations, elapsedTime);
		updateShaderData(shaderData, camPos, objectRotations, core.getExtent());

		VkCommandBuffer cb = core.beginFrame();
		if (cb == VK_NULL_HANDLE) {
			continue; // swapchain was rebuilt; skip this frame
		}
		uint32_t frame = core.getFrameIndex();
		memcpy(shaderDataBuffers[frame].getMappedData(), &shaderData, sizeof(ShaderData));

		// Single pass straight to the swapchain image.
		core.cmdTransitionToAttachment(cb, core.getCurrentImage(), core.getDepthImage());
		core.cmdBeginRendering(cb, core.getCurrentImageView(), core.getDepthImageView(), core.getExtent());
		core.cmdSetFullViewportScissor(cb, core.getExtent());
		recordDraw(cb, pipeline, pipelineLayout, descriptorSet, mesh, shaderDataBuffers[frame].getDeviceAddress());
		vkCmdEndRendering(cb);
		core.cmdTransitionToPresent(cb, core.getCurrentImage());

		core.endFrame();
	}

	// Tear down demo-owned resources before the core releases the device.
	chk(vkDeviceWaitIdle(device));
	for (auto& buffer: shaderDataBuffers) {
		buffer.shutdown(ctx);
	}
	vkDestroyPipeline(device, pipeline, nullptr);
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
	vkDestroyShaderModule(device, shaderModule, nullptr);
	vkDestroyDescriptorPool(device, descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
	for (auto& texture: textures) {
		texture.shutdown(ctx);
	}
	mesh.shutdown(ctx);

	core.shutdown();
	return 0;
}

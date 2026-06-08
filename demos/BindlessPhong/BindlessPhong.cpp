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

int main() {
    VulkanCore core;
    core.init("Bindless Phong");

    const VulkanContext& ctx = core.getContext();
    VkDevice device = core.getDevice();

    // Mesh
    VertexBuffer mesh = core.loadModel("assets/suzanne.obj");

    // Textures (one per instance) addressed by a descriptor-indexed array.
    constexpr uint32_t textureCount{ 3 };
    std::array<Texture, textureCount> textures{};
    std::vector<VkDescriptorImageInfo> textureDescriptors{};
    for (uint32_t i = 0; i < textureCount; i++) {
        textures[i].init(ctx, "assets/suzanne" + std::to_string(i) + ".ktx");
        textureDescriptors.push_back(textures[i].getDescriptorInfo());
    }

    // Descriptor set with a variable-count combined-image-sampler array (bindless).
    VkDescriptorBindingFlags descVariableFlag{ VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT };
    VkDescriptorSetLayoutBindingFlagsCreateInfo descBindingFlags{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO, .bindingCount = 1, .pBindingFlags = &descVariableFlag };
    VkDescriptorSetLayoutBinding descLayoutBindingTex{ .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = textureCount, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT };
    VkDescriptorSetLayoutCreateInfo descLayoutTexCI{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .pNext = &descBindingFlags, .bindingCount = 1, .pBindings = &descLayoutBindingTex };
    VkDescriptorSetLayout descriptorSetLayoutTex{ VK_NULL_HANDLE };
    chk(vkCreateDescriptorSetLayout(device, &descLayoutTexCI, nullptr, &descriptorSetLayoutTex));

    VkDescriptorPoolSize poolSize{ .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = textureCount };
    VkDescriptorPoolCreateInfo descPoolCI{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .maxSets = 1, .poolSizeCount = 1, .pPoolSizes = &poolSize };
    VkDescriptorPool descriptorPool{ VK_NULL_HANDLE };
    chk(vkCreateDescriptorPool(device, &descPoolCI, nullptr, &descriptorPool));

    uint32_t variableDescCount{ textureCount };
    VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescCountAI{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT, .descriptorSetCount = 1, .pDescriptorCounts = &variableDescCount };
    VkDescriptorSetAllocateInfo texDescSetAlloc{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, .pNext = &variableDescCountAI, .descriptorPool = descriptorPool, .descriptorSetCount = 1, .pSetLayouts = &descriptorSetLayoutTex };
    VkDescriptorSet descriptorSetTex{ VK_NULL_HANDLE };
    chk(vkAllocateDescriptorSets(device, &texDescSetAlloc, &descriptorSetTex));
    VkWriteDescriptorSet writeDescSet{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = descriptorSetTex, .dstBinding = 0, .descriptorCount = static_cast<uint32_t>(textureDescriptors.size()), .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = textureDescriptors.data() };
    vkUpdateDescriptorSets(device, 1, &writeDescSet, 0, nullptr);

    // Shader + graphics pipeline
    VkShaderModule shaderModule = loadSlangShader(ctx, "assets/shader.slang");

    VkPushConstantRange pushConstantRange{ .stageFlags = VK_SHADER_STAGE_VERTEX_BIT, .size = sizeof(VkDeviceAddress) };
    VkPipelineLayoutCreateInfo pipelineLayoutCI{ .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = 1, .pSetLayouts = &descriptorSetLayoutTex, .pushConstantRangeCount = 1, .pPushConstantRanges = &pushConstantRange };
    VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
    chk(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout));

    std::vector<VkPipelineShaderStageCreateInfo> shaderStages{
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = shaderModule, .pName = "main" },
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = shaderModule, .pName = "main" }
    };
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
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{ .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST };
    std::vector<VkDynamicState> dynamicStates{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState{ .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()), .pDynamicStates = dynamicStates.data() };
    VkPipelineViewportStateCreateInfo viewportState{ .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .scissorCount = 1 };
    VkPipelineRasterizationStateCreateInfo rasterizationState{ .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, .lineWidth = 1.0f };
    VkPipelineMultisampleStateCreateInfo multisampleState{ .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT };
    VkPipelineDepthStencilStateCreateInfo depthStencilState{ .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, .depthTestEnable = VK_TRUE, .depthWriteEnable = VK_TRUE, .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL };
    VkPipelineColorBlendAttachmentState blendAttachment{ .colorWriteMask = 0xF };
    VkPipelineColorBlendStateCreateInfo colorBlendState{ .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, .attachmentCount = 1, .pAttachments = &blendAttachment };
    VkFormat colorFormat = core.getColorFormat();
    VkFormat depthFormat = core.getDepthFormat();
    VkPipelineRenderingCreateInfo renderingCI{ .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO, .colorAttachmentCount = 1, .pColorAttachmentFormats = &colorFormat, .depthAttachmentFormat = depthFormat };
    VkGraphicsPipelineCreateInfo pipelineCI{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
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
        .layout = pipelineLayout
    };
    VkPipeline pipeline{ VK_NULL_HANDLE };
    chk(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &pipeline));

    // Per-frame shader data buffers (host visible, addressed via BDA).
    std::array<Buffer, VulkanCore::maxFramesInFlight> shaderDataBuffers{};
    for (auto& buffer : shaderDataBuffers) {
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

        for (SDL_Event event; SDL_PollEvent(&event);) {
            if (event.type == SDL_EVENT_QUIT) {
                quit = true;
                break;
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

        VkExtent2D extent = core.getExtent();
        shaderData.projection = glm::perspective(glm::radians(45.0f), static_cast<float>(extent.width) / static_cast<float>(extent.height), 0.1f, 32.0f);
        shaderData.view = glm::translate(glm::mat4(1.0f), camPos);
        for (int i = 0; i < 3; i++) {
            auto instancePos = glm::vec3(static_cast<float>(i - 1) * 3.0f, 0.0f, 0.0f);
            shaderData.model[i] = glm::translate(glm::mat4(1.0f), instancePos) * glm::mat4_cast(glm::quat(objectRotations[i]));
        }

        VkCommandBuffer cb = core.beginFrame();
        if (cb == VK_NULL_HANDLE) {
            continue; // swapchain was rebuilt; skip this frame
        }
        uint32_t frame = core.getFrameIndex();
        memcpy(shaderDataBuffers[frame].getMappedData(), &shaderData, sizeof(ShaderData));

        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSetTex, 0, nullptr);
        VkDeviceAddress shaderDataAddress = shaderDataBuffers[frame].getDeviceAddress();
        vkCmdPushConstants(cb, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(VkDeviceAddress), &shaderDataAddress);
        VkBuffer vertexBuffer = mesh.getBuffer();
        VkDeviceSize vertexOffset{ 0 };
        vkCmdBindVertexBuffers(cb, 0, 1, &vertexBuffer, &vertexOffset);
        vkCmdBindIndexBuffer(cb, mesh.getBuffer(), mesh.getIndexOffset(), VK_INDEX_TYPE_UINT16);
        vkCmdDrawIndexed(cb, mesh.getIndexCount(), 3, 0, 0, 0);

        core.endFrame();
    }

    // Tear down (device is idle after core.shutdown()'s wait, but destroy demo
    // resources before the core releases the device).
    chk(vkDeviceWaitIdle(device));
    for (auto& buffer : shaderDataBuffers) {
        buffer.shutdown(ctx);
    }
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyShaderModule(device, shaderModule, nullptr);
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayoutTex, nullptr);
    for (auto& texture : textures) {
        texture.shutdown(ctx);
    }
    mesh.shutdown(ctx);

    core.shutdown();
    return 0;
}

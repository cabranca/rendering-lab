#include "Texture.h"

#include <cstring>
#include <string>
#include <vector>

#include <volk/volk.h>
#include <ktx.h>
#include <ktxvulkan.h>

#include "Check.h"
#include "VulkanContext.h"

namespace lab {

    void Texture::init(const VulkanContext& ctx, std::string_view path) {
        auto device = ctx.getDevice();
        auto allocator = ctx.getAllocator();

        ktxTexture* ktx{ nullptr };
        std::string pathStr(path);
        chk(ktxTexture_CreateFromNamedFile(pathStr.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktx) == KTX_SUCCESS);
        const uint32_t mipLevels = ktx->numLevels;

        VkImageCreateInfo imgCI{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = ktxTexture_GetVkFormat(ktx),
            .extent = { .width = ktx->baseWidth, .height = ktx->baseHeight, .depth = 1 },
            .mipLevels = mipLevels,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };
        VmaAllocationCreateInfo imgAllocCI{ .usage = VMA_MEMORY_USAGE_AUTO };
        chk(vmaCreateImage(allocator, &imgCI, &imgAllocCI, &m_Image, &m_Allocation, nullptr));

        VkImageViewCreateInfo viewCI{ .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = m_Image, .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = imgCI.format, .subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = mipLevels, .layerCount = 1 } };
        chk(vkCreateImageView(device, &viewCI, nullptr, &m_View));

        // Staging buffer holding the whole KTX payload.
        VkBuffer srcBuffer{};
        VmaAllocation srcAllocation{};
        VkBufferCreateInfo srcBufferCI{ .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = static_cast<VkDeviceSize>(ktx->dataSize), .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT };
        VmaAllocationCreateInfo srcAllocCI{ .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT, .usage = VMA_MEMORY_USAGE_AUTO };
        VmaAllocationInfo srcAllocInfo{};
        chk(vmaCreateBuffer(allocator, &srcBufferCI, &srcAllocCI, &srcBuffer, &srcAllocation, &srcAllocInfo));
        memcpy(srcAllocInfo.pMappedData, ktx->pData, ktx->dataSize);

        std::vector<VkBufferImageCopy> copyRegions{};
        for (uint32_t level = 0; level < mipLevels; level++) {
            ktx_size_t mipOffset{ 0 };
            ktxTexture_GetImageOffset(ktx, level, 0, 0, &mipOffset);
            copyRegions.push_back({
                .bufferOffset = mipOffset,
                .imageSubresource{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = level, .layerCount = 1 },
                .imageExtent{ .width = ktx->baseWidth >> level, .height = ktx->baseHeight >> level, .depth = 1 },
            });
        }

        ctx.submitImmediate([&](VkCommandBuffer cb) {
            VkImageMemoryBarrier2 toTransfer{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
                .srcAccessMask = VK_ACCESS_2_NONE,
                .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .image = m_Image,
                .subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = mipLevels, .layerCount = 1 }
            };
            VkDependencyInfo dep{ .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &toTransfer };
            vkCmdPipelineBarrier2(cb, &dep);

            vkCmdCopyBufferToImage(cb, srcBuffer, m_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<uint32_t>(copyRegions.size()), copyRegions.data());

            VkImageMemoryBarrier2 toRead{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                .image = m_Image,
                .subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = mipLevels, .layerCount = 1 }
            };
            dep.pImageMemoryBarriers = &toRead;
            vkCmdPipelineBarrier2(cb, &dep);
        });

        vmaDestroyBuffer(allocator, srcBuffer, srcAllocation);

        VkSamplerCreateInfo samplerCI{
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .anisotropyEnable = VK_TRUE,
            .maxAnisotropy = 8.0f,
            .maxLod = static_cast<float>(mipLevels),
        };
        chk(vkCreateSampler(device, &samplerCI, nullptr, &m_Sampler));

        ktxTexture_Destroy(ktx);
    }

    void Texture::shutdown(const VulkanContext& ctx) {
        vkDestroySampler(ctx.getDevice(), m_Sampler, nullptr);
        vkDestroyImageView(ctx.getDevice(), m_View, nullptr);
        vmaDestroyImage(ctx.getAllocator(), m_Image, m_Allocation);
    }
}

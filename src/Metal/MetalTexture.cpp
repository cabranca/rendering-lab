#include "MetalTexture.h"

#include "stb_image.h"

#include <print>

#include <Foundation/NSTypes.hpp>
#include <Metal/MTLDevice.hpp>
#include <Metal/MTLPixelFormat.hpp>
#include <Metal/MTLTexture.hpp>
#include <Metal/MTLTypes.hpp>

namespace lab::mtl {

	void MetalTexture::init(MTL::Device* device, std::string_view path) {
        int width, height, channels;
        // Force 4 components (RGBA) so the source stride matches the RGBA8 MetalTexture.
        stbi_uc* data = stbi_load(path.data(), &width, &height, &channels, STBI_rgb_alpha);
        if (data == nullptr) {
            std::println("MetalTexture::init - Failed to load {}", path);
            return;
        }

        auto* texDesc = MTL::TextureDescriptor::texture2DDescriptor(MTL::PixelFormatRGBA8Unorm, width, height, false);
        m_Texture = device->newTexture(texDesc);
        m_Texture->replaceRegion(MTL::Region::Make2D(0, 0, width, height), 0, data, static_cast<NS::UInteger>(width) * 4);
        stbi_image_free(data);
        texDesc->release();
    }

	void MetalTexture::shutdown() {
        m_Texture->release();
    }

	MTL::Texture* MetalTexture::getMTLTexture() {
		return m_Texture;
	}
} // namespace lab::mtl
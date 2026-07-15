#pragma once

#include <string_view>
namespace MTL {
    class Device;
	class Texture;
}

namespace lab::mtl {

	class MetalTexture {
	  public:
		void init(MTL::Device* device, std::string_view path);
        void shutdown();

        [[nodiscard]] MTL::Texture* getMTLTexture();

	  private:
		MTL::Texture* m_Texture = nullptr;
	};
} // namespace lab::mtl
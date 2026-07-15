#pragma once

#include <Math/Vector2.h>

#include "MetalTexture.h"
#include "Window.h"

namespace MTL {
	class Device;
	class RenderPipelineState;
	class Buffer;
	class ResidencySet;
}

namespace MTL4 {
	class CommandQueue;
	class CommandBuffer;
	class CommandAllocator;
}

namespace CA {
	class MetalLayer;
}

namespace NS {
	class Error;
}

namespace lab::mtl {

	// Must match VertexIn in colored.metal: float2 position + float2 texCoords.
	struct VertexData {
		math::Vector2 Position;
		math::Vector2 TexCoords;
	};

	class MetalEngine {
	  public:
		void init(const Window& window);
		void run();
		void shutdown();

	  private:
		constexpr static int k_MaxFramesInFlight = 3;
		int m_FrameIndex = 0;

		Window m_Window; // NOT THE OWNER
		MTL::Device* m_Device = nullptr;
		MTL4::CommandQueue* m_Queue = nullptr;
		std::array<MTL4::CommandAllocator*, k_MaxFramesInFlight> m_Allocators{nullptr, nullptr, nullptr};
		MTL4::CommandBuffer* m_CmdBuffers = nullptr;
        void* m_MetalView = nullptr;
        CA::MetalLayer* m_Layer = nullptr;
		MTL::RenderPipelineState* m_PipelineState = nullptr;
		MTL::Buffer* m_VertexBuffer = nullptr;
		MTL::Buffer* m_IndexBuffer = nullptr;
		MTL::ResidencySet* m_ResidencySet = nullptr;
		MetalTexture m_Texture;

		void createRenderPipelineState();
		void createBuffers();
		static std::string getShaderSource(std::string_view path);
		static void checkError(const void* result, NS::Error* error, std::string_view context);
	};
} // namespace lab::mtl
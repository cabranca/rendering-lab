#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#include "MetalEngine.h"

#include <array>
#include <cstdint>
#include <fstream>
#include <print>
#include <sstream>

#include <Foundation/NSAutoreleasePool.hpp>
#include <Foundation/NSString.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/CAMetalLayer.hpp>
#include "SDL3/SDL_events.h"
#include "SDL3/SDL_metal.h"

namespace lab::mtl {

	using namespace MTL;
    using namespace CA;

	void MetalEngine::init(const Window& window) {
		m_Window = window;
		m_Device = CreateSystemDefaultDevice();
		std::println("Hello Metal! GPU: {}", m_Device->name()->utf8String());

		GPUFamily gpuFamily = GPUFamilyMetal4;
		if (!m_Device->supportsFamily(gpuFamily)) {
			std::println("MetalEngine::init - Device doesn't support Metal 4");
			m_Device->release();
            return;
		}

        m_MetalView = SDL_Metal_CreateView(window.getWindowHandle());
        m_Layer = reinterpret_cast<MetalLayer*>(SDL_Metal_GetLayer(m_MetalView));
        m_Layer->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
		m_Layer->setDevice(m_Device);
        

		m_Queue = m_Device->newMTL4CommandQueue();
		m_Queue->addResidencySet(m_Layer->residencySet());

		m_CmdBuffers = m_Device->newCommandBuffer();
		
		for (int i = 0; i < 3; i++) {
			m_Allocators[i] = m_Device->newCommandAllocator();
		}

		createRenderPipelineState();
		createBuffers();
		m_Texture.init(m_Device, "assets/textures/awesomeface.png");

		// The texture is sampled on the GPU, so it must be resident too.
		m_ResidencySet->addAllocation(m_Texture.getMTLTexture());
		m_ResidencySet->commit();
	}

	void MetalEngine::run() {
        NS::Error* error = nullptr;
        auto* tableDesc = MTL4::ArgumentTableDescriptor::alloc()->init();
        tableDesc->setMaxBufferBindCount(2);
        tableDesc->setMaxTextureBindCount(1);
        auto* table = m_Device->newArgumentTable(tableDesc, &error);
        checkError(table, error, "Argument table creation");
        tableDesc->release();

        // Binding index 0 must match [[buffer(0)]] in the vertex shader.
        table->setAddress(m_VertexBuffer->gpuAddress(), 0);
		table->setTexture(m_Texture.getMTLTexture()->gpuResourceID(), 0);
        
        auto* renderDescriptor = MTL4::RenderPassDescriptor::alloc()->init();
        auto* colorAttachment = renderDescriptor->colorAttachments()->object(0);
        colorAttachment->setLoadAction(MTL::LoadActionClear);
        colorAttachment->setStoreAction(MTL::StoreActionStore);
        colorAttachment->setClearColor(MTL::ClearColor::Make(0.1, 0.2, 0.4, 1.0));
        
		bool isRunning = true;
		while (isRunning) {
			SDL_Event event;
			while (SDL_PollEvent(&event)) {
				if (event.type == SDL_EVENT_QUIT) {
					isRunning = false;
				}
			}

			NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

			auto* drawable = m_Layer->nextDrawable();
			if (drawable == nullptr) {
				pool->release();
				continue;
			}

			colorAttachment->setTexture(drawable->texture());
			renderDescriptor->setRenderTargetWidth(drawable->texture()->width());
			renderDescriptor->setRenderTargetHeight(drawable->texture()->height());

			m_Allocators[m_FrameIndex]->reset();
			m_CmdBuffers->beginCommandBuffer(m_Allocators[m_FrameIndex]);
			auto* encoder = m_CmdBuffers->renderCommandEncoder(renderDescriptor);
			encoder->setRenderPipelineState(m_PipelineState);
			encoder->setArgumentTable(table, RenderStageVertex | RenderStageFragment);
			encoder->drawIndexedPrimitives(PrimitiveTypeTriangle, 6, IndexTypeUInt16, m_IndexBuffer->gpuAddress(),
			                               m_IndexBuffer->length());
			encoder->endEncoding();
			m_CmdBuffers->endCommandBuffer();
			m_Queue->wait(drawable);
			m_Queue->commit(&m_CmdBuffers, 1);
            m_Queue->signalDrawable(drawable);
			drawable->present();

			pool->release();

			m_FrameIndex = (m_FrameIndex + 1) % k_MaxFramesInFlight;
		}

		table->release();
		renderDescriptor->release();
	}

	void MetalEngine::shutdown() {
		m_Texture.shutdown();
		m_ResidencySet->release();
		m_IndexBuffer->release();
		m_VertexBuffer->release();
		m_PipelineState->release();
		for (auto* allocator : m_Allocators) {
			allocator->release();
		}
		m_CmdBuffers->release();
		m_Queue->release();
        SDL_Metal_DestroyView(m_MetalView);
		m_Device->release();
	}

	void MetalEngine::createRenderPipelineState() {
		NS::Error* error = nullptr;
		auto* compilerDescriptor = MTL4::CompilerDescriptor::alloc()->init();
		auto* compiler = m_Device->newCompiler(compilerDescriptor, &error);
		checkError(compiler, error, "Shader compiler creation");

		auto* pipelineDesc = MTL4::RenderPipelineDescriptor::alloc()->init();
		auto* colorAttachment = pipelineDesc->colorAttachments()->object(0);
		// Must match the layer's pixel format and the render pass attachment.
		colorAttachment->setPixelFormat(PixelFormatBGRA8Unorm);
		// Standard (non-premultiplied) alpha blending so the PNG's transparent
		// texels show the background instead of black.
		colorAttachment->setBlendingState(MTL4::BlendStateEnabled);
		colorAttachment->setRgbBlendOperation(BlendOperationAdd);
		colorAttachment->setAlphaBlendOperation(BlendOperationAdd);
		colorAttachment->setSourceRGBBlendFactor(BlendFactorSourceAlpha);
		colorAttachment->setDestinationRGBBlendFactor(BlendFactorOneMinusSourceAlpha);
		colorAttachment->setSourceAlphaBlendFactor(BlendFactorSourceAlpha);
		colorAttachment->setDestinationAlphaBlendFactor(BlendFactorOneMinusSourceAlpha);

		auto* compileOptions = MTL::CompileOptions::alloc()->init();
		
		std::string source = getShaderSource("assets/shaders/colored.metal");
		auto* library = m_Device->newLibrary(NS::String::string(source.c_str(), NS::UTF8StringEncoding), compileOptions, &error);
		checkError(library, error, "Shader library compilation");
		auto* vertexFuncDesc = MTL4::LibraryFunctionDescriptor::alloc()->init();
		vertexFuncDesc->setLibrary(library);
		vertexFuncDesc->setName(NS::String::string("vertexShader", NS::UTF8StringEncoding));
		auto* fragmentFuncDesc = MTL4::LibraryFunctionDescriptor::alloc()->init();
		fragmentFuncDesc->setLibrary(library);
		fragmentFuncDesc->setName(NS::String::string("fragmentShader", NS::UTF8StringEncoding));
		pipelineDesc->setVertexFunctionDescriptor(vertexFuncDesc);
		pipelineDesc->setFragmentFunctionDescriptor(fragmentFuncDesc);

		auto* compilerTaskOptions = MTL4::CompilerTaskOptions::alloc()->init();

		m_PipelineState = compiler->newRenderPipelineState(pipelineDesc, compilerTaskOptions, &error);
		checkError(m_PipelineState, error, "Render pipeline creation");

		compilerTaskOptions->release();
		fragmentFuncDesc->release();
		vertexFuncDesc->release();
		library->release();
		compileOptions->release();
		pipelineDesc->release();
		compiler->release();
		compilerDescriptor->release();
	}

	void MetalEngine::createBuffers() {
		std::array<VertexData, 4> vertices {{
			{.Position = {0.5F, 0.5F}, .TexCoords = {1.0f, 0.0f}},
			{.Position = {0.5F, -0.5F}, .TexCoords = {1.0f, 1.0f}},
			{.Position = {-0.5F, -0.5F}, .TexCoords = {0.0f, 1.0f}},
			{.Position = {-0.5F, 0.5F}, .TexCoords = {0.0f, 0.0f}}
		}};
		std::array<uint16_t, 6> indexes{ 0, 1, 2, 0, 2, 3 };

		m_VertexBuffer = m_Device->newBuffer(vertices.data(), vertices.size() * sizeof(VertexData), ResourceStorageModeShared);
		m_IndexBuffer = m_Device->newBuffer(indexes.data(), indexes.size() * sizeof(uint16_t), ResourceStorageModeShared);

		// The layer's residency set only covers drawables; our own buffers need
		// their own set attached to the queue to be GPU-accessible in Metal 4.
		NS::Error* error = nullptr;
		auto* residencyDesc = ResidencySetDescriptor::alloc()->init();
		m_ResidencySet = m_Device->newResidencySet(residencyDesc, &error);
		checkError(m_ResidencySet, error, "Residency set creation");
		residencyDesc->release();

		m_ResidencySet->addAllocation(m_VertexBuffer);
		m_ResidencySet->addAllocation(m_IndexBuffer);
		m_ResidencySet->commit();
		m_Queue->addResidencySet(m_ResidencySet);
	}

	std::string MetalEngine::getShaderSource(std::string_view path) {
		std::ifstream stream{ std::string{ path } };
		if (!stream.is_open()) {
			std::println("MetalEngine::getShaderSource - Can't open {}", path);
			return {};
		}
		std::stringstream buffer;
		buffer << stream.rdbuf();
		return buffer.str();
	}

	void MetalEngine::checkError(const void* result, NS::Error* error, std::string_view context) {
		if (result != nullptr) {
			return;
		}
		const char* message = error != nullptr ? error->localizedDescription()->utf8String() : "no error information";
		std::println("{} failed: {}", context, message);
	}
} // namespace lab::mtl
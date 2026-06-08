#include "ShaderCompiler.h"

#include <array>
#include <string>

#include <volk/volk.h>
#include "slang/slang.h"
#include "slang/slang-com-ptr.h"

#include "Check.h"
#include "VulkanContext.h"

namespace lab {

    // The Slang global session is expensive to create, so build it once and reuse it.
    static slang::IGlobalSession* getGlobalSession() {
        static Slang::ComPtr<slang::IGlobalSession> globalSession = [] {
            Slang::ComPtr<slang::IGlobalSession> session;
            slang::createGlobalSession(session.writeRef());
            return session;
        }();
        return globalSession.get();
    }

    VkShaderModule loadSlangShader(const VulkanContext& ctx, std::string_view path) {
        slang::IGlobalSession* globalSession = getGlobalSession();

        auto targets = std::to_array<slang::TargetDesc>({ { .format = SLANG_SPIRV, .profile = globalSession->findProfile("spirv_1_4") } });
        auto options = std::to_array<slang::CompilerOptionEntry>({ { slang::CompilerOptionName::EmitSpirvDirectly, { slang::CompilerOptionValueKind::Int, 1 } } });
        slang::SessionDesc sessionDesc{
            .targets = targets.data(),
            .targetCount = SlangInt(targets.size()),
            .defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
            .compilerOptionEntries = options.data(),
            .compilerOptionEntryCount = uint32_t(options.size()),
        };

        Slang::ComPtr<slang::ISession> session;
        globalSession->createSession(sessionDesc, session.writeRef());

        std::string pathStr(path);
        Slang::ComPtr<slang::IModule> module{ session->loadModuleFromSource("shader", pathStr.c_str(), nullptr, nullptr) };
        Slang::ComPtr<ISlangBlob> spirv;
        module->getTargetCode(0, spirv.writeRef());

        VkShaderModuleCreateInfo shaderModuleCI{ .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = spirv->getBufferSize(), .pCode = static_cast<const uint32_t*>(spirv->getBufferPointer()) };
        VkShaderModule shaderModule{};
        chk(vkCreateShaderModule(ctx.getDevice(), &shaderModuleCI, nullptr, &shaderModule));
        return shaderModule;
    }
}

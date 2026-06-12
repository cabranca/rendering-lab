#include "ShaderCompiler.h"

#include <fstream>
#include <string>
#include <vector>

#include <volk/volk.h>
#include "glslang/Include/glslang_c_shader_types.h"
#include "slang/slang.h"
#include "slang/slang-com-ptr.h"

#include "glslang/include/glslang_c_interface.h"
#include "glslang/Public/resource_limits_c.h"

#include "Check.h"
#include "vulkan/vulkan_core.h"

namespace lab {

	struct Shader {
		std::vector<uint32_t> SPIRV;
		VkShaderModule shaderModule = nullptr;

		void initialize(glslang_program_t* program) {
			size_t programSize = glslang_program_SPIRV_get_size(program);
			SPIRV.resize(programSize);
			glslang_program_SPIRV_get(program, SPIRV.data());
		}
	};

	static bool compileShader(VkDevice device, glslang_stage_t stage, const char* pShaderCode, Shader& shaderModule) {
		glslang_input_t input {
			.language = GLSLANG_SOURCE_GLSL,
			.stage = stage,
			.client = GLSLANG_CLIENT_VULKAN,
			.client_version = GLSLANG_TARGET_VULKAN_1_3,
			.target_language = GLSLANG_TARGET_SPV,
			.target_language_version = GLSLANG_TARGET_SPV_1_3,
			.code = pShaderCode,
			.default_version = 100,
			.default_profile = GLSLANG_NO_PROFILE,
			.force_default_version_and_profile = false,
			.forward_compatible = false,
			.messages = GLSLANG_MSG_DEFAULT_BIT,
			.resource = glslang_default_resource()
		};

		glslang_shader_t* shader = glslang_shader_create(&input);

		if (!glslang_shader_preprocess(shader, &input) || !glslang_shader_parse(shader, &input))
			exit(1);

		glslang_program_t* program = glslang_program_create();
		glslang_program_add_shader(program, shader);

		if (!glslang_program_link(program, GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT))
			exit(1);

		glslang_program_SPIRV_generate(program, stage);

		shaderModule.initialize(program);

		VkShaderModuleCreateInfo shaderCI {
			.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.codeSize = shaderModule.SPIRV.size() * sizeof(uint32_t),
			.pCode = shaderModule.SPIRV.data()
		};
		chk(vkCreateShaderModule(device, &shaderCI, nullptr, &shaderModule.shaderModule));

		glslang_program_delete(program);
		glslang_shader_delete(shader);

		return shaderModule.SPIRV.size() > 0;
	}

	// The Slang global session is expensive to create, so build it once and reuse it.
	static slang::IGlobalSession* getGlobalSession() {
		static Slang::ComPtr<slang::IGlobalSession> globalSession = [] {
			Slang::ComPtr<slang::IGlobalSession> session;
			slang::createGlobalSession(session.writeRef());
			return session;
		}();
		return globalSession.get();
	}

	VkShaderModule loadGLSLShader(VkDevice device, std::string_view path) {
		std::string shaderString;
		std::ifstream f(path.data());
		if (f.is_open()) {
			std::string line;
			while(std::getline(f, line)) {
				shaderString.append(line);
				shaderString.append("\n");
			}

			f.close();
		}

		glslang_stage_t stage;
		if (path.ends_with(".vert"))
			stage = GLSLANG_STAGE_VERTEX;
		else if (path.ends_with(".frag"))
			stage = GLSLANG_STAGE_FRAGMENT;
		else
			exit(1);

		glslang_initialize_process();

		Shader shaderModule;
		VkShaderModule res;

		bool success = compileShader(device, stage, shaderString.c_str(), shaderModule);
		if (success)
			res = shaderModule.shaderModule;

		glslang_finalize_process();
		return res;
	}
} // namespace lab

#include "ShaderCompiler.h"
#include "ResourceLimits.h"
#include <arcana/experimental/array.h>
#include <glslang/Public/ShaderLang.h>
#include <SPIRV/GlslangToSpv.h>
#include <spirv_parser.hpp>
#include <spirv_glsl.hpp>
#include "Console.h"

namespace babylon
{
    extern const TBuiltInResource DefaultTBuiltInResource;

    namespace
    {
        void AddShader(glslang::TProgram& program, glslang::TShader& shader, std::string_view source)
        {
            const std::array<const char*, 1> sources{ source.data() };
            shader.setStrings(sources.data(), gsl::narrow_cast<int>(sources.size()));
            shader.setEnvInput(glslang::EShSourceGlsl, shader.getStage(), glslang::EShClientVulkan, 100);
            shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_0);
            shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);

            if (!shader.parse(&DefaultTBuiltInResource, 100, false, EShMsgDefault))
            {
                Console::Error(shader.getInfoDebugLog());
                throw std::exception();
            }

            program.addShader(&shader);
        }

        std::unique_ptr<spirv_cross::Compiler> CompileShader(glslang::TProgram& program, EShLanguage stage, std::vector<uint32_t>& spirv)
        {
            glslang::GlslangToSpv(*program.getIntermediate(stage), spirv);


            spirv_cross::Parser parser{ spirv };
            parser.parse();

            auto compiler = std::make_unique<spirv_cross::CompilerGLSL>(parser.get_parsed_ir());
            return std::move(compiler);
        }
    }

    ShaderCompiler::ShaderCompiler()
    {
        glslang::InitializeProcess();
    }

    ShaderCompiler::~ShaderCompiler()
    {
        glslang::FinalizeProcess();
    }

    void ShaderCompiler::Compile(std::string_view vertexSource, std::string_view fragmentSource, std::function<void(ShaderInfo, ShaderInfo)> onCompiled)
    {
        glslang::TProgram program;

        glslang::TShader vertexShader{ EShLangVertex };
        AddShader(program, vertexShader, vertexSource);

        glslang::TShader fragmentShader{ EShLangFragment };
        AddShader(program, fragmentShader, fragmentSource);

        if (!program.link(EShMsgDefault))
        {
            Console::Error(program.getInfoDebugLog());
            throw std::exception();
        }

        std::vector<uint32_t> spirvVS;
        auto vertexCompiler = CompileShader(program, EShLangVertex, spirvVS);

        std::vector<uint32_t> spirvFS;
        auto fragmentCompiler = CompileShader(program, EShLangFragment, spirvFS);

        uint8_t* strVertex = (uint8_t*)spirvVS.data();
        uint8_t* strFragment = (uint8_t*)spirvFS.data();
        onCompiled
        (
            { std::move(vertexCompiler), gsl::make_span(strVertex, spirvVS.size() * sizeof(uint32_t)) },
            { std::move(fragmentCompiler), gsl::make_span(strFragment, spirvFS.size() * sizeof(uint32_t)) }
        );
    }
}

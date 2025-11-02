#pragma once
#include <ShaderLib.h>
#include <string>
#include <vector>
#include <shaderc/shaderc.hpp>
#include "ShaderParserPEGTL.h"

using namespace ShaderLib;

namespace Shader {
    /**
     * Compiles GLSL shader code to SPIR-V
     */
    class ShaderCompiler {
    public:
        ShaderCompiler();

        // Compile complete shader from parsed data
        ShaderData Compile(const ParsedShaderData& data);

        // Enable/disable debug printf in shaders
        void SetDebugPrintf(bool enable) { debugPrintf_ = enable; }
        bool GetDebugPrintf() const { return debugPrintf_; }

    private:
        // Compile single stage to SPIR-V
        std::vector<uint32_t> CompileStage(
            const std::string& source,
            Stage stage,
            const std::string& stageName);

        // Setup compiler options
        void SetupCompilerOptions(shaderc::CompileOptions& options);

        // Write debug output
        void WriteDebugSource(
            const std::string& source,
            const std::string& stageName);

        shaderc::Compiler compiler_;
        bool debugPrintf_;
    };

} // namespace Shader
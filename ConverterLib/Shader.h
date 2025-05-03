#pragma once
#include <ShaderLib.h>
#include "ConverterLib.h"

using namespace ShaderLib;

namespace Shader {

    AssetData ProcessShader(const std::string& inputPath, const Converter::Settings& settings);

    // Pre-defined UBO schemas
    UniformBufferObject CreateGlobalUBO();
    UniformBufferObject CreateObjectUBO();

    // Core processing functions
    ShaderData CompileShader(const std::string& source);

    std::pair<std::vector<uint32_t>, ShaderMetadata> CompileStage(
        Stage stage,
        std::string source,
        shaderc::Compiler& compiler,
        const shaderc::CompileOptions& options
    );

    std::string GenerateUBODefinition(const UniformBufferObject& ubo);
    std::string PreprocessShaderSource(const std::string& source, const ShaderMetadata& existingMetadata);
}

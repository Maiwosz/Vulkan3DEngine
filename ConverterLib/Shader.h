#pragma once
#include <ShaderLib.h>
#include "ConverterLib.h"

using namespace ShaderLib;

namespace Shader {

    /**
     * Process shader file and convert to asset format
     * @param inputPath Path to shader source file
     * @param settings Converter settings
     * @return Asset data ready for writing
     */
    AssetData ProcessShader(const std::string& inputPath, const Converter::Settings& settings);

    /**
     * Compile shader source to SPIR-V with metadata
     * @param source Shader source code
     * @return Compiled shader data with SPIR-V and metadata
     */
    ShaderData CompileShader(const std::string& source);

} // namespace Shader
#pragma once
#include <ShaderLib.h>
#include <vector>
#include <cstdint>

using namespace ShaderLib;

namespace Shader {

    /**
     * Extracts reflection data from compiled SPIR-V
     */
    class ShaderReflector {
    public:
        ShaderReflector() = default;

        // Process SPIR-V and extract reflection data
        void ProcessReflectionData(
            const std::vector<uint32_t>& spirv,
            Stage stage,
            ShaderMetadata& metadata);

    private:
        // Extract compute shader specific info
        void ExtractComputeInfo(
            const std::vector<uint32_t>& spirv,
            ComputeShaderInfo& computeInfo);

        // Extract push constant ranges
        void ExtractPushConstants(
            const std::vector<uint32_t>& spirv,
            Stage stage,
            ShaderMetadata& metadata);
    };

} // namespace Shader
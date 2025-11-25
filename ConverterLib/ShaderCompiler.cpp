#include "pch.h"
#include "ShaderCompiler.h"
#include "ShaderParserPEGTL.h"
#include "ShaderBuilder.h"
#include "ShaderReflector.h"
#include <TypeConversions.h>
#include <Serialization.h>
#include <DescriptorBuilder.h>
#include <fstream>
#include <sstream>
#include "BuiltInBuffers.h"

using namespace ShaderLib;
using namespace ShaderLib::TypeConversion;

namespace Shader {

    ShaderCompiler::ShaderCompiler()
        : debugPrintf_(false) {
    }

    void ShaderCompiler::SetupCompilerOptions(shaderc::CompileOptions& options) {
        options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
        options.SetTargetSpirv(shaderc_spirv_version_1_3);
        options.SetOptimizationLevel(shaderc_optimization_level_performance);

        if (debugPrintf_) {
            options.SetGenerateDebugInfo();
            options.AddMacroDefinition("DEBUG_PRINTF_ENABLED", "1");
        }
    }

    void ShaderCompiler::WriteDebugSource(
        const std::string& source,
        const std::string& stageName) {

        std::string filename = "debug_complete_" + stageName + ".glsl";
        std::ofstream debugFile(filename);
        debugFile << source;
        debugFile.close();
    }

    std::vector<uint32_t> ShaderCompiler::CompileStage(
        const std::string& source,
        Stage stage,
        const std::string& stageName) {

        shaderc::CompileOptions options;
        SetupCompilerOptions(options);

        shaderc_shader_kind kind = StageToShadercKind(stage);
        shaderc::SpvCompilationResult module =
            compiler_.CompileGlslToSpv(source, kind, "shader", options);

        if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
            throw std::runtime_error(
                "Shader compilation failed (" + stageName + "): " + module.GetErrorMessage()
            );
        }

        return std::vector<uint32_t>(module.cbegin(), module.cend());
    }

    ShaderData ShaderCompiler::Compile(const ParsedShaderData& data) {
        if (data.stages.empty()) {
            throw std::runtime_error("No shader stages found in source");
        }

        // Validate stage requirements for output/inputOutput data
        if (data.HasOutputData() || data.HasInputOutputData()) {
            bool hasComputeOrFragment = false;
            for (const auto& stage : data.stages) {
                if (stage.stage == Stage::Compute || stage.stage == Stage::Fragment) {
                    hasComputeOrFragment = true;
                    break;
                }
            }
            if (!hasComputeOrFragment) {
                throw std::runtime_error(
                    "OutputData/InputOutputData requires compute or fragment stage"
                );
            }
        }

        ShaderBuilder builder;
        ShaderReflector reflector;

        // Setup metadata
        ShaderMetadata metadata;
        metadata.usesGlobalUBO = data.usesGlobalUBO;
        metadata.usesObjectUBO = data.usesObjectUBO;

        // Determine stage flags
        StageFlags stageFlags = 0;
        for (const auto& stage : data.stages) {
            stageFlags |= static_cast<StageFlags>(stage.stage);
        }
        metadata.availableStages = stageFlags;

        // Build descriptor sets
        std::optional<DescriptorSet> globalSet;
        std::optional<DescriptorSet> objectSet;
        std::optional<DescriptorSet> customSet;

        // Build Global descriptor set (set 0)
        if (data.usesGlobalUBO) {
            DescriptorSetBuilder globalBuilder(GLOBAL_DESCRIPTOR_SET);
            auto globalUBO = CreateGlobalUBODefinition();
            globalBuilder.AddBuffer(GLOBAL_UBO_BINDING, globalUBO, stageFlags);
            globalSet = globalBuilder.Build();
        }

        // Build Object descriptor set (set 1)
        if (data.usesObjectUBO) {
            DescriptorSetBuilder objectBuilder(OBJECT_DESCRIPTOR_SET);
            auto objectUBO = CreateObjectUBODefinition();
            objectBuilder.AddBuffer(OBJECT_UBO_BINDING, objectUBO, stageFlags);
            objectSet = objectBuilder.Build();
        }

        // Build Custom descriptor set (set 2) - now fully dynamic
        // No predefined bindings, no special treatment for input/output buffers
        if (data.HasInputData() || data.HasOutputData() ||
            data.HasInputOutputData() || data.HasSamplers()) {

            customSet = builder.BuildCustomDescriptorSet(data);
        }

        // Add descriptor sets to metadata
        if (globalSet) {
            metadata.descriptorSets.push_back(*globalSet);
        }
        if (objectSet) {
            metadata.descriptorSets.push_back(*objectSet);
        }
        if (customSet) {
            metadata.descriptorSets.push_back(*customSet);
        }

        // Compile stages
        ShaderData result;

        for (const auto& stage : data.stages) {
            // Generate complete shader source
            std::string finalSource = builder.GenerateShaderSource(
                data,
                stage,
                globalSet ? &(*globalSet) : nullptr,
                objectSet ? &(*objectSet) : nullptr,
                customSet ? &(*customSet) : nullptr
            );

            // Write debug output
            std::string stageName = StageToString(stage.stage);
            WriteDebugSource(finalSource, stageName);

            // Compile to SPIR-V
            std::vector<uint32_t> spirv = CompileStage(finalSource, stage.stage, stageName);
            result.stages.push_back({ spirv, stage.stage });

            // Extract reflection data
            reflector.ProcessReflectionData(spirv, stage.stage, metadata);
        }

        // Serialize metadata
        nlohmann::json jsonMetadata = SerializeMetadata(metadata);
        result.metadata = jsonMetadata;

        return result;
    }

} // namespace Shader

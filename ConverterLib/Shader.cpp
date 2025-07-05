#include "pch.h"
#include "Shader.h"
#include <ShaderLib.h>
#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv_cross.hpp>
#include <spirv_cross/spirv_glsl.hpp>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <TypeConversions.h>
#include <Serialization.h>
#include <fstream>
#include <UBODefinitions.h>
#include "ShaderParser.h"

using namespace ShaderLib;
using namespace ShaderLib::TypeConversion;

bool usePritnf = false;

namespace Shader {

    // Create descriptor bindings for input variables
    std::vector<DescriptorBinding> CreateDescriptorBindings(const std::vector<InputVariable>& variables) {
        std::vector<DescriptorBinding> bindings;
        uint32_t bindingIndex = 0;

        // First, create binding for the InputData UBO
        bool hasNonTextureVars = false;
        for (const auto& var : variables) {
            if (!var.isSampler) {
                hasNonTextureVars = true;
                break;
            }
        }

        if (hasNonTextureVars) {
            bindings.push_back({
                CUSTOM_DESCRIPTOR_SET,
                bindingIndex++,
                DescriptorType::UniformBuffer,
                static_cast<StageFlags>(Stage::Vertex) | static_cast<StageFlags>(Stage::Fragment),
                "inputData"
                });
        }

        // Create bindings for textures and samplers
        for (const auto& var : variables) {
            if (var.isSampler) {
                DescriptorType type;

                if (var.type == "sampler") {
                    type = DescriptorType::CombinedImageSampler;
                }

                bindings.push_back({
                    CUSTOM_DESCRIPTOR_SET,
                    bindingIndex++,
                    type,
                    static_cast<StageFlags>(Stage::Vertex) | static_cast<StageFlags>(Stage::Fragment),
                    var.name
                    });
            }
        }

        return bindings;
    }

    // Build custom UBO from input variables
    UniformBufferObject BuildInputDataUBO(const std::vector<InputVariable>& variables) {
        UBOBuilder builder("InputData", CUSTOM_DESCRIPTOR_SET, 0);

        for (const auto& var : variables) {
            // Skip textures and samplers
            if (var.isSampler) continue;

            // Parse variable type and add to builder
            if (var.type == "float") {
                builder.AddField<float>(var.name);
            }
            else if (var.type == "vec2") {
                builder.AddField<glm::vec2>(var.name);
            }
            else if (var.type == "vec3") {
                builder.AddField<glm::vec3>(var.name);
            }
            else if (var.type == "vec4") {
                builder.AddField<glm::vec4>(var.name);
            }
            else if (var.type == "int") {
                builder.AddField<int>(var.name);
            }
            else if (var.type == "bool") {
                builder.AddField<bool>(var.name);
            }
            else if (var.type == "mat4") {
                builder.AddField<glm::mat4>(var.name);
            }
            // Add more types as needed
        }

        return builder.Build();
    }

    // Generate texture and sampler declarations
    std::string GenerateTextureBindings(const std::vector<InputVariable>& variables, uint32_t startBinding) {
        std::stringstream ss;
        uint32_t binding = startBinding;

        for (const auto& var : variables) {
            if (var.isSampler) {
                std::string typeStr = "sampler2D"; // Default to sampler2D for texture samplers
                ss << "layout(set = " << CUSTOM_DESCRIPTOR_SET << ", binding = " << binding++ << ") "
                    << "uniform " << typeStr << " " << var.name << ";\n";
            }
        }

        return ss.str();
    }

    // Generate custom UBO GLSL
    std::string GenerateCustomUBO(const UniformBufferObject& ubo) {
        std::stringstream ss;

        ss << "layout(set = " << CUSTOM_DESCRIPTOR_SET << ", binding = 0) uniform InputDataUBO {\n";
        for (const auto& var : ubo.variables) {
            ss << "    " << UniformTypeToString(var.type) << " " << var.name << ";\n";
        }
        ss << "} inputData;\n";

        return ss.str();
    }

    // Generate shader source from scratch
    std::string GenerateShaderSource(const ParsedShaderData& data, const ShaderStage& stage,
        const UniformBufferObject* customUBO = nullptr) {
        std::stringstream ss;

        // 1. Version line must be first
        ss << data.versionLine << "\n\n";

        if (usePritnf) {
            ss << "#extension GL_EXT_debug_printf : enable" << "\n\n";
        }

        // 2. Add global UBO if needed
        if (data.usesGlobalUBO) {
            ss << UBORegistry::Get().GenerateGLSL("GlobalUBO") << "\n\n";
        }

        // 3. Add object UBO if needed
        if (data.usesObjectUBO) {
            ss << UBORegistry::Get().GenerateGLSL("ObjectUBO") << "\n\n";
        }

        // 4. Add custom UBO if needed
        bool hasNonTextureVars = false;
        for (const auto& var : data.inputVariables) {
            if (!var.isSampler) {
                hasNonTextureVars = true;
                break;
            }
        }

        if (hasNonTextureVars && customUBO) {
            ss << GenerateCustomUBO(*customUBO) << "\n";
        }

        // 5. Add texture bindings
        uint32_t startBinding = hasNonTextureVars ? 1 : 0;
        std::string textureBindings = GenerateTextureBindings(data.inputVariables, startBinding);
        if (!textureBindings.empty()) {
            ss << textureBindings << "\n";
        }

        // 6. Add stage-specific code
        ss << stage.code;

        return ss.str();
    }

    // Process reflection data from SPIR-V
    void ProcessReflectionData(const std::vector<uint32_t>& spirv, Stage stage, ShaderMetadata& metadata) {
        spirv_cross::Compiler spirvCompiler(spirv);
        spirv_cross::ShaderResources resources = spirvCompiler.get_shader_resources();

        // Extract push constants if any
        for (const auto& resource : resources.push_constant_buffers) {
            const auto& ranges = spirvCompiler.get_active_buffer_ranges(resource.id);
            uint32_t size = 0;

            for (const auto& range : ranges) {
                size = std::max<uint32_t>(size, range.offset + range.range);
            }

            PushConstantRange pcRange = {
                static_cast<StageFlags>(stage),
                0, // offset
                size
            };

            // Check if we already have this push constant range
            bool found = false;
            for (auto& existing : metadata.pushConstants) {
                if (existing.offset == pcRange.offset && existing.size == pcRange.size) {
                    existing.stages |= pcRange.stages;
                    found = true;
                    break;
                }
            }

            if (!found) {
                metadata.pushConstants.push_back(pcRange);
            }
        }

        // Add more reflection data processing as needed
    }

    ShaderData CompileShader(const std::string& source) {
        // Initialize shader compiler
        shaderc::Compiler compiler;
        shaderc::CompileOptions options;
        options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
        options.SetTargetSpirv(shaderc_spirv_version_1_3);
        options.SetOptimizationLevel(shaderc_optimization_level_performance);

        if (usePritnf) {
            options.SetGenerateDebugInfo();
            options.AddMacroDefinition("DEBUG_PRINTF_ENABLED", "1");
        }

        ShaderData result;
        ShaderMetadata metadata;
        StageFlags stageFlags = 0;

        // Simply parse the source - all complexity is handled by ParserDictionary
        ShaderParser parser;
        ParsedShaderData sourceData = parser.Parse(source);

        // Check if parser found any real errors (not just info messages)
        auto& errorManager = ShaderErrorManager::Instance();
        if (errorManager.HasNonWarningErrors()) {
            throw std::runtime_error("Shader parsing failed with errors:\n" + errorManager.FormatAllErrors());
        }

        // Check if we have any stages to compile
        if (sourceData.stages.empty()) {
            throw std::runtime_error("No shader stages found in source");
        }

        // Set UBO usage in metadata
        metadata.usesGlobalUBO = sourceData.usesGlobalUBO;
        metadata.usesObjectUBO = sourceData.usesObjectUBO;

        // Create descriptor bindings
        std::vector<DescriptorBinding> customDescriptors = CreateDescriptorBindings(sourceData.inputVariables);

        // Create custom UBO if needed
        UniformBufferObject customUBO;
        bool hasCustomUbo = false;

        for (const auto& var : sourceData.inputVariables) {
            if (!var.isSampler) {
                hasCustomUbo = true;
                break;
            }
        }

        if (hasCustomUbo) {
            customUBO = BuildInputDataUBO(sourceData.inputVariables);
            metadata.customUBOs.push_back(customUBO);
        }

        // Add standard UBOs to metadata
        if (sourceData.usesGlobalUBO) {
            metadata.globalUBO = UBORegistry::CreateGlobalUBO();
        }

        if (sourceData.usesObjectUBO) {
            metadata.objectUBO = UBORegistry::CreateObjectUBO();
        }

        // Build descriptor list
        if (sourceData.usesGlobalUBO) {
            metadata.descriptors.push_back({
                GLOBAL_DESCRIPTOR_SET,
                0, // binding
                DescriptorType::UniformBuffer,
                0, // will be updated with stages
                "GlobalUBO"
                });
        }

        if (sourceData.usesObjectUBO) {
            metadata.descriptors.push_back({
                OBJECT_DESCRIPTOR_SET,
                0, // binding
                DescriptorType::UniformBuffer,
                0, // will be updated with stages
                "ObjectUBO"
                });
        }

        // Add custom descriptors
        for (const auto& desc : customDescriptors) {
            metadata.descriptors.push_back(desc);
        }

        // Compile each stage
        for (const auto& stage : sourceData.stages) {
            // Update stage flags
            stageFlags |= static_cast<StageFlags>(stage.stage);

            // Update descriptor stages
            for (auto& desc : metadata.descriptors) {
                desc.stages |= static_cast<StageFlags>(stage.stage);
            }

            // Generate shader source for this stage from scratch
            std::string finalSource = GenerateShaderSource(
                sourceData,
                stage,
                hasCustomUbo ? &customUBO : nullptr
            );

            // Debug output
            std::string stageName = StageToString(stage.stage);
            std::string debug_filename = "debug_complete_" + stageName + ".glsl";
            std::ofstream debugFile(debug_filename);
            debugFile << finalSource;
            debugFile.close();

            // Compile shader
            shaderc_shader_kind kind = StageToShadercKind(stage.stage);
            shaderc::SpvCompilationResult module =
                compiler.CompileGlslToSpv(finalSource, kind, "shader", options);

            if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
                throw std::runtime_error("Shader compilation failed: " + module.GetErrorMessage());
            }

            // Extract SPIR-V
            std::vector<uint32_t> spirv(module.cbegin(), module.cend());
            result.stages.push_back({ spirv, stage.stage });

            // Extract reflection data
            ProcessReflectionData(spirv, stage.stage, metadata);
        }

        metadata.availableStages = stageFlags;

        // Serialize metadata
        nlohmann::json jsonMetadata = SerializeMetadata(metadata);
        result.metadata = jsonMetadata;

        return result;
    }

    AssetData ProcessShader(const std::string& inputPath, const Converter::Settings& settings) {
        // Initialize standard UBOs if not already done
        if (ShaderLib::UBORegistry::Get().GetUBO("GlobalUBO") == nullptr) {
            ShaderLib::UBORegistry::Get().InitializeStandardUBOs();
        }

        std::ifstream file(inputPath);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open shader file: " + inputPath);
        }
        std::string source((std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>());

        ShaderData shaderData = CompileShader(source);

        std::string filename = std::filesystem::path(inputPath).filename().string();

        return AssetLib::WriteShader(
            filename,
            shaderData,
            AssetLib::CompressionType::LZ4,
            settings.compressionLevel
        );
    }

} // namespace Shader
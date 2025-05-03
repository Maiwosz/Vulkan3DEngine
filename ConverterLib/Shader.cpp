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

using namespace ShaderLib;
using namespace ShaderLib::TypeConversion;

namespace Shader {

    AssetData ProcessShader(const std::string& inputPath, const Converter::Settings& settings) {
        std::ifstream file(inputPath);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open shader file: " + inputPath);
        }
        std::string source((std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>());

        shaderc::Compiler compiler;
        shaderc::CompileOptions options;
        options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
        options.SetOptimizationLevel(shaderc_optimization_level_performance);

        ShaderData shadarData = CompileShader(inputPath);

        std::string filename = std::filesystem::path(inputPath).filename().string();

        return AssetLib::WriteShader(
            filename,
            shadarData,
            AssetLib::CompressionType::LZ4,
            settings.compressionLevel
        );
    }

    // Define pre-defined UBO schemas
    UniformBufferObject CreateGlobalUBO() {
        UniformBufferObject globalUBO;
        globalUBO.name = "GlobalUBO";
        globalUBO.set = GLOBAL_DESCRIPTOR_SET;
        globalUBO.binding = 0;

        // Define DirectionalLight structure
        UniformVariable dirLightDir;
        dirLightDir.name = "direction";
        dirLightDir.type = UniformType::Vec3;
        dirLightDir.size = 12;
        dirLightDir.offset = 0;

        UniformVariable dirLightColor;
        dirLightColor.name = "color";
        dirLightColor.type = UniformType::Vec4;
        dirLightColor.size = 16;
        dirLightColor.offset = 16; // Aligned to 16 bytes

        // DirectionalLight structure
        UniformVariable dirLight;
        dirLight.name = "directionalLight";
        dirLight.type = UniformType::Struct;
        dirLight.typeName = "DirectionalLight";
        dirLight.size = 32; // Size of DirectionalLight structure
        dirLight.offset = 48; // Aligned after view and proj matrices

        // PointLight structure vars
        UniformVariable pointLightPos;
        pointLightPos.name = "position";
        pointLightPos.type = UniformType::Vec3;
        pointLightPos.size = 12;
        pointLightPos.offset = 0;

        UniformVariable pointLightRadius;
        pointLightRadius.name = "radius";
        pointLightRadius.type = UniformType::Float;
        pointLightRadius.size = 4;
        pointLightRadius.offset = 12;

        UniformVariable pointLightColor;
        pointLightColor.name = "color";
        pointLightColor.type = UniformType::Vec4;
        pointLightColor.size = 16;
        pointLightColor.offset = 16; // Aligned to 16 bytes

        // PointLight array
        UniformVariable pointLights;
        pointLights.name = "pointLights";
        pointLights.type = UniformType::Array;
        pointLights.typeName = "PointLight";
        pointLights.arraySize = 64;
        pointLights.size = 64 * 32; // 64 PointLight structures
        pointLights.offset = 80; // After directionalLight

        // Define main GlobalUBO variables
        UniformVariable viewMatrix;
        viewMatrix.name = "view";
        viewMatrix.type = UniformType::Mat4;
        viewMatrix.size = 16 * 4;
        viewMatrix.offset = 0;

        UniformVariable projMatrix;
        projMatrix.name = "proj";
        projMatrix.type = UniformType::Mat4;
        projMatrix.size = 16 * 4;
        projMatrix.offset = 16 * 4; // After view matrix

        UniformVariable cameraPos;
        cameraPos.name = "cameraPosition";
        cameraPos.type = UniformType::Vec3;
        cameraPos.size = 12;
        cameraPos.offset = 32 * 4; // After proj matrix

        UniformVariable activePointLights;
        activePointLights.name = "activePointLights";
        activePointLights.type = UniformType::Int;
        activePointLights.size = 4;
        activePointLights.offset = 80 + (64 * 32); // After pointLights array

        // Add variables to UBO
        globalUBO.variables.push_back(viewMatrix);
        globalUBO.variables.push_back(projMatrix);
        globalUBO.variables.push_back(cameraPos);
        globalUBO.variables.push_back(dirLight);
        globalUBO.variables.push_back(pointLights);
        globalUBO.variables.push_back(activePointLights);

        // Calculate total size
        globalUBO.size = 80 + (64 * 32) + 4;

        return globalUBO;
    }

    UniformBufferObject CreateObjectUBO() {
        UniformBufferObject objectUBO;
        objectUBO.name = "ObjectUBO";
        objectUBO.set = OBJECT_DESCRIPTOR_SET;
        objectUBO.binding = 0;

        // Define ObjectUBO variables
        UniformVariable modelMatrix;
        modelMatrix.name = "model";
        modelMatrix.type = UniformType::Mat4;
        modelMatrix.size = 16 * 4;
        modelMatrix.offset = 0;

        // Add variables to UBO
        objectUBO.variables.push_back(modelMatrix);

        // Calculate total size
        objectUBO.size = 16 * 4;

        return objectUBO;
    }

    ShaderData CompileShader(const std::string& source) {
        shaderc::Compiler compiler;
        shaderc::CompileOptions options;
        options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
        options.SetOptimizationLevel(shaderc_optimization_level_performance);

        ShaderData result;
        ShaderMetadata metadata;

        // Initialize pre-defined UBOs
        metadata.globalUBO = CreateGlobalUBO();
        metadata.objectUBO = CreateObjectUBO();

        // Extract shader directives and stage declarations
        std::regex stage_re(R"(#\s*stage\s+(\w+)\s*([\s\S]*?)(?=#\s*stage|\s*$))");
        std::regex use_global_ubo_re(R"(#\s*use\s+global_ubo\s*)");
        std::regex use_object_ubo_re(R"(#\s*use\s+object_ubo\s*)");

        // Check for global directives first
        if (std::regex_search(source, use_global_ubo_re)) {
            metadata.usesGlobalUBO = true;
        }

        if (std::regex_search(source, use_object_ubo_re)) {
            metadata.usesObjectUBO = true;
        }

        // Process each stage
        std::sregex_iterator it(source.begin(), source.end(), stage_re);
        std::sregex_iterator end;

        while (it != end) {
            const auto& match = *it;
            const Stage stage = StringToStage(match[1].str());
            std::string stage_source = match[2].str();

            // Preprocess the stage source to inject UBO definitions if needed
            std::string preprocessed_source = PreprocessShaderSource(stage_source, metadata);

            // Process individual stage
            auto stage_data = CompileStage(stage, preprocessed_source, compiler, options);

            // Merge metadata
            metadata.availableStages |= static_cast<StageFlags>(stage);
            metadata.usesGlobalUBO |= stage_data.second.usesGlobalUBO;
            metadata.usesObjectUBO |= stage_data.second.usesObjectUBO;

            metadata.pushConstants.insert(
                metadata.pushConstants.end(),
                stage_data.second.pushConstants.begin(),
                stage_data.second.pushConstants.end()
            );

            // Merge descriptor bindings for non-standard UBOs
            for (const auto& desc : stage_data.second.descriptors) {
                // Skip standard UBOs that are already handled
                if ((desc.name == "GlobalUBO" && desc.set == GLOBAL_DESCRIPTOR_SET) ||
                    (desc.name == "ObjectUBO" && desc.set == OBJECT_DESCRIPTOR_SET)) {
                    continue;
                }

                metadata.descriptors.push_back(desc);
            }

            // Merge custom UBOs
            for (const auto& ubo : stage_data.second.customUBOs) {
                // Skip standard UBOs that are already handled
                if ((ubo.name == "GlobalUBO" && ubo.set == GLOBAL_DESCRIPTOR_SET) ||
                    (ubo.name == "ObjectUBO" && ubo.set == OBJECT_DESCRIPTOR_SET)) {
                    continue;
                }

                metadata.customUBOs.push_back(ubo);
            }

            result.stages.push_back({ std::move(stage_data.first), stage });
            ++it;
        }

        nlohmann::json jsonMetadata = SerializeMetadata(metadata);
        result.metadata = jsonMetadata;

        return result;
    }

    std::pair<std::vector<uint32_t>, ShaderMetadata> CompileStage(
        Stage stage,
        std::string source,
        shaderc::Compiler& compiler,
        const shaderc::CompileOptions& options)
    {
        ShaderMetadata metadata;
        metadata.availableStages = static_cast<StageFlags>(stage);
        metadata.globalUBO = CreateGlobalUBO();
        metadata.objectUBO = CreateObjectUBO();

        // Check for UBO usage in this specific stage
        std::regex use_global_ubo_re(R"(#\s*use\s+global_ubo\s*)");
        std::regex use_object_ubo_re(R"(#\s*use\s+object_ubo\s*)");

        if (std::regex_search(source, use_global_ubo_re)) {
            metadata.usesGlobalUBO = true;
        }

        if (std::regex_search(source, use_object_ubo_re)) {
            metadata.usesObjectUBO = true;
        }

        const auto kind = StageToShadercKind(stage);
        const auto preprocessed = compiler.PreprocessGlsl(source, kind, "shader_src", options);

        if (preprocessed.GetCompilationStatus() != shaderc_compilation_status_success) {
            throw std::runtime_error(preprocessed.GetErrorMessage());
        }

        const auto result = compiler.CompileGlslToSpv(
            preprocessed.cbegin(),
            static_cast<size_t>(std::strlen(preprocessed.cbegin())),
            kind,
            "shader_src",
            "main",
            options
        );

        if (result.GetCompilationStatus() != shaderc_compilation_status_success)
            throw std::runtime_error(result.GetErrorMessage());

        std::vector<uint32_t> spirv(result.cbegin(), result.cend());

        spirv_cross::CompilerGLSL spirv_compiler(spirv.data(), spirv.size());
        spirv_cross::ShaderResources resources = spirv_compiler.get_shader_resources();

        std::map<uint32_t, std::map<ShaderLib::DescriptorType, uint32_t>> bindingCounters;

        auto assign_binding = [&](uint32_t set, ShaderLib::DescriptorType type) {
            return bindingCounters[set][type]++;
            };

        auto process_resource = [&](const auto& resource, ShaderLib::DescriptorType type) {
            const std::string name = spirv_compiler.get_name(resource.id);
            uint32_t set = CUSTOM_DESCRIPTOR_SET;

            // Check if this is a built-in UBO
            if (name == "GlobalUBO") {
                metadata.usesGlobalUBO = true;
                set = GLOBAL_DESCRIPTOR_SET;
            }
            else if (name == "ObjectUBO") {
                metadata.usesObjectUBO = true;
                set = OBJECT_DESCRIPTOR_SET;
            }

            // Create descriptor binding info
            ShaderLib::DescriptorBinding binding{};
            binding.set = set;
            binding.binding = (set == GLOBAL_DESCRIPTOR_SET || set == OBJECT_DESCRIPTOR_SET) ? 0 : assign_binding(set, type);
            binding.type = type;
            binding.stages = static_cast<StageFlags>(stage);
            binding.name = name;

            metadata.descriptors.push_back(binding);

            // For UBOs, extract structure information
            if (type == ShaderLib::DescriptorType::UniformBuffer) {
                UniformBufferObject ubo{};
                ubo.name = name;
                ubo.set = set;
                ubo.binding = binding.binding;

                const auto& type = spirv_compiler.get_type(resource.base_type_id);
                ubo.size = static_cast<uint32_t>(spirv_compiler.get_declared_struct_size(type));

                const auto member_count = type.member_types.size();
                for (uint32_t i = 0; i < member_count; i++) {
                    UniformVariable var{};
                    var.name = spirv_compiler.get_member_name(resource.base_type_id, i);

                    const auto& member_type = spirv_compiler.get_type(type.member_types[i]);
                    var.type = SPIRTypeToUniformType(member_type);
                    var.typeName = (var.type == UniformType::Struct) ?
                        spirv_compiler.get_name(type.member_types[i]) :
                        UniformTypeToString(var.type);

                    // Handle arrays
                    if (!member_type.array.empty()) {
                        var.arraySize = member_type.array[0];
                        var.size = ComputeArraySize(member_type, spirv_compiler);
                    }
                    else {
                        var.arraySize = 0;
                        var.size = spirv_compiler.get_declared_struct_member_size(type, i);
                    }

                    var.offset = spirv_compiler.type_struct_member_offset(type, i);
                    ubo.variables.push_back(var);
                }

                // Store UBO in the appropriate place
                if (name == "GlobalUBO") {
                    metadata.globalUBO = ubo;
                }
                else if (name == "ObjectUBO") {
                    metadata.objectUBO = ubo;
                }
                else {
                    metadata.customUBOs.push_back(ubo);
                }
            }
            };

        // Process resources
        for (const auto& res : resources.uniform_buffers)
            process_resource(res, ShaderLib::DescriptorType::UniformBuffer);

        for (const auto& res : resources.sampled_images)
            process_resource(res, ShaderLib::DescriptorType::CombinedImageSampler);

        for (const auto& res : resources.separate_images)
            process_resource(res, ShaderLib::DescriptorType::SeparateImage);

        for (const auto& res : resources.separate_samplers)
            process_resource(res, ShaderLib::DescriptorType::SeparateSampler);

        // Process push constants
        for (const auto& pc : resources.push_constant_buffers) {
            const auto& ranges = spirv_compiler.get_active_buffer_ranges(pc.id);
            uint32_t total_size = 0;

            for (const auto& range : ranges) {
                total_size = std::max(total_size, static_cast<uint32_t>(range.offset + range.range));
            }

            metadata.pushConstants.push_back({
                static_cast<StageFlags>(stage),
                0,
                total_size
                });
        }

        return { std::move(spirv), metadata };
    }

    std::string GenerateUBODefinition(const UniformBufferObject& ubo) {
        std::stringstream ss;

        // Handle standard UBO types with special struct definitions
        if (ubo.name == "GlobalUBO") {
            // First define the DirectionalLight and PointLight structures
            ss << "struct DirectionalLight {\n";
            ss << "    vec3 direction;\n";
            ss << "    vec4 color; // w is for intensity\n";
            ss << "};\n\n";

            ss << "struct PointLight {\n";
            ss << "    vec3 position;\n";
            ss << "    float radius;\n";
            ss << "    vec4 color; // w is for intensity\n";
            ss << "};\n\n";
        }

        // Generate the UBO layout
        ss << "layout(std140, set = " << ubo.set << ", binding = " << ubo.binding << ") uniform " << ubo.name << " {\n";

        for (const auto& var : ubo.variables) {
            ss << "    ";

            if (var.type == UniformType::Array) {
                ss << var.typeName << " " << var.name << "[" << var.arraySize << "]";
            }
            else if (var.type == UniformType::Struct) {
                ss << var.typeName << " " << var.name;
            }
            else {
                ss << UniformTypeToString(var.type) << " " << var.name;
            }

            ss << ";\n";
        }

        ss << "};\n\n";

        return ss.str();
    }

    std::string PreprocessShaderSource(const std::string& source, const ShaderMetadata& metadata) {
        std::stringstream result;
        std::string processed = source;

        // Remove any '#use global_ubo' or '#use object_ubo' directives
        std::regex use_global_ubo_re(R"(#\s*use\s+global_ubo\s*)");
        std::regex use_object_ubo_re(R"(#\s*use\s+object_ubo\s*)");

        processed = std::regex_replace(processed, use_global_ubo_re, "");
        processed = std::regex_replace(processed, use_object_ubo_re, "");

        // Find version declaration to insert definitions after it
        std::regex version_re(R"(#\s*version\s+\d+(\s+\w+)?\s*)");
        std::smatch version_match;

        if (std::regex_search(processed, version_match, version_re)) {
            // Put the version declaration first
            result << version_match[0].str() << "\n\n";

            // Insert global UBO if needed
            if (metadata.usesGlobalUBO) {
                result << GenerateUBODefinition(metadata.globalUBO);
            }

            // Insert object UBO if needed
            if (metadata.usesObjectUBO) {
                result << GenerateUBODefinition(metadata.objectUBO);
            }

            // Insert the rest of the shader code (without the version declaration)
            result << processed.substr(version_match[0].length());
        }
        else {
            // No version declaration found, just add UBOs at the beginning
            if (metadata.usesGlobalUBO) {
                result << GenerateUBODefinition(metadata.globalUBO);
            }

            if (metadata.usesObjectUBO) {
                result << GenerateUBODefinition(metadata.objectUBO);
            }

            result << processed;
        }

        return result.str();
    }

} // namespace ShaderLib
#include "pch.h"
#include "ShaderBuilder.h"
#include <TypeConversions.h>
#include <BuiltInBuffers.h>
#include <sstream>

using namespace ShaderLib;
using namespace ShaderLib::TypeConversion;

namespace Shader {

    void ShaderBuilder::ValidateVariables(
        const std::vector<InputVariable>& vars,
        const std::string& structName,
        bool allowSamplers) {

        for (const auto& var : vars) {
            if (var.isSampler && !allowSamplers) {
                throw std::runtime_error(
                    "Sampler '" + var.name + "' cannot be in " + structName +
                    ". Use Samplers structure instead."
                );
            }
            if (!var.isSampler && allowSamplers && structName == "Samplers") {
                throw std::runtime_error(
                    "Non-sampler variable '" + var.name + "' in Samplers structure"
                );
            }
        }
    }

    std::shared_ptr<StructureDefinition> ShaderBuilder::BuildStructDefinitionFromParsed(
        const StructDefinition& def,
        LayoutStandard standard) {

        auto structDef = MakeStruct(def.name);

        for (const auto& field : def.fields) {
            if (field.type.isStruct) {
                if (!field.type.structDef) {
                    throw std::runtime_error(
                        "Struct field '" + field.name + "' has no struct definition"
                    );
                }

                // Recursively build nested struct definition
                auto nestedStructDef = BuildStructDefinitionFromParsed(
                    *field.type.structDef,
                    standard
                );

                // Add field using fluent API
                structDef->AddField(field.name, nestedStructDef, field.type.arraySize);
            }
            else {
                // Base type field
                BaseType baseType = StringToBaseType(field.type.baseType);
                if (baseType == BaseType::Unknown) {
                    throw std::runtime_error(
                        "Unknown type '" + field.type.baseType +
                        "' for field '" + field.name + "'"
                    );
                }

                // Add field using fluent API (arraySize=0 means no array)
                structDef->AddField(field.name, baseType, field.type.arraySize);
            }
        }

        return structDef;
    }

    void ShaderBuilder::AddVariableToStructure(
        std::shared_ptr<StructureDefinition> structDef,
        const InputVariable& var,
        LayoutStandard standard) {

        if (var.typeInfo.isStruct) {
            // Build nested struct definition
            auto nestedStructDef = BuildStructDefinitionFromParsed(
                *var.typeInfo.structDef,
                standard
            );

            // Add to structure (arraySize=0 means no array)
            structDef->AddField(var.name, nestedStructDef, var.typeInfo.arraySize);
            return;
        }

        // Simple base type
        BaseType baseType = StringToBaseType(var.typeInfo.baseType);
        if (baseType == BaseType::Unknown) {
            throw std::runtime_error("Unknown type: " + var.typeInfo.baseType);
        }

        // Add to structure (arraySize=0 means no array)
        structDef->AddField(var.name, baseType, var.typeInfo.arraySize);
    }

    std::shared_ptr<BufferObjectDefinition> ShaderBuilder::BuildInputBuffer(
        const std::vector<InputVariable>& variables) {

        ValidateVariables(variables, "InputData", false);

        // Create structure definition
        auto structure = MakeStruct("InputData");

        // Add all variables to the structure
        for (const auto& var : variables) {
            AddVariableToStructure(structure, var, LayoutStandard::Std140);
        }

        // Create uniform buffer with the structure
        // MakeUniformBuffer automatically creates BufferLayout with std140
        auto bufferDef = MakeUniformBuffer(structure);

        return bufferDef;
    }

    std::shared_ptr<BufferObjectDefinition> ShaderBuilder::BuildOutputBuffer(
        const std::vector<InputVariable>& variables) {

        ValidateVariables(variables, "OutputData", false);

        // Create structure definition
        auto structure = MakeStruct("OutputData");

        // Add all variables to the structure
        for (const auto& var : variables) {
            AddVariableToStructure(structure, var, LayoutStandard::Std430);
        }

        // Create storage buffer with the structure
        // MakeStorageBuffer automatically creates BufferLayout with std430
        auto bufferDef = MakeStorageBuffer(structure, LayoutStandard::Std430);

        return bufferDef;
    }

    std::shared_ptr<BufferObjectDefinition> ShaderBuilder::BuildInputOutputBuffer(
        const std::vector<InputVariable>& variables) {

        ValidateVariables(variables, "InputOutputData", false);

        // Create structure definition
        auto structure = MakeStruct("InputOutputData");

        // Add all variables to the structure
        for (const auto& var : variables) {
            AddVariableToStructure(structure, var, LayoutStandard::Std430);
        }

        // Create storage buffer with the structure
        auto bufferDef = MakeStorageBuffer(structure, LayoutStandard::Std430);

        return bufferDef;
    }

    DescriptorType ShaderBuilder::GetSamplerDescriptorType(const std::string& typeStr) {
        if (typeStr.empty() || typeStr == "sampler2D") {
            return DescriptorType::Sampler2D;
        }

        return StringToDescriptorType(typeStr);
    }

    DescriptorSet ShaderBuilder::BuildCustomDescriptorSet(
        const ParsedShaderData& data,
        std::shared_ptr<const BufferObjectDefinition> inputBuffer,
        std::shared_ptr<const BufferObjectDefinition> outputBuffer,
        std::shared_ptr<const BufferObjectDefinition> inputOutputBuffer) {

        ValidateVariables(data.samplerVariables, "Samplers", true);

        DescriptorSetBuilder builder(CUSTOM_DESCRIPTOR_SET);

        // Determine stage flags based on shader stages
        StageFlags bufferStages = 0;
        StageFlags samplerStages = 0;

        for (const auto& stage : data.stages) {
            StageFlags stageFlag = static_cast<StageFlags>(stage.stage);
            bufferStages |= stageFlag;

            // Samplers typically used in vertex and fragment stages
            if (stage.stage == Stage::Vertex || stage.stage == Stage::Fragment) {
                samplerStages |= stageFlag;
            }
        }

        // Add input buffer (uniform)
        if (inputBuffer) {
            builder.AddBuffer(INPUT_DATA_BINDING, inputBuffer, bufferStages);
        }

        // Add output buffer (storage, usually fragment/compute)
        if (outputBuffer) {
            StageFlags outputStages = 0;
            for (const auto& stage : data.stages) {
                if (stage.stage == Stage::Fragment || stage.stage == Stage::Compute) {
                    outputStages |= static_cast<StageFlags>(stage.stage);
                }
            }
            builder.AddBuffer(OUTPUT_DATA_BINDING, outputBuffer, outputStages);
        }

        // Add input/output buffer (storage, all stages)
        if (inputOutputBuffer) {
            builder.AddBuffer(INPUT_OUTPUT_DATA_BINDING, inputOutputBuffer, bufferStages);
        }

        // Add samplers starting from SAMPLERS_START_BINDING
        uint32_t samplerBinding = SAMPLERS_START_BINDING;
        for (const auto& sampler : data.samplerVariables) {
            DescriptorType samplerType = GetSamplerDescriptorType(sampler.type);
            builder.AddSampler(samplerBinding++, sampler.name, samplerType, samplerStages);
        }

        return builder.Build();
    }

    std::string ShaderBuilder::GenerateShaderSource(
        const ParsedShaderData& data,
        const ShaderStage& stage,
        const DescriptorSet* globalSet,
        const DescriptorSet* objectSet,
        const DescriptorSet* customSet) {

        std::stringstream ss;

        // Version
        ss << data.versionLine << "\n\n";

        // Debug printf extension if enabled
        extern bool usePritnf;
        if (usePritnf) {
            ss << "#extension GL_EXT_debug_printf : enable\n\n";
        }

        // Generate descriptor sets using built-in GenerateGLSL
        if (globalSet) {
            ss << globalSet->GenerateGLSL() << "\n";
        }

        if (objectSet) {
            ss << objectSet->GenerateGLSL() << "\n";
        }

        if (customSet) {
            ss << customSet->GenerateGLSL() << "\n";
        }

        // Stage code
        ss << stage.code;

        return ss.str();
    }

} // namespace Shader

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
        const std::string& structName) {

        // Podstawowa walidacja - możesz dodać więcej sprawdzeń
        for (const auto& var : vars) {
            if (var.name.empty()) {
                throw std::runtime_error(
                    "Variable in " + structName + " has empty name"
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

    std::shared_ptr<BufferObjectDefinition> ShaderBuilder::BuildUniformBuffer(
        const std::string& name,
        const std::vector<InputVariable>& variables) {

        ValidateVariables(variables, name);

        // Create structure definition
        auto structure = MakeStruct(name);

        // Add all variables to the structure
        for (const auto& var : variables) {
            AddVariableToStructure(structure, var, LayoutStandard::Std140);
        }

        // Create uniform buffer with the structure
        // MakeUniformBuffer automatically creates BufferLayout with std140
        auto bufferDef = MakeUniformBuffer(structure);

        return bufferDef;
    }

    std::shared_ptr<BufferObjectDefinition> ShaderBuilder::BuildStorageBuffer(
        const std::string& name,
        const std::vector<InputVariable>& variables,
        LayoutStandard standard) {

        ValidateVariables(variables, name);

        // Create structure definition
        auto structure = MakeStruct(name);

        // Add all variables to the structure
        for (const auto& var : variables) {
            AddVariableToStructure(structure, var, standard);
        }

        // Create storage buffer with the structure
        auto bufferDef = MakeStorageBuffer(structure, standard);

        return bufferDef;
    }

    DescriptorType ShaderBuilder::GetSamplerDescriptorType(const std::string& typeStr) {
        if (typeStr.empty() || typeStr == "sampler2D") {
            return DescriptorType::Sampler2D;
        }

        return StringToDescriptorType(typeStr);
    }

    DescriptorSet ShaderBuilder::BuildCustomDescriptorSet(
        const ParsedShaderData& data) {

        DescriptorSetBuilder builder(CUSTOM_DESCRIPTOR_SET);

        // Determine stage flags based on shader stages
        StageFlags allStages = 0;
        StageFlags samplerStages = 0;

        for (const auto& stage : data.stages) {
            StageFlags stageFlag = static_cast<StageFlags>(stage.stage);
            allStages |= stageFlag;

            // Samplers typically used in vertex and fragment stages
            if (stage.stage == Stage::Vertex || stage.stage == Stage::Fragment) {
                samplerStages |= stageFlag;
            }
        }

        // Dynamic binding assignment - starts from 0
        uint32_t nextBinding = 0;

        // Filter out samplers from input variables
        std::vector<InputVariable> nonSamplerInputs;
        for (const auto& var : data.inputVariables) {
            if (!var.isSampler) {
                nonSamplerInputs.push_back(var);
            }
        }

        // Add single input buffer with ALL input variables (non-samplers)
        if (!nonSamplerInputs.empty()) {
            auto buffer = BuildUniformBuffer("InputData", nonSamplerInputs);
            builder.AddBuffer(nextBinding++, buffer, allStages);
        }

        // Filter out samplers from output variables
        std::vector<InputVariable> nonSamplerOutputs;
        for (const auto& var : data.outputVariables) {
            if (!var.isSampler) {
                nonSamplerOutputs.push_back(var);
            }
        }

        // Add single output buffer with ALL output variables (non-samplers)
        if (!nonSamplerOutputs.empty()) {
            StageFlags outputStages = 0;
            for (const auto& stage : data.stages) {
                if (stage.stage == Stage::Fragment || stage.stage == Stage::Compute) {
                    outputStages |= static_cast<StageFlags>(stage.stage);
                }
            }

            auto buffer = BuildStorageBuffer("OutputData", nonSamplerOutputs, LayoutStandard::Std430);
            builder.AddBuffer(nextBinding++, buffer, outputStages);
        }

        // Filter out samplers from input/output variables
        std::vector<InputVariable> nonSamplerInputOutputs;
        for (const auto& var : data.inputOutputVariables) {
            if (!var.isSampler) {
                nonSamplerInputOutputs.push_back(var);
            }
        }

        // Add single input/output buffer with ALL input/output variables (non-samplers)
        if (!nonSamplerInputOutputs.empty()) {
            auto buffer = BuildStorageBuffer("InputOutputData", nonSamplerInputOutputs, LayoutStandard::Std430);
            builder.AddBuffer(nextBinding++, buffer, allStages);
        }

        // Add samplers
        for (const auto& sampler : data.samplerVariables) {
            DescriptorType samplerType = GetSamplerDescriptorType(sampler.type);
            builder.AddSampler(nextBinding++, sampler.name, samplerType, samplerStages);
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

#include "pch.h"
#include "ShaderBuilder.h"
#include "ShaderStruct.h"
#include "ShaderArray.h"
#include <TypeConversions.h>
#include <BuiltInBuffers.h>
#include <Serialization.h>
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

    std::shared_ptr<const ShaderStructDefinition> ShaderBuilder::BuildStructDefinitionFromParsed(
        const StructDefinition& def,
        LayoutStandard standard) {

        auto structDef = std::make_shared<ShaderStructDefinition>(def.name, standard);

        for (const auto& field : def.fields) {
            if (field.type.isStruct) {
                if (!field.type.structDef) {
                    throw std::runtime_error(
                        "Struct field '" + field.name + "' has no struct definition"
                    );
                }

                // Recursively build nested struct definition
                auto nestedStructDef = BuildStructDefinitionFromParsed(*field.type.structDef, standard);

                if (field.type.isArray) {
                    // Create array of structs definition
                    auto arrayDef = std::make_shared<ShaderArrayDefinition>(
                        nestedStructDef,
                        field.type.arraySize,
                        standard
                    );
                    structDef->AddCompositeField(field.name, arrayDef);
                }
                else {
                    // Add struct field directly
                    structDef->AddCompositeField(field.name, nestedStructDef);
                }
            }
            else {
                // Base type field
                BaseType baseType = StringToBaseType(field.type.baseType);
                if (baseType == BaseType::Unknown) {
                    throw std::runtime_error(
                        "Unknown type '" + field.type.baseType + "' for field '" + field.name + "'"
                    );
                }

                if (field.type.isArray) {
                    // Create array of base types definition
                    auto arrayDef = std::make_shared<ShaderArrayDefinition>(
                        baseType,
                        field.type.arraySize,
                        standard
                    );
                    structDef->AddCompositeField(field.name, arrayDef);
                }
                else {
                    // Add base type field directly
                    structDef->AddField(field.name, baseType);
                }
            }
        }

        structDef->Finalize();
        return structDef;
    }

    std::shared_ptr<const CompositeTypeDefinition> ShaderBuilder::BuildCompositeDefinitionFromTypeInfo(
        const TypeInfo& typeInfo,
        LayoutStandard standard) {

        if (typeInfo.isArray) {
            if (typeInfo.isStruct) {
                if (!typeInfo.structDef) {
                    throw std::runtime_error("Array of struct has no struct definition");
                }

                // Build struct definition first
                auto structDef = BuildStructDefinitionFromParsed(*typeInfo.structDef, standard);

                // Create array of structs definition
                return std::make_shared<ShaderArrayDefinition>(
                    structDef,
                    typeInfo.arraySize,
                    standard
                );
            }

            // Array of base types
            BaseType baseType = StringToBaseType(typeInfo.baseType);
            if (baseType == BaseType::Unknown) {
                throw std::runtime_error("Unknown base type for array: " + typeInfo.baseType);
            }

            return std::make_shared<ShaderArrayDefinition>(
                baseType,
                typeInfo.arraySize,
                standard
            );
        }

        if (typeInfo.isStruct) {
            if (!typeInfo.structDef) {
                throw std::runtime_error("Struct type has no definition");
            }

            return BuildStructDefinitionFromParsed(*typeInfo.structDef, standard);
        }

        throw std::runtime_error("TypeInfo is neither struct nor array");
    }

    void ShaderBuilder::AddVariableToBuilder(
        BufferBuilder& builder,
        const InputVariable& var,
        LayoutStandard standard) {

        if (var.typeInfo.isArray || var.typeInfo.isStruct) {
            // Build composite type definition and add it
            auto compositeDef = BuildCompositeDefinitionFromTypeInfo(var.typeInfo, standard);
            builder.AddCompositeField(var.name, compositeDef);
            return;
        }

        // Simple base type
        BaseType baseType = StringToBaseType(var.typeInfo.baseType);
        if (baseType == BaseType::Unknown) {
            throw std::runtime_error("Unknown type: " + var.typeInfo.baseType);
        }

        builder.AddField(var.name, baseType);
    }

    BufferObject ShaderBuilder::BuildInputBuffer(const std::vector<InputVariable>& variables) {
        ValidateVariables(variables, "InputData", false);

        BufferBuilder builder("InputData",
            BufferType::Uniform,
            BufferAccessMode::ReadOnly,
            LayoutStandard::Std140);

        for (const auto& var : variables) {
            AddVariableToBuilder(builder, var, LayoutStandard::Std140);
        }

        return builder.Build();
    }

    BufferObject ShaderBuilder::BuildOutputBuffer(const std::vector<InputVariable>& variables) {
        ValidateVariables(variables, "OutputData", false);

        BufferBuilder builder("OutputData",
            BufferType::Storage,
            BufferAccessMode::WriteOnly,
            LayoutStandard::Std430);

        for (const auto& var : variables) {
            AddVariableToBuilder(builder, var, LayoutStandard::Std430);
        }

        return builder.Build();
    }

    BufferObject ShaderBuilder::BuildInputOutputBuffer(const std::vector<InputVariable>& variables) {
        ValidateVariables(variables, "InputOutputData", false);

        BufferBuilder builder("InputOutputData",
            BufferType::Storage,
            BufferAccessMode::ReadWrite,
            LayoutStandard::Std430);

        for (const auto& var : variables) {
            AddVariableToBuilder(builder, var, LayoutStandard::Std430);
        }

        return builder.Build();
    }

    DescriptorType ShaderBuilder::GetSamplerDescriptorType(const std::string& typeStr) {
        if (typeStr.empty() || typeStr == "sampler2D") {
            return DescriptorType::Sampler2D;
        }

        return StringToDescriptorType(typeStr);
    }

    DescriptorSet ShaderBuilder::BuildCustomDescriptorSet(
        const ParsedShaderData& data,
        const BufferObject* inputBuffer,
        const BufferObject* outputBuffer,
        const BufferObject* inputOutputBuffer) {

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
            builder.AddBuffer(INPUT_DATA_BINDING, *inputBuffer, bufferStages);
        }

        // Add output buffer (storage, usually fragment/compute)
        if (outputBuffer) {
            StageFlags outputStages = 0;
            for (const auto& stage : data.stages) {
                if (stage.stage == Stage::Fragment || stage.stage == Stage::Compute) {
                    outputStages |= static_cast<StageFlags>(stage.stage);
                }
            }
            builder.AddBuffer(OUTPUT_DATA_BINDING, *outputBuffer, outputStages);
        }

        // Add input/output buffer (storage, all stages)
        if (inputOutputBuffer) {
            builder.AddBuffer(INPUT_OUTPUT_DATA_BINDING, *inputOutputBuffer, bufferStages);
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
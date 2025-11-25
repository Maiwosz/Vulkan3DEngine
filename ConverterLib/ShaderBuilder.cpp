#include "pch.h"
#include "ShaderBuilder.h"
#include <TypeConversions.h>
#include <BuiltInBuffers.h>
#include <sstream>

using namespace ShaderLib;
using namespace ShaderLib::TypeConversion;

namespace Shader {

    void ShaderBuilder::ValidateVariables(
        const std::vector<StructField>& fields,
        const std::string& structName) {

        for (const auto& field : fields) {
            if (field.name.empty()) {
                throw std::runtime_error(
                    "Field in " + structName + " has empty name"
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

                auto nestedStructDef = BuildStructDefinitionFromParsed(
                    *field.type.structDef,
                    standard
                );

                structDef->AddField(field.name, nestedStructDef, field.type.arraySize);
            }
            else {
                BaseType baseType = StringToBaseType(field.type.baseType);
                if (baseType == BaseType::Unknown) {
                    throw std::runtime_error(
                        "Unknown type '" + field.type.baseType +
                        "' for field '" + field.name + "'"
                    );
                }

                structDef->AddField(field.name, baseType, field.type.arraySize);
            }
        }

        return structDef;
    }

    std::shared_ptr<StructureDefinition> ShaderBuilder::BuildStructDefinitionFromFields(
        const std::string& name,
        const std::vector<StructField>& fields,
        LayoutStandard standard) {

        auto structDef = MakeStruct(name);

        for (const auto& field : fields) {
            if (field.type.isStruct) {
                if (!field.type.structDef) {
                    throw std::runtime_error(
                        "Struct field '" + field.name + "' has no struct definition"
                    );
                }

                auto nestedStructDef = BuildStructDefinitionFromParsed(
                    *field.type.structDef,
                    standard
                );

                structDef->AddField(field.name, nestedStructDef, field.type.arraySize);
            }
            else {
                BaseType baseType = StringToBaseType(field.type.baseType);
                if (baseType == BaseType::Unknown) {
                    throw std::runtime_error(
                        "Unknown type '" + field.type.baseType +
                        "' for field '" + field.name + "'"
                    );
                }

                structDef->AddField(field.name, baseType, field.type.arraySize);
            }
        }

        return structDef;
    }

    std::shared_ptr<BufferObjectDefinition> ShaderBuilder::BuildBufferFromDefinition(
        const BufferDefinition& bufferDef) {

        ValidateVariables(bufferDef.fields, bufferDef.name);

        // Utwórz strukturę z pól
        LayoutStandard layoutStandard =
            (bufferDef.bufferType == BufferType::Uniform)
            ? LayoutStandard::Std140
            : LayoutStandard::Std430;

        auto structure = BuildStructDefinitionFromFields(
            bufferDef.name,
            bufferDef.fields,
            layoutStandard
        );

        // Utwórz access patterns z wartościami domyślnymi
        BufferAccessPatterns accessPatterns = bufferDef.CreateAccessPatterns();

        // Walidacja
        if (!accessPatterns.IsValid()) {
            throw std::runtime_error(
                "Invalid access patterns for buffer '" + bufferDef.name +
                "': " + accessPatterns.GetValidationError()
            );
        }

        // Utwórz buffer definition
        auto bufferObjDef = std::make_shared<BufferObjectDefinition>(
            structure,
            bufferDef.bufferType,
            accessPatterns,
            layoutStandard
        );

        return bufferObjDef;
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

        // Determine stage flags
        StageFlags allStages = 0;
        StageFlags samplerStages = 0;

        for (const auto& stage : data.stages) {
            StageFlags stageFlag = static_cast<StageFlags>(stage.stage);
            allStages |= stageFlag;

            if (stage.stage == Stage::Vertex || stage.stage == Stage::Fragment) {
                samplerStages |= stageFlag;
            }
        }

        // Dynamic binding assignment
        uint32_t nextBinding = 0;

        // Add all buffers from buffer definitions
        for (const auto& bufferDef : data.bufferDefinitions) {
            auto buffer = BuildBufferFromDefinition(bufferDef);

            // Determine appropriate stage flags based on buffer type and access patterns
            StageFlags bufferStages = allStages;

            // Storage buffers used in compute typically
            if (bufferDef.bufferType == BufferType::Storage) {
                bufferStages = 0;
                for (const auto& stage : data.stages) {
                    if (stage.stage == Stage::Compute || stage.stage == Stage::Fragment) {
                        bufferStages |= static_cast<StageFlags>(stage.stage);
                    }
                }
                // If no compute/fragment, use all stages
                if (bufferStages == 0) {
                    bufferStages = allStages;
                }
            }

            builder.AddBuffer(nextBinding++, buffer, bufferStages);
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

        // Generate descriptor sets
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

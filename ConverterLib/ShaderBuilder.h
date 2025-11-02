#pragma once
#include <ShaderLib.h>
#include <BufferBuilder.h>
#include <DescriptorBuilder.h>
#include <ShaderStruct.h>
#include "ShaderParserPEGTL.h"
#include <memory>
#include <vector>
#include <string>

using namespace ShaderLib;

namespace Shader {

    /**
     * Builds shader buffers and descriptor sets from parsed shader data
     */
    class ShaderBuilder {
    public:
        ShaderBuilder() = default;

        // Build buffer objects from variables
        BufferObject BuildInputBuffer(const std::vector<InputVariable>& variables);
        BufferObject BuildOutputBuffer(const std::vector<InputVariable>& variables);
        BufferObject BuildInputOutputBuffer(const std::vector<InputVariable>& variables);

        // Build complete descriptor set for custom data
        DescriptorSet BuildCustomDescriptorSet(
            const ParsedShaderData& data,
            const BufferObject* inputBuffer,
            const BufferObject* outputBuffer,
            const BufferObject* inputOutputBuffer);

        // Generate complete shader source for a stage
        std::string GenerateShaderSource(
            const ParsedShaderData& data,
            const ShaderStage& stage,
            const DescriptorSet* globalSet,
            const DescriptorSet* objectSet,
            const DescriptorSet* customSet);

    private:
        // Helper: Add variable to buffer builder
        void AddVariableToBuilder(
            BufferBuilder& builder,
            const InputVariable& var,
            LayoutStandard standard);

        // Helper: Build composite type definition from TypeInfo
        std::shared_ptr<const CompositeTypeDefinition> BuildCompositeDefinitionFromTypeInfo(
            const TypeInfo& typeInfo,
            LayoutStandard standard);

        // Helper: Build struct definition from parsed struct
        std::shared_ptr<const ShaderStructDefinition> BuildStructDefinitionFromParsed(
            const StructDefinition& def,
            LayoutStandard standard);

        // Validation
        void ValidateVariables(
            const std::vector<InputVariable>& vars,
            const std::string& structName,
            bool allowSamplers);

        // Helper: Convert sampler type string to DescriptorType
        DescriptorType GetSamplerDescriptorType(const std::string& typeStr);
    };

} // namespace Shader
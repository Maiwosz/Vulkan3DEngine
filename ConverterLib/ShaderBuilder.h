#pragma once
#include <ShaderLib.h>
#include <DescriptorBuilder.h>
#include "ShaderParserPEGTL.h"
#include <memory>
#include <vector>
#include <string>

using namespace ShaderLib;

namespace Shader {

    /**
     * Builds shader buffers and descriptor sets from parsed shader data
     *
     * Refactored to use new ShaderLib API:
     * - BufferObjectDefinition wraps StructureDefinition
     * - Fluent API for building structures
     * - Automatic finalization handling
     */
    class ShaderBuilder {
    public:
        ShaderBuilder() = default;

        // ====================================================================
        // BUFFER BUILDING - Create BufferObjectDefinitions from variables
        // ====================================================================

        /**
         * Build uniform buffer (UBO) for input data
         * - Uses std140 layout
         * - ReadOnly access
         */
        std::shared_ptr<BufferObjectDefinition> BuildInputBuffer(
            const std::vector<InputVariable>& variables);

        /**
         * Build storage buffer (SSBO) for output data
         * - Uses std430 layout
         * - WriteOnly access
         */
        std::shared_ptr<BufferObjectDefinition> BuildOutputBuffer(
            const std::vector<InputVariable>& variables);

        /**
         * Build storage buffer (SSBO) for input/output data
         * - Uses std430 layout
         * - ReadWrite access
         */
        std::shared_ptr<BufferObjectDefinition> BuildInputOutputBuffer(
            const std::vector<InputVariable>& variables);

        // ====================================================================
        // DESCRIPTOR SET BUILDING
        // ====================================================================

        /**
         * Build complete descriptor set for custom data
         * Includes input/output buffers and samplers
         */
        DescriptorSet BuildCustomDescriptorSet(
            const ParsedShaderData& data,
            std::shared_ptr<const BufferObjectDefinition> inputBuffer,
            std::shared_ptr<const BufferObjectDefinition> outputBuffer,
            std::shared_ptr<const BufferObjectDefinition> inputOutputBuffer);

        // ====================================================================
        // SHADER SOURCE GENERATION
        // ====================================================================

        /**
         * Generate complete shader source for a stage
         * Combines descriptor set GLSL with stage code
         */
        std::string GenerateShaderSource(
            const ParsedShaderData& data,
            const ShaderStage& stage,
            const DescriptorSet* globalSet,
            const DescriptorSet* objectSet,
            const DescriptorSet* customSet);

    private:
        // ====================================================================
        // INTERNAL HELPERS
        // ====================================================================

        /**
         * Add variable to structure definition
         * Handles both base types and nested structs
         */
        void AddVariableToStructure(
            std::shared_ptr<StructureDefinition> structDef,
            const InputVariable& var,
            LayoutStandard standard);

        /**
         * Build StructureDefinition from parsed struct
         * Recursively handles nested structs
         */
        std::shared_ptr<StructureDefinition> BuildStructDefinitionFromParsed(
            const StructDefinition& def,
            LayoutStandard standard);

        /**
         * Validate variables (check for sampler placement, etc.)
         */
        void ValidateVariables(
            const std::vector<InputVariable>& vars,
            const std::string& structName,
            bool allowSamplers);

        /**
         * Convert sampler type string to DescriptorType
         */
        DescriptorType GetSamplerDescriptorType(const std::string& typeStr);
    };

} // namespace Shader

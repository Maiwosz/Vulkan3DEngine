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
     * Simplified approach:
     * - No hardcoded bindings for custom descriptor set
     * - All resources added dynamically
     * - Find resources by name, not by predefined constants
     */
    class ShaderBuilder {
    public:
        ShaderBuilder() = default;

        // ====================================================================
        // BUFFER BUILDING - Create BufferObjectDefinitions from variables
        // ====================================================================

        /**
         * Build uniform buffer (UBO) with given name and layout
         */
        std::shared_ptr<BufferObjectDefinition> BuildUniformBuffer(
            const std::string& name,
            const std::vector<InputVariable>& variables);

        /**
         * Build storage buffer (SSBO) with given name and layout
         */
        std::shared_ptr<BufferObjectDefinition> BuildStorageBuffer(
            const std::string& name,
            const std::vector<InputVariable>& variables,
            LayoutStandard standard = LayoutStandard::Std430);

        // ====================================================================
        // DESCRIPTOR SET BUILDING
        // ====================================================================

        /**
         * Build complete custom descriptor set from parsed data
         * - Dynamically assigns bindings starting from 0
         * - Adds all buffers and samplers in order
         * - No hardcoded binding positions
         */
        DescriptorSet BuildCustomDescriptorSet(
            const ParsedShaderData& data);

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
         * Validate variables (check for type compatibility, etc.)
         */
        void ValidateVariables(
            const std::vector<InputVariable>& vars,
            const std::string& structName);

        /**
         * Convert sampler type string to DescriptorType
         */
        DescriptorType GetSamplerDescriptorType(const std::string& typeStr);
    };

} // namespace Shader

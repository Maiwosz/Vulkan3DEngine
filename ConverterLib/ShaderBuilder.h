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
     */
    class ShaderBuilder {
    public:
        ShaderBuilder() = default;

        // ====================================================================
        // BUFFER BUILDING z BufferDefinition
        // ====================================================================

        /**
         * Build BufferObjectDefinition from parsed BufferDefinition
         * - Automatycznie określa layout standard (Std140 dla uniform, Std430 dla storage)
         * - Wypełnia brakujące wartości access patterns domyślnymi dla danego typu
         * - Waliduje poprawność access patterns
         */
        std::shared_ptr<BufferObjectDefinition> BuildBufferFromDefinition(
            const BufferDefinition& bufferDef);

        // ====================================================================
        // DESCRIPTOR SET BUILDING
        // ====================================================================

        /**
         * Build complete custom descriptor set from parsed data
         * - Dynamicznie przypisuje bindingi startując od 0
         * - Buduje bufory z BufferDefinition zamiast predefiniowanych zmiennych
         * - Dodaje wszystkie bufory i samplery w kolejności
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
         * Build StructureDefinition from parsed struct
         * Recursively handles nested structs
         */
        std::shared_ptr<StructureDefinition> BuildStructDefinitionFromParsed(
            const StructDefinition& def,
            LayoutStandard standard);

        /**
         * Build StructureDefinition from vector of StructFields
         * Używane przy budowaniu struktur z BufferDefinition
         */
        std::shared_ptr<StructureDefinition> BuildStructDefinitionFromFields(
            const std::string& name,
            const std::vector<StructField>& fields,
            LayoutStandard standard);

        /**
         * Validate fields (check for type compatibility, etc.)
         */
        void ValidateVariables(
            const std::vector<StructField>& fields,
            const std::string& structName);

        /**
         * Convert sampler type string to DescriptorType
         */
        DescriptorType GetSamplerDescriptorType(const std::string& typeStr);
    };

} // namespace Shader

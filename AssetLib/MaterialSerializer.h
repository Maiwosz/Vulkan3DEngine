#pragma once
#include "MaterialTypes.h"
#include "AssetLib.h"
#include "BufferObjectDefinition.h"
#include "BufferObjectInstance.h"
#include <json.hpp>
#include <memory>

namespace AssetLib {
    using json = nlohmann::json;

    // ============================================================================
    // HIGH-LEVEL SERIALIZATION
    // ============================================================================

    // Serialize Material to AssetData (ready to write to file)
    AssetData WriteMaterial(
        const std::string& source,
        const MaterialDefinition& material,
        CompressionType compression = CompressionType::LZ4,
        int compressionLevel = 1
    );

    // Deserialize Material from AssetData
    MaterialDefinition ReadMaterial(const AssetData& asset);

    // Read only shader name and texture dependencies (fast, no decompression)
    std::string GetMaterialShaderName(const AssetData& asset);
    std::vector<std::string> GetMaterialTextureDependencies(const AssetData& asset);
    std::unordered_map<std::string, ColorSpace> GetMaterialTextureColorSpaces(const AssetData& asset);

    // ============================================================================
    // BUFFER INSTANCE CREATION
    // ============================================================================

    /**
     * Create BufferObjectInstance from shader definition and material field values
     *
     * This is the CORE function that creates buffer instances from assets.
     *
     * @param shaderBufferDef: Buffer definition from shader (structure source of truth)
     * @param materialFieldValues: JSON object with field values from .mat file
     *
     * @return New instance with:
     *   - Structure matching shaderBufferDef (this is always correct)
     *   - Values filled from materialFieldValues (where field names match)
     *   - Default values for fields not in materialFieldValues
     *   - Extra fields in materialFieldValues are ignored (backwards compatibility)
     *
     * No validation needed - shader definition is always correct.
     */
    std::shared_ptr<ShaderLib::BufferObjectInstance> CreateBufferInstanceFromMaterial(
        std::shared_ptr<const ShaderLib::BufferObjectDefinition> shaderBufferDef,
        const json& materialFieldValues
    );

    // ============================================================================
    // JSON CONVERSIONS (for .mat text files)
    // ============================================================================

    // Material JSON format (simplified - only field values):
    // {
    //   "shader": "ShaderName",
    //   "buffers": {
    //     "BufferName1": {
    //       "fieldName": value,
    //       "nested": { "field": value },
    //       "arrayField": [value1, value2, ...]
    //     },
    //     "BufferName2": { ... }
    //   },
    //   "samplers": [ ... ]
    // }
    //
    // Alternative flat format (backward compatible):
    // {
    //   "shader": "ShaderName",
    //   "InputData": { ... },      // Buffer name at top level
    //   "MaterialParams": { ... },
    //   "samplers": [ ... ]
    // }
    //
    // Values are plain JSON types:
    // - Scalars: number, boolean
    // - Vectors: [x, y, z, w]
    // - Matrices: [[row0], [row1], [row2], [row3]]
    // - Arrays: [elem0, elem1, ...]
    // - Nested structures: { field1: value1, field2: value2 }
    //
    // The buffer structure (types, layout) comes from the shader definition.
    // Material only provides field VALUES, not structure.
    MaterialDefinition MaterialFromJson(const json& j);

    // Sampler configuration helpers
    SamplerDescription SamplerConfigFromJson(const json& j);

    // ============================================================================
    // STRING CONVERSIONS
    // ============================================================================

    std::string ColorSpaceToString(ColorSpace cs);
    ColorSpace StringToColorSpace(const std::string& str);

    std::string FilterToString(SamplerDescription::Filter filter);
    SamplerDescription::Filter StringToFilter(const std::string& str);

    std::string AddressModeToString(SamplerDescription::AddressMode mode);
    SamplerDescription::AddressMode StringToAddressMode(const std::string& str);

} // namespace AssetLib

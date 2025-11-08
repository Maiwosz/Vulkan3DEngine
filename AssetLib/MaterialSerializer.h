#pragma once
#include "MaterialTypes.h"
#include "AssetLib.h"
#include <json.hpp>

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
    // JSON CONVERSIONS (for .mat text files)
    // ============================================================================

    // All fields must use explicit type annotation format:
    // {
    //   "fieldName": {
    //     "baseType": "float",
    //     "value": 1.0
    //   }
    // }
    //
    // Supported baseType values:
    // - Scalars: "bool", "float", "int", "uint", "double"
    // - Float vectors: "vec2", "vec3", "vec4"
    // - Integer vectors: "ivec2", "ivec3", "ivec4"
    // - Unsigned vectors: "uvec2", "uvec3", "uvec4"
    // - Double vectors: "dvec2", "dvec3", "dvec4"
    // - Matrices: "mat2", "mat3", "mat4"
    // - Atomic: "atomic_uint"
    //
    // Arrays are specified as:
    // {
    //   "lights": {
    //     "baseType": "vec3",
    //     "value": [[1,0,0], [0,1,0], [0,0,1]]
    //   }
    // }
    //
    // Nested structures are plain objects without baseType/value:
    // {
    //   "material": {
    //     "diffuse": { "baseType": "vec3", "value": [1,1,1] },
    //     "specular": { "baseType": "vec3", "value": [1,1,1] }
    //   }
    // }
    //
    // Buffers are automatically configured:
    // - inputBuffer: Uniform buffer, std140 layout, binding 0
    // - outputBuffer: Storage buffer, std430 layout, WriteOnly, binding 1
    // - inputOutputBuffer: Storage buffer, std430 layout, ReadWrite, binding 2
    // - Samplers: Starting at binding 3
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

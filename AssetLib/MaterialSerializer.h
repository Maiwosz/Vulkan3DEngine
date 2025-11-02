#pragma once
#include "MaterialTypes.h"
#include "AssetLib.h"
#include <json.hpp>

namespace AssetLib {
    using json = nlohmann::json;

    // ============================================================================
    // HIGH-LEVEL SERIALIZATION (Material <-> AssetData)
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

    // Read only dependencies from AssetData (fast, no decompression needed)
    MaterialDependencies ReadMaterialDependencies(const AssetData& asset);

    // ============================================================================
    // INTERMEDIATE FORMAT CONVERSIONS
    // ============================================================================

    // Convert high-level Material to low-level MaterialData
    MaterialData MaterialToData(const MaterialDefinition& material);

    // Convert low-level MaterialData to high-level Material
    MaterialDefinition DataToMaterial(const MaterialData& data);

    // ============================================================================
    // JSON CONVERSIONS
    // ============================================================================

    // Serialize Material to JSON (for .mat files)
    json MaterialToJson(const MaterialDefinition& material);

    // Deserialize Material from JSON
    MaterialDefinition MaterialFromJson(const json& j);

    // Serialize ParameterValue to JSON
    json ParameterValueToJson(const ParameterValue& param);

    // Deserialize ParameterValue from JSON
    ParameterValue ParameterValueFromJson(const json& j);

    // Serialize SamplerDescription to JSON
    json SamplerDescriptionToJson(const SamplerDescription& sampler);

    // Deserialize SamplerDescription from JSON
    SamplerDescription SamplerDescriptionFromJson(const json& j);

    // Serialize MaterialDependencies to JSON
    json MaterialDependenciesToJson(const MaterialDependencies& deps);

    // Deserialize MaterialDependencies from JSON
    MaterialDependencies MaterialDependenciesFromJson(const json& j);

    // ============================================================================
    // BINARY FORMAT HELPERS
    // ============================================================================

    // Serialize MaterialParameter (binary struct) to JSON
    json MaterialParameterToJson(const MaterialParameter& param);

    // Deserialize MaterialParameter from JSON
    MaterialParameter MaterialParameterFromJson(const json& j);

    // Serialize MaterialInfo to JSON
    json MaterialInfoToJson(const MaterialInfo& info);

    // Deserialize MaterialInfo from JSON
    MaterialInfo MaterialInfoFromJson(const json& j);

} // namespace AssetLib
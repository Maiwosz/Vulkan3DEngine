#pragma once
#include <json.hpp>
#include "ShaderLib.h"

namespace ShaderLib {
    using json = nlohmann::json;

    // ============================================================================
    // ENUM SERIALIZATION
    // ============================================================================

    // Stage
    void to_json(json& j, Stage stage);
    void from_json(const json& j, Stage& stage);

    // StageFlags
    void to_json(json& j, StageFlags flags);
    void from_json(const json& j, StageFlags& flags);

    // BaseType
    void to_json(json& j, BaseType type);
    void from_json(const json& j, BaseType& type);

    // DescriptorType
    void to_json(json& j, DescriptorType type);
    void from_json(const json& j, DescriptorType& type);

    // LayoutStandard
    void to_json(json& j, LayoutStandard standard);
    void from_json(const json& j, LayoutStandard& standard);

    // BufferType
    void to_json(json& j, BufferType type);
    void from_json(const json& j, BufferType& type);

    // ============================================================================
    // STRUCTURE SERIALIZATION
    // ============================================================================

    // BufferVariable
    void to_json(json& j, const BufferVariable& var);
    void from_json(const json& j, BufferVariable& var);

    // PushConstantRange
    void to_json(json& j, const PushConstantRange& range);
    void from_json(const json& j, PushConstantRange& range);

    // DescriptorBinding
    void to_json(json& j, const DescriptorBinding& binding);
    void from_json(const json& j, DescriptorBinding& binding);

    // BufferObject
    void to_json(json& j, const BufferObject& buffer);
    void from_json(const json& j, BufferObject& buffer);

    // ShaderMetadata
    void to_json(json& j, const ShaderMetadata& metadata);
    void from_json(const json& j, ShaderMetadata& metadata);

    // ============================================================================
    // HIGH-LEVEL SERIALIZATION FUNCTIONS
    // ============================================================================

    // Serialize metadata to JSON string
    std::string SerializeMetadata(const ShaderMetadata& metadata);

    // Deserialize metadata from JSON string
    ShaderMetadata DeserializeMetadata(const std::string& jsonStr);

    // Serialize compiled stages to binary format
    std::vector<uint8_t> SerializeStages(const std::vector<CompiledStage>& stages);

    // Deserialize compiled stages from binary format
    std::vector<CompiledStage> DeserializeStages(const std::vector<uint8_t>& data);

    // ============================================================================
    // COMPLETE SHADER SERIALIZATION
    // ============================================================================

    // Serialized shader data structure
    struct SerializedShaderData {
        std::string metadataJson;
        std::vector<uint8_t> stagesBinary;
    };

    // Serialize complete ShaderData (metadata + stages)
    SerializedShaderData SerializeShaderData(const ShaderData& shaderData);

    // Deserialize complete ShaderData
    ShaderData DeserializeShaderData(const SerializedShaderData& serialized);

} // namespace ShaderLib
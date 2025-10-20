#pragma once
#include <json.hpp>
#include "ShaderLib.h"

namespace ShaderLib {
    using json = nlohmann::json;

    // Deklaracje dla typów wyliczeniowych
    void to_json(json& j, Stage stage);
    void from_json(const json& j, Stage& stage);

    void to_json(json& j, DescriptorType type);
    void from_json(const json& j, DescriptorType& type);

    void to_json(json& j, UniformType type);
    void from_json(const json& j, UniformType& type);

    // Deklaracje dla flag etapów
    void to_json(json& j, StageFlags flags);
    void from_json(const json& j, StageFlags& flags);

    // Deklaracje dla struktur
    void to_json(json& j, const PushConstantRange& range);
    void from_json(const json& j, PushConstantRange& range);

    void to_json(json& j, const DescriptorBinding& binding);
    void from_json(const json& j, DescriptorBinding& binding);

    void to_json(json& j, const UniformVariable& var);
    void from_json(const json& j, UniformVariable& var);

    void to_json(json& j, BufferType type);
    void from_json(const json& j, BufferType& type);

    void to_json(json& j, LayoutStandard standard);
    void from_json(const json& j, LayoutStandard& standard);

    void to_json(json& j, const BufferObject& buffer);
    void from_json(const json& j, BufferObject& buffer);

    void to_json(json& j, const ShaderMetadata& metadata);
    void from_json(const json& j, ShaderMetadata& metadata);

    // Deklaracje funkcji pomocniczych
    std::string SerializeMetadata(const ShaderMetadata& metadata);
    ShaderMetadata DeserializeMetadata(const std::string& jsonStr);

    // ShaderStages
    std::vector<uint8_t> SerializeStages(const std::vector<CompiledStage>& stages);
    std::vector<CompiledStage> DeserializeStages(const std::vector<uint8_t>& data);
}// namespace ShaderLib


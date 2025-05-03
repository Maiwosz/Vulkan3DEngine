#include "pch.h"
#include "Serialization.h"
#include "TypeConversions.h"


using namespace ShaderLib::TypeConversion;

namespace ShaderLib {
    // Implementacje dla typów wyliczeniowych
    void to_json(json& j, Stage stage) {
        switch (stage) {
        case Stage::Vertex: j = "Vertex"; break;
        case Stage::Fragment: j = "Fragment"; break;
        case Stage::Compute: j = "Compute"; break;
        case Stage::Geometry: j = "Geometry"; break;
        case Stage::TessellationControl: j = "TessellationControl"; break;
        case Stage::TessellationEvaluation: j = "TessellationEvaluation"; break;
        default: j = "Unknown"; break;
        }
    }

    void from_json(const json& j, Stage& stage) {
        std::string s = j.get<std::string>();
        if (s == "Vertex") stage = Stage::Vertex;
        else if (s == "Fragment") stage = Stage::Fragment;
        else if (s == "Compute") stage = Stage::Compute;
        else if (s == "Geometry") stage = Stage::Geometry;
        else if (s == "TessellationControl") stage = Stage::TessellationControl;
        else if (s == "TessellationEvaluation") stage = Stage::TessellationEvaluation;
        else throw std::runtime_error("Unknown Stage: " + s);
    }

    void to_json(json& j, DescriptorType type) {
        switch (type) {
        case DescriptorType::UniformBuffer: j = "UniformBuffer"; break;
        case DescriptorType::StorageBuffer: j = "StorageBuffer"; break;
        case DescriptorType::CombinedImageSampler: j = "CombinedImageSampler"; break;
        case DescriptorType::SeparateImage: j = "SeparateImage"; break;
        case DescriptorType::SeparateSampler: j = "SeparateSampler"; break;
        default: j = "Unknown"; break;
        }
    }

    void from_json(const json& j, DescriptorType& type) {
        std::string s = j.get<std::string>();
        if (s == "UniformBuffer") type = DescriptorType::UniformBuffer;
        else if (s == "StorageBuffer") type = DescriptorType::StorageBuffer;
        else if (s == "CombinedImageSampler") type = DescriptorType::CombinedImageSampler;
        else if (s == "SeparateImage") type = DescriptorType::SeparateImage;
        else if (s == "SeparateSampler") type = DescriptorType::SeparateSampler;
        else throw std::runtime_error("Unknown DescriptorType: " + s);
    }

    void to_json(json& j, UniformType type) {
        j = UniformTypeToString(type);
    }

    void from_json(const json& j, UniformType& type) {
        type = StringToUniformType(j.get<std::string>());
    }

    // Implementacje dla flag etapów
    void to_json(json& j, StageFlags flags) {
        json arr = json::array();
        if (flags & static_cast<StageFlags>(Stage::Vertex))
            arr.push_back("Vertex");
        if (flags & static_cast<StageFlags>(Stage::Fragment))
            arr.push_back("Fragment");
        if (flags & static_cast<StageFlags>(Stage::Compute))
            arr.push_back("Compute");
        if (flags & static_cast<StageFlags>(Stage::Geometry))
            arr.push_back("Geometry");
        if (flags & static_cast<StageFlags>(Stage::TessellationControl))
            arr.push_back("TessellationControl");
        if (flags & static_cast<StageFlags>(Stage::TessellationEvaluation))
            arr.push_back("TessellationEvaluation");
        j = arr;
    }

    void from_json(const json& j, StageFlags& flags) {
        flags = 0;
        for (auto& stage : j) {
            Stage s;
            from_json(stage, s);
            flags |= static_cast<StageFlags>(s);
        }
    }

    // Implementacje dla struktur
    void to_json(json& j, const PushConstantRange& range) {
        j = json{
            {"stages", range.stages},
            {"offset", range.offset},
            {"size", range.size}
        };
    }

    void from_json(const json& j, PushConstantRange& range) {
        j.at("stages").get_to(range.stages);
        j.at("offset").get_to(range.offset);
        j.at("size").get_to(range.size);
    }

    void to_json(json& j, const DescriptorBinding& binding) {
        j = json{
            {"set", binding.set},
            {"binding", binding.binding},
            {"type", binding.type},
            {"stages", binding.stages},
            {"name", binding.name}
        };
    }

    void from_json(const json& j, DescriptorBinding& binding) {
        j.at("set").get_to(binding.set);
        j.at("binding").get_to(binding.binding);
        j.at("type").get_to(binding.type);
        j.at("stages").get_to(binding.stages);
        j.at("name").get_to(binding.name);
    }

    void to_json(json& j, const UniformVariable& var) {
        j = json{
            {"name", var.name},
            {"type", var.type},
            {"size", var.size},
            {"offset", var.offset},
            {"arraySize", var.arraySize},
            {"typeName", var.typeName}
        };
    }

    void from_json(const json& j, UniformVariable& var) {
        j.at("name").get_to(var.name);
        j.at("type").get_to(var.type);
        j.at("size").get_to(var.size);
        j.at("offset").get_to(var.offset);
        j.at("arraySize").get_to(var.arraySize);
        j.at("typeName").get_to(var.typeName);
    }

    void to_json(json& j, const UniformBufferObject& ubo) {
        j = json{
            {"name", ubo.name},
            {"set", ubo.set},
            {"binding", ubo.binding},
            {"size", ubo.size},
            {"variables", ubo.variables}
        };
    }

    void from_json(const json& j, UniformBufferObject& ubo) {
        j.at("name").get_to(ubo.name);
        j.at("set").get_to(ubo.set);
        j.at("binding").get_to(ubo.binding);
        j.at("size").get_to(ubo.size);
        j.at("variables").get_to(ubo.variables);
    }

    void to_json(json& j, const ShaderMetadata& metadata) {
        j = json{
            {"availableStages", metadata.availableStages},
            {"usesGlobalUBO", metadata.usesGlobalUBO},
            {"usesObjectUBO", metadata.usesObjectUBO},
            {"pushConstants", metadata.pushConstants},
            {"descriptors", metadata.descriptors},
            {"customUBOs", metadata.customUBOs},
            {"globalUBO", metadata.globalUBO},
            {"objectUBO", metadata.objectUBO}
        };
    }

    void from_json(const json& j, ShaderMetadata& metadata) {
        j.at("availableStages").get_to(metadata.availableStages);
        j.at("usesGlobalUBO").get_to(metadata.usesGlobalUBO);
        j.at("usesObjectUBO").get_to(metadata.usesObjectUBO);
        j.at("pushConstants").get_to(metadata.pushConstants);
        j.at("descriptors").get_to(metadata.descriptors);
        j.at("customUBOs").get_to(metadata.customUBOs);
        j.at("globalUBO").get_to(metadata.globalUBO);
        j.at("objectUBO").get_to(metadata.objectUBO);
    }

    // Implementacje funkcji pomocniczych
    std::string SerializeMetadata(const ShaderMetadata& metadata) {
        json j = metadata;
        return j.dump(4);
    }

    ShaderMetadata DeserializeMetadata(const std::string& jsonStr) {
        try {
            json j = json::parse(jsonStr);
            return j.get<ShaderMetadata>();
        }
        catch (const std::exception& e) {
            throw std::runtime_error("Deserialization error: " + std::string(e.what()));
        }
    }

    namespace { // Pomocnicze funkcje wewnętrzne
        template<typename T>
        void AppendBytes(std::vector<uint8_t>& vec, const T& value) {
            const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value);
            vec.insert(vec.end(), bytes, bytes + sizeof(T));
        }

        template<typename T>
        T ReadBytes(const std::vector<uint8_t>& data, size_t& offset) {
            if (offset + sizeof(T) > data.size()) {
                throw std::runtime_error("Deserialization error: unexpected end of data");
            }
            T value;
            std::memcpy(&value, data.data() + offset, sizeof(T));
            offset += sizeof(T);
            return value;
        }
    }

    std::vector<uint8_t> SerializeStages(const std::vector<CompiledStage>& stages) {
        std::vector<uint8_t> result;

        // Zapis liczby etapów
        AppendBytes(result, static_cast<uint64_t>(stages.size()));

        for (const auto& stage : stages) {
            // Zapis rozmiaru spirv
            const uint64_t spirvSize = stage.spirv.size();
            AppendBytes(result, spirvSize);

            // Zapis danych spirv
            const size_t spirvBytes = spirvSize * sizeof(uint32_t);
            const uint8_t* spirvData = reinterpret_cast<const uint8_t*>(stage.spirv.data());
            result.insert(result.end(), spirvData, spirvData + spirvBytes);

            // Zapis typu etapu
            AppendBytes(result, static_cast<uint32_t>(stage.stage));
        }

        return result;
    }

    std::vector<CompiledStage> DeserializeStages(const std::vector<uint8_t>& data) {
        std::vector<CompiledStage> stages;
        size_t offset = 0;

        // Odczyt liczby etapów
        const uint64_t numStages = ReadBytes<uint64_t>(data, offset);

        for (uint64_t i = 0; i < numStages; ++i) {
            // Odczyt rozmiaru spirv
            const uint64_t spirvSize = ReadBytes<uint64_t>(data, offset);

            // Sprawdzenie dostępnych danych
            const size_t spirvBytes = spirvSize * sizeof(uint32_t);
            if (offset + spirvBytes > data.size()) {
                throw std::runtime_error("Deserialization error: corrupted spirv data");
            }

            // Kopiowanie danych spirv
            std::vector<uint32_t> spirv(spirvSize);
            std::memcpy(spirv.data(), data.data() + offset, spirvBytes);
            offset += spirvBytes;

            // Odczyt typu etapu
            const auto stage = static_cast<Stage>(ReadBytes<uint32_t>(data, offset));

            stages.push_back({ std::move(spirv), stage });
        }

        return stages;
    }

} // namespace ShaderLib
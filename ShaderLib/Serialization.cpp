#include "pch.h"
#include "Serialization.h"
#include "ShaderStruct.h"
#include "ShaderArray.h"

namespace ShaderLib {

    // ============================================================================
    // STAGE SERIALIZATION
    // ============================================================================

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

    // ============================================================================
    // STAGE FLAGS SERIALIZATION
    // ============================================================================

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
        for (const auto& stage : j) {
            Stage s;
            from_json(stage, s);
            flags |= static_cast<StageFlags>(s);
        }
    }

    // ============================================================================
    // BASE TYPE SERIALIZATION
    // ============================================================================

    void to_json(json& j, BaseType type) {
        j = BaseTypeToString(type);
    }

    void from_json(const json& j, BaseType& type) {
        type = StringToBaseType(j.get<std::string>());
    }

    // ============================================================================
    // DESCRIPTOR TYPE SERIALIZATION
    // ============================================================================

    void to_json(json& j, DescriptorType type) {
        j = DescriptorTypeToString(type);
    }

    void from_json(const json& j, DescriptorType& type) {
        type = StringToDescriptorType(j.get<std::string>());
    }

    // ============================================================================
    // LAYOUT STANDARD SERIALIZATION
    // ============================================================================

    void to_json(json& j, LayoutStandard standard) {
        switch (standard) {
        case LayoutStandard::Std140: j = "Std140"; break;
        case LayoutStandard::Std430: j = "Std430"; break;
        case LayoutStandard::Packed: j = "Packed"; break;
        default: j = "Unknown"; break;
        }
    }

    void from_json(const json& j, LayoutStandard& standard) {
        std::string s = j.get<std::string>();
        if (s == "Std140") standard = LayoutStandard::Std140;
        else if (s == "Std430") standard = LayoutStandard::Std430;
        else if (s == "Packed") standard = LayoutStandard::Packed;
        else throw std::runtime_error("Unknown LayoutStandard: " + s);
    }

    // ============================================================================
    // BUFFER TYPE SERIALIZATION
    // ============================================================================

    void to_json(json& j, BufferType type) {
        switch (type) {
        case BufferType::Uniform: j = "Uniform"; break;
        case BufferType::Storage: j = "Storage"; break;
        default: j = "Unknown"; break;
        }
    }

    void from_json(const json& j, BufferType& type) {
        std::string s = j.get<std::string>();
        if (s == "Uniform") type = BufferType::Uniform;
        else if (s == "Storage") type = BufferType::Storage;
        else throw std::runtime_error("Unknown BufferType: " + s);
    }

    // ============================================================================
    // BUFFER ACCESS MODE SERIALIZATION
    // ============================================================================

    void to_json(json& j, BufferAccessMode mode) {
        switch (mode) {
        case BufferAccessMode::ReadOnly: j = "ReadOnly"; break;
        case BufferAccessMode::WriteOnly: j = "WriteOnly"; break;
        case BufferAccessMode::ReadWrite: j = "ReadWrite"; break;
        default: j = "Unknown"; break;
        }
    }

    void from_json(const json& j, BufferAccessMode& mode) {
        std::string s = j.get<std::string>();
        if (s == "ReadOnly") mode = BufferAccessMode::ReadOnly;
        else if (s == "WriteOnly") mode = BufferAccessMode::WriteOnly;
        else if (s == "ReadWrite") mode = BufferAccessMode::ReadWrite;
        else throw std::runtime_error("Unknown BufferAccessMode: " + s);
    }

    // ============================================================================
    // BUFFER VARIABLE SERIALIZATION
    // ============================================================================

    void to_json(json& j, const BufferVariable& var) {
        j = json{
            {"name", var.name},
            {"baseType", var.baseType},
            {"size", var.size},
            {"offset", var.offset},
            {"accessMode", var.accessMode}
        };

        // Serialize composite structure if present
        if (var.composite) {
            j["composite"] = var.composite->ToJson();
        }
    }

    void from_json(const json& j, BufferVariable& var) {
        j.at("name").get_to(var.name);
        j.at("baseType").get_to(var.baseType);
        j.at("size").get_to(var.size);
        j.at("offset").get_to(var.offset);
        j.at("accessMode").get_to(var.accessMode);

        // Deserialize composite structure if present
        if (j.contains("composite") && !j["composite"].is_null()) {
            // Use static method from base class - no need to include specific types!
            var.composite = CompositeTypeDefinition::FromJson(j["composite"]);
        }
        else {
            var.composite = nullptr;
        }
    }

    // ============================================================================
    // PUSH CONSTANT RANGE SERIALIZATION
    // ============================================================================

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

    // ============================================================================
    // DESCRIPTOR SLOT SERIALIZATION
    // ============================================================================

    void to_json(json& j, const DescriptorSlot& slot) {
        j = json{
            {"binding", slot.binding},
            {"type", slot.type},
            {"stages", slot.stages},
            {"name", slot.name}
        };
    }

    void from_json(const json& j, DescriptorSlot& slot) {
        j.at("binding").get_to(slot.binding);
        j.at("type").get_to(slot.type);
        j.at("stages").get_to(slot.stages);
        j.at("name").get_to(slot.name);
    }

    // ============================================================================
    // DESCRIPTOR SET SERIALIZATION
    // ============================================================================

    void to_json(json& j, const DescriptorSet& set) {
        j = json{
            {"setNumber", set.setNumber},
            {"slots", set.slots},
            {"buffers", set.buffers}
        };
    }

    void from_json(const json& j, DescriptorSet& set) {
        j.at("setNumber").get_to(set.setNumber);
        j.at("slots").get_to(set.slots);
        j.at("buffers").get_to(set.buffers);
    }

    // ============================================================================
    // BUFFER OBJECT SERIALIZATION
    // ============================================================================

    void to_json(json& j, const BufferObject& buffer) {
        j = json{
            {"name", buffer.name},
            {"size", buffer.size},
            {"bufferType", buffer.bufferType},
            {"layoutStandard", buffer.layoutStandard},
            {"accessMode", buffer.accessMode},
            {"variables", buffer.variables}
        };
    }

    void from_json(const json& j, BufferObject& buffer) {
        j.at("name").get_to(buffer.name);
        j.at("size").get_to(buffer.size);
        j.at("bufferType").get_to(buffer.bufferType);
        j.at("layoutStandard").get_to(buffer.layoutStandard);
        j.at("accessMode").get_to(buffer.accessMode);
        j.at("variables").get_to(buffer.variables);
    }

    // ============================================================================
    // SHADER COMPUTE INFO SERIALIZATION
    // ============================================================================

    void to_json(json& j, const ComputeShaderInfo& info) {
        j = json{
            {"localSizeX", info.localSizeX},
            {"localSizeY", info.localSizeY},
            {"localSizeZ", info.localSizeZ},
            {"usesSharedMemory", info.usesSharedMemory},
            {"usesAtomics", info.usesAtomics},
            {"usesBarriers", info.usesBarriers},
            {"sharedMemorySize", info.sharedMemorySize}
        };
    }

    void from_json(const json& j, ComputeShaderInfo& info) {
        j.at("localSizeX").get_to(info.localSizeX);
        j.at("localSizeY").get_to(info.localSizeY);
        j.at("localSizeZ").get_to(info.localSizeZ);
        j.at("usesSharedMemory").get_to(info.usesSharedMemory);
        j.at("usesAtomics").get_to(info.usesAtomics);
        j.at("usesBarriers").get_to(info.usesBarriers);
        j.at("sharedMemorySize").get_to(info.sharedMemorySize);
    }

    // ============================================================================
    // SHADER METADATA SERIALIZATION
    // ============================================================================

    void to_json(json& j, const ShaderMetadata& metadata) {
        j = json{
            {"availableStages", metadata.availableStages},
            {"usesGlobalUBO", metadata.usesGlobalUBO},
            {"usesObjectUBO", metadata.usesObjectUBO},
            {"pushConstants", metadata.pushConstants},
            {"descriptorSets", metadata.descriptorSets},
            {"customBuffers", metadata.customBuffers}
        };

        // Only serialize globalUBO if it's actually used
        if (metadata.usesGlobalUBO) {
            j["globalUBO"] = metadata.globalUBO;
        }

        // Only serialize objectUBO if it's actually used
        if (metadata.usesObjectUBO) {
            j["objectUBO"] = metadata.objectUBO;
        }

        if (metadata.computeInfo) {
            j["computeInfo"] = *metadata.computeInfo;
        }
    }

    void from_json(const json& j, ShaderMetadata& metadata) {
        j.at("availableStages").get_to(metadata.availableStages);
        j.at("usesGlobalUBO").get_to(metadata.usesGlobalUBO);
        j.at("usesObjectUBO").get_to(metadata.usesObjectUBO);
        j.at("pushConstants").get_to(metadata.pushConstants);
        j.at("descriptorSets").get_to(metadata.descriptorSets);
        j.at("customBuffers").get_to(metadata.customBuffers);

        // Only deserialize globalUBO if it's actually used
        if (metadata.usesGlobalUBO && j.contains("globalUBO")) {
            j.at("globalUBO").get_to(metadata.globalUBO);
        }

        // Only deserialize objectUBO if it's actually used
        if (metadata.usesObjectUBO && j.contains("objectUBO")) {
            j.at("objectUBO").get_to(metadata.objectUBO);
        }

        if (j.contains("computeInfo")) {
            metadata.computeInfo = j["computeInfo"].get<ComputeShaderInfo>();
        }
    }

    // ============================================================================
    // HIGH-LEVEL SERIALIZATION FUNCTIONS
    // ============================================================================

    std::string SerializeMetadata(const ShaderMetadata& metadata) {
        json j = metadata;
        return j.dump(4); // Pretty print with 4-space indent
    }

    ShaderMetadata DeserializeMetadata(const std::string& jsonStr) {
        try {
            json j = json::parse(jsonStr);
            return j.get<ShaderMetadata>();
        }
        catch (const std::exception& e) {
            throw std::runtime_error("Metadata deserialization error: " + std::string(e.what()));
        }
    }

    // ============================================================================
    // BINARY STAGE SERIALIZATION
    // ============================================================================

    namespace {
        template<typename T>
        void AppendBytes(std::vector<uint8_t>& vec, const T& value) {
            const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value);
            vec.insert(vec.end(), bytes, bytes + sizeof(T));
        }

        template<typename T>
        T ReadBytes(const std::vector<uint8_t>& data, size_t& offset) {
            if (offset + sizeof(T) > data.size()) {
                throw std::runtime_error("Stage deserialization error: unexpected end of data");
            }
            T value;
            std::memcpy(&value, data.data() + offset, sizeof(T));
            offset += sizeof(T);
            return value;
        }
    }

    std::vector<uint8_t> SerializeStages(const std::vector<CompiledStage>& stages) {
        std::vector<uint8_t> result;

        // Write number of stages
        AppendBytes(result, static_cast<uint64_t>(stages.size()));

        for (const auto& stage : stages) {
            // Write stage type
            AppendBytes(result, static_cast<uint32_t>(stage.stage));

            // Write SPIR-V size
            const uint64_t spirvSize = stage.spirv.size();
            AppendBytes(result, spirvSize);

            // Write SPIR-V data
            const size_t spirvBytes = spirvSize * sizeof(uint32_t);
            const uint8_t* spirvData = reinterpret_cast<const uint8_t*>(stage.spirv.data());
            result.insert(result.end(), spirvData, spirvData + spirvBytes);
        }

        return result;
    }

    std::vector<CompiledStage> DeserializeStages(const std::vector<uint8_t>& data) {
        std::vector<CompiledStage> stages;
        size_t offset = 0;

        // Read number of stages
        const uint64_t numStages = ReadBytes<uint64_t>(data, offset);

        stages.reserve(numStages);
        for (uint64_t i = 0; i < numStages; ++i) {
            // Read stage type
            const auto stage = static_cast<Stage>(ReadBytes<uint32_t>(data, offset));

            // Read SPIR-V size
            const uint64_t spirvSize = ReadBytes<uint64_t>(data, offset);

            // Validate available data
            const size_t spirvBytes = spirvSize * sizeof(uint32_t);
            if (offset + spirvBytes > data.size()) {
                throw std::runtime_error("Stage deserialization error: corrupted SPIR-V data");
            }

            // Copy SPIR-V data
            std::vector<uint32_t> spirv(spirvSize);
            std::memcpy(spirv.data(), data.data() + offset, spirvBytes);
            offset += spirvBytes;

            stages.push_back({ std::move(spirv), stage });
        }

        return stages;
    }

    // ============================================================================
    // COMPLETE SHADER DATA SERIALIZATION
    // ============================================================================

    SerializedShaderData SerializeShaderData(const ShaderData& shaderData) {
        SerializedShaderData result;

        // Serialize metadata (assumes metadata is already JSON)
        result.metadataJson = shaderData.metadata.dump(4);

        // Serialize stages to binary
        result.stagesBinary = SerializeStages(shaderData.stages);

        return result;
    }

    ShaderData DeserializeShaderData(const SerializedShaderData& serialized) {
        ShaderData result;

        try {
            // Deserialize metadata
            result.metadata = nlohmann::json::parse(serialized.metadataJson);

            // Deserialize stages
            result.stages = DeserializeStages(serialized.stagesBinary);

            return result;
        }
        catch (const std::exception& e) {
            throw std::runtime_error("ShaderData deserialization error: " + std::string(e.what()));
        }
    }

} // namespace ShaderLib
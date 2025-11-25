#pragma once
#include "ShaderLib.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <json.hpp>

namespace AssetLib {

    using json = nlohmann::json;

    // ============================================================================
    // SAMPLER CONFIGURATION
    // ============================================================================

    enum class ColorSpace : uint8_t {
        Linear = 0,
        SRGB = 1,
        HDR = 2
    };

    struct SamplerDescription {
        enum class Filter : uint8_t { Nearest = 0, Linear = 1 };
        enum class AddressMode : uint8_t {
            Repeat = 0,
            MirroredRepeat = 1,
            ClampToEdge = 2,
            ClampToBorder = 3
        };

        std::string name;                           // Sampler name in shader
        ShaderLib::DescriptorType descriptorType;   // Texture type

        std::string texturePath;                    // Path to texture asset
        ColorSpace colorSpace = ColorSpace::Linear;

        Filter magFilter = Filter::Linear;
        Filter minFilter = Filter::Linear;
        AddressMode addressModeU = AddressMode::Repeat;
        AddressMode addressModeV = AddressMode::Repeat;
        AddressMode addressModeW = AddressMode::Repeat;
        float anisotropy = 1.0f;
        float minLod = 0.0f;
        float maxLod = 1000.0f;

        bool operator==(const SamplerDescription&) const = default;
    };

    // ============================================================================
    // MATERIAL DEFINITION (IN-MEMORY, FROM ASSET)
    // ============================================================================

    /**
     * MaterialDefinition - In-memory representation of a material asset
     *
     * KEY DESIGN PRINCIPLE:
     * - Material assets store ONLY field VALUES (JSON)
     * - Material assets do NOT store buffer structure/types
     * - Buffer structure always comes from shader definition
     *
     * This ensures:
     * - Shader is always source of truth for structure
     * - Materials are backwards compatible when shaders evolve
     * - No need for validation or synchronization
     *
     * Example:
     * Shader defines:
     *   layout(set = 2, binding = 0) uniform MaterialParams {
     *       vec3 color;
     *       float roughness;
     *   };
     *
     * Material asset stores:
     *   "MaterialParams": {
     *     "color": [1.0, 0.0, 0.0],
     *     "roughness": 0.5
     *   }
     *
     * At runtime:
     * 1. Get buffer definition from shader: GetInputDataBuffer()
     * 2. Create instance: definition->CreateInstance()
     * 3. Fill values: CreateBufferInstanceFromMaterial(definition, materialValues)
     */
    struct MaterialDefinition {
        std::string shaderName;

        // Buffer field values from .mat file
        // Key: buffer name (e.g. "InputData", "MaterialParams", etc.)
        // Value: JSON object with field values (NOT structure, just values!)
        // 
        // These are just VALUES - the structure comes from the shader.
        std::unordered_map<std::string, json> buffers;

        // Samplers
        std::vector<SamplerDescription> samplers;

        // ---- Helper Methods ----

        // Validation
        bool Validate() const;

        // Find sampler by name
        const SamplerDescription* FindSampler(const std::string& name) const;
        SamplerDescription* FindSampler(const std::string& name);

        // Get all texture dependencies
        std::vector<std::string> GetTextureDependencies() const;

        // Buffer operations
        bool HasBuffer(const std::string& name) const {
            return buffers.find(name) != buffers.end();
        }

        const json* GetBufferValues(const std::string& name) const {
            auto it = buffers.find(name);
            return it != buffers.end() ? &it->second : nullptr;
        }

        std::vector<std::string> GetBufferNames() const {
            std::vector<std::string> names;
            names.reserve(buffers.size());
            for (const auto& [name, _] : buffers) {
                names.push_back(name);
            }
            return names;
        }
    };

    // ============================================================================
    // BINARY FORMAT (Low-level, for file storage)
    // ============================================================================

#pragma pack(push, 1)

    struct MaterialHeader {
        std::array<char, 64> shaderName;
        uint32_t bufferCount;           // Number of buffers
        uint32_t samplerCount;
        uint32_t totalBufferDataSize;   // Total size of all buffer JSONs
    };

    struct BinaryBufferEntry {
        std::array<char, 64> bufferName;
        uint32_t dataSize;              // Size of JSON data for this buffer
    };

    struct BinarySamplerConfig {
        std::array<char, 64> name;
        ShaderLib::DescriptorType descriptorType;
        std::array<char, 256> texturePath;
        ColorSpace colorSpace;
        SamplerDescription::Filter magFilter;
        SamplerDescription::Filter minFilter;
        SamplerDescription::AddressMode addressModeU;
        SamplerDescription::AddressMode addressModeV;
        SamplerDescription::AddressMode addressModeW;
        float anisotropy;
        float minLod;
        float maxLod;
    };

#pragma pack(pop)

    // Binary layout:
    // [MaterialHeader]
    // [BinaryBufferEntry] * bufferCount
    // [Buffer Field Values JSON] * bufferCount (concatenated, sizes in entries)
    // [BinarySamplerConfig] * samplerCount

} // namespace AssetLib

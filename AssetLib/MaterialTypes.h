#pragma once
#include "ShaderLib.h"
#include "BufferObjectDefinition.h"
#include "BufferObjectInstance.h"
#include <string>
#include <vector>
#include <optional>
#include <memory>

namespace AssetLib {

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
        uint32_t binding;                           // Binding number

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
    // MATERIAL DEFINITION (High-level, maps to CustomDescriptorSet)
    // ============================================================================

    struct MaterialDefinition {
        std::string shaderName;

        // Buffers at specific bindings
        std::shared_ptr<ShaderLib::BufferObjectInstance> inputBuffer;      // binding 0
        std::shared_ptr<ShaderLib::BufferObjectInstance> outputBuffer;     // binding 1
        std::shared_ptr<ShaderLib::BufferObjectInstance> inputOutputBuffer; // binding 2

        // Samplers starting at binding 3
        std::vector<SamplerDescription> samplers;

        // ---- Helper Methods ----

        // Validation
        bool Validate() const;

        // Find sampler by name
        const SamplerDescription* FindSampler(const std::string& name) const;
        SamplerDescription* FindSampler(const std::string& name);

        // Get all texture dependencies
        std::vector<std::string> GetTextureDependencies() const;

        // Check which buffers are used
        bool HasInputBuffer() const { return inputBuffer != nullptr; }
        bool HasOutputBuffer() const { return outputBuffer != nullptr; }
        bool HasInputOutputBuffer() const { return inputOutputBuffer != nullptr; }

        // Ensure samplers have correct bindings
        void NormalizeSamplerBindings();
    };

    // ============================================================================
    // BINARY FORMAT (Low-level, for file storage)
    // ============================================================================

#pragma pack(push, 1)

    struct MaterialHeader {
        std::array<char, 64> shaderName;
        uint32_t inputBufferSize;       // 0 if not present
        uint32_t outputBufferSize;      // 0 if not present
        uint32_t inputOutputBufferSize; // 0 if not present
        uint32_t samplerCount;
        uint32_t totalDataSize;
    };

    struct BinarySamplerConfig {
        std::array<char, 64> name;
        ShaderLib::DescriptorType descriptorType;
        uint32_t binding;
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
    // [Buffer Definition JSON for Input] (if inputBufferSize > 0)
    // [Buffer Definition JSON for Output] (if outputBufferSize > 0)
    // [Buffer Definition JSON for InputOutput] (if inputOutputBufferSize > 0)
    // [BinarySamplerConfig] * samplerCount
    // [Input Buffer Data] (if present)
    // [Output Buffer Data] (if present)
    // [InputOutput Buffer Data] (if present)

} // namespace AssetLib
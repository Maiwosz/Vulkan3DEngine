#pragma once
#include <cstdint>
#include <vector>
#include <array>
#include <string_view>
#include <json.hpp>

namespace AssetLib {

#pragma pack(push, 1)
    // =============================================
    // Podstawowe typy enum
    // =============================================
    enum class AssetType : uint8_t {
        Texture = 0,
        Mesh = 1,
        Material = 2,
        Shader = 3
    };

    enum class CompressionType : uint8_t {
        None = 0,
        LZ4 = 1
    };

    // =============================================
    // Textures
    // =============================================
    enum class TextureFormat : uint8_t {
        RGBA8 = 0,
        BC7 = 1,
        BC1 = 2,
        BC3 = 3,
        BC5 = 4
    };

    struct TextureInfo {
        TextureFormat format;
        uint32_t width;
        uint32_t height;
        uint32_t mipLevels;
        uint32_t flags;
    };

    // =============================================
    // Mesh
    // =============================================
    enum class VertexAttribute : uint32_t {
        Position = 0x01,
        Normal = 0x02,
        TexCoord = 0x04,
        Tangent = 0x08,
        Color = 0x10
    };

    inline VertexAttribute operator|(VertexAttribute a, VertexAttribute b) {
        return static_cast<VertexAttribute>(
            static_cast<uint32_t>(a) | static_cast<uint32_t>(b)
            );
    }

    inline VertexAttribute& operator|=(VertexAttribute& a, VertexAttribute b) {
        a = a | b;
        return a;
    }

    struct MeshInfo {
        uint32_t vertexCount;
        uint32_t indexCount;
        uint32_t vertexStride;
        uint32_t attributes;
        uint8_t indexType; // 0 = uint16, 1 = uint32
    };

    // =============================================
    // Shaders
    // =============================================
    enum class ShaderStage : uint8_t {
        Vertex = 0,
        Fragment = 1,
        Compute = 2,
        Geometry = 3,
        TessellationControl = 4,
        TessellationEvaluation = 5
    };

    enum class ShaderStageFlags : uint8_t {
        Vertex = 0x01,
        Fragment = 0x02,
        Compute = 0x04,
        Geometry = 0x08,
        TessellationControl = 0x10,
        TessellationEvaluation = 0x20
    };

    enum class DescriptorType : uint8_t {
        UniformBuffer = 0,
        CombinedImageSampler = 1,
        StorageBuffer = 2,
        StorageImage = 3,
        InputAttachment = 4
    };

    struct DescriptorBinding {
        uint32_t set;
        uint32_t binding;
        DescriptorType type;
        ShaderStageFlags stageFlags;
        uint32_t size; // Dla buforów
        std::array<char, 32> name;
    };

    struct PushConstantRange {
        ShaderStageFlags stageFlags;
        uint32_t offset;
        uint32_t size;
    };

    struct ShaderReflection {
        uint32_t descriptorSetCount;
        uint32_t pushConstantRangeCount;
        uint32_t descriptorBindingsCount;
        // Po tej strukturze następują:
        // DescriptorBinding[descriptorBindingsCount]
        // PushConstantRange[pushConstantRangeCount]
    };

    struct ShaderStageInfo {
        ShaderStage stage;
        uint32_t spirvVersion;
        std::vector<uint8_t> spirvCode;
        std::vector<uint8_t> reflectionData;
    };

    // =============================================
    // Materials
    // =============================================
    enum class SamplerFilter : uint8_t {
        Nearest = 0,
        Linear = 1
    };

    enum class SamplerAddressMode : uint8_t {
        Repeat = 0,
        MirroredRepeat = 1,
        ClampToEdge = 2,
        ClampToBorder = 3
    };

    struct SamplerDescription {
        SamplerFilter magFilter;
        SamplerFilter minFilter;
        SamplerAddressMode addressModeU;
        SamplerAddressMode addressModeV;
        SamplerAddressMode addressModeW;
        float anisotropy;
        float minLod;
        float maxLod;
    };

    struct MaterialParameter {
        std::array<char, 32> name;
        enum class Type : uint8_t {
            Float,
            Float2,
            Float3,
            Float4,
            Int,
            UInt,
            Bool,
            Texture
        } type;
        uint32_t arraySize;
        SamplerDescription samplerDesc;
        uint32_t dataOffset;
    };

    struct MaterialInfo {
        std::array<char, 32> shaderName;
        uint32_t parameterCount;
        uint32_t dataSize;
        // Po tej strukturze następują:
        // MaterialParameter[parameterCount]
        // uint8_t[dataSize] (dane parametrów)
    };

    // =============================================
    // Wspólna struktura nagłówka
    // =============================================
    struct Header {
        std::array<char, 4> magic{ 'A', 'S', 'E', 'T' };
        uint32_t version = 1;
        AssetType assetType;
        CompressionType compression;
        uint64_t decompressedSize;
        uint32_t metadataSize;
        uint64_t dataSize;
    };

    struct AssetData {
        Header header;
        nlohmann::json metadata;
        std::vector<uint8_t> compressedData;
    };

#pragma pack(pop)

    // =============================================
    // Funkcje dla tekstur
    // =============================================
    AssetData WriteTexture(const std::string& source, const TextureInfo& info, const std::vector<uint8_t>& pixelData, CompressionType compression = CompressionType::LZ4, int compressionLevel = 1);
    std::pair<TextureInfo, std::vector<uint8_t>> ReadTexture(const AssetData asset);

    // =============================================
    // Funkcje dla meshów
    // =============================================
    AssetData WriteMesh(const std::string& source, const MeshInfo& info, const std::vector<uint8_t>& vertexData, const std::vector<uint8_t>& indexData, CompressionType compression = CompressionType::LZ4, int compressionLevel = 1);
    std::tuple<MeshInfo, std::vector<uint8_t>, std::vector<uint8_t>> ReadMesh(const AssetData asset);

    // =============================================
    // Funkcje dla materiałów
    // =============================================
    AssetData WriteMaterial(const std::string& source, const MaterialInfo& info, const std::vector<MaterialParameter>& parameters, const std::vector<uint8_t>& parameterData, CompressionType compression = CompressionType::LZ4, int compressionLevel = 1);
    std::tuple<MaterialInfo, std::vector<MaterialParameter>, std::vector<uint8_t>> ReadMaterial(const AssetData asset);

    // =============================================
    // Funkcje dla shaderów
    // =============================================
    AssetData WriteShader(const std::string& source, const std::vector<ShaderStageInfo>& stages, uint32_t flags, CompressionType compression = CompressionType::LZ4, int compressionLevel = 1);
    std::tuple<std::vector<ShaderStageInfo>, std::string, uint32_t> ReadShader(const AssetData& asset);

    // =============================================
    // Funkcje
    // =============================================
    std::vector<uint8_t> Compress(const void* data, size_t size, CompressionType compressionType, int compressionLevel = 1);
    std::vector<uint8_t> Decompress(const void* data, size_t size, size_t decompressedSize);
    bool ValidateHeader(const Header& header);
    void WriteAsset(const std::string& path, const AssetData& assetData);
    AssetData ReadAsset(const std::string& path);

    // =============================================
    // Rozszerzenia plików dla typów zasobów
    // =============================================
    namespace Utilities {
        constexpr std::string_view GetAssetExtension(AssetType type) {
            switch (type) {
            case AssetType::Texture:   return ".atex";
            case AssetType::Mesh:      return ".amsh";
            case AssetType::Material:  return ".amat";
            case AssetType::Shader:    return ".ashd";
            default:                   return "";
            }
        }
    } // namespace Utilities

} // namespace AssetLib
#pragma once
#include <cstdint>
#include <vector>
#include <array>
#include <string_view>
#include <json.hpp>
#include "ShaderLib.h"

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
        BC7 = 1
    };

    // Struktura opisująca pojedynczy poziom mipmapy
    struct MipLevel {
        uint32_t width;
        uint32_t height;
        uint32_t dataOffset;  // Offset w bajtach od początku bufora danych
        uint32_t dataSize;    // Rozmiar poziomu mipmapy w bajtach
    };

    struct TextureInfo {
        TextureFormat format;
        uint32_t width;       // Szerokość podstawowego poziomu (level 0)
        uint32_t height;      // Wysokość podstawowego poziomu (level 0)
        uint32_t mipLevels;   // Liczba poziomów mipmap
        uint32_t flags;
        std::vector<MipLevel> mips;  // Informacje o każdym poziomie mipmapy
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
    // Materials
    // =============================================

    // Using ShaderLib's descriptor types for consistency
    using DescriptorType = ShaderLib::DescriptorType;
    using UniformType = ShaderLib::UniformType;

    // Sampler settings for texture parameters
    struct SamplerDescription {
        enum class Filter : uint8_t {
            Nearest = 0,
            Linear = 1
        };

        enum class AddressMode : uint8_t {
            Repeat = 0,
            MirroredRepeat = 1,
            ClampToEdge = 2,
            ClampToBorder = 3
        };

        Filter magFilter;
        Filter minFilter;
        AddressMode addressModeU;
        AddressMode addressModeV;
        AddressMode addressModeW;
        float anisotropy;
        float minLod;
        float maxLod;
    };

    // Parameter for material (corresponds to CustomDescriptorSet bindings in shader)
    struct MaterialParameter {
        std::array<char, 32> name;       // Name of the parameter
        DescriptorType descriptorType;   // Type of descriptor (UniformBuffer, CombinedImageSampler, etc.)
        UniformType uniformType;         // For UBO variables - the uniform type
        uint32_t arraySize;              // Size if it's an array, 0 otherwise
        SamplerDescription samplerDesc;  // Sampler settings for textures
        uint32_t dataOffset;             // Offset into the parameter data
        uint32_t dataSize;               // Size of this parameter's data
    };

    struct MaterialInfo {
        std::array<char, 32> shaderName; // Name of the shader asset this material uses
        uint32_t parameterCount;         // Number of parameters
        uint32_t dataSize;               // Total size of parameter data
        // Following this structure:
        // MaterialParameter[parameterCount]
        // uint8_t[dataSize] (parameter data)
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
    std::pair<TextureInfo, std::vector<uint8_t>> ReadTexture(const AssetData& asset);

    // =============================================
    // Funkcje dla meshów
    // =============================================
    AssetData WriteMesh(const std::string& source, const MeshInfo& info, const std::vector<uint8_t>& vertexData, const std::vector<uint8_t>& indexData, CompressionType compression = CompressionType::LZ4, int compressionLevel = 1);
    std::tuple<MeshInfo, std::vector<uint8_t>, std::vector<uint8_t>> ReadMesh(const AssetData& asset);

    // =============================================
    // Funkcje dla materiałów
    // =============================================
    AssetData WriteMaterial(
        const std::string& source,
        const MaterialInfo& info,
        const std::vector<MaterialParameter>& parameters,
        const std::vector<uint8_t>& parameterData,
        CompressionType compression = CompressionType::LZ4,
        int compressionLevel = 1
    );

    std::tuple<MaterialInfo, std::vector<MaterialParameter>, std::vector<uint8_t>> ReadMaterial(const AssetData& asset);

    // Helper functions to convert between ShaderLib and AssetLib types
    SamplerDescription::Filter ConvertSamplerFilter(const std::string& filter);
    SamplerDescription::AddressMode ConvertAddressMode(const std::string& mode);
    DescriptorType ConvertDescriptorType(const std::string& type);
    UniformType ConvertUniformType(const std::string& type);

    // =============================================
    // Funkcje dla shaderów
    // =============================================
    AssetData WriteShader(const std::string& source, const ShaderLib::ShaderData shaderData, CompressionType compression = CompressionType::LZ4, int compressionLevel = 1);
    std::tuple<ShaderLib::ShaderMetadata, std::vector<ShaderLib::CompiledStage>> ReadShader(const AssetData& asset);

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
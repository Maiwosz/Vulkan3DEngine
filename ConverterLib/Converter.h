#pragma once
#include <string>
#include "AssetLib.h"

using namespace AssetLib;

class Converter {
public:
    struct Settings {
        AssetLib::TextureFormat textureFormat = AssetLib::TextureFormat::RGBA8;
        int compressionLevel = 1;
        bool generateMipmaps = true;
    };

    void Convert(const std::string& inputPath,
        const std::string& outputPath,
        const Settings& settings);

private:
    struct ShaderStageSource {
        AssetLib::ShaderStage stage;
        std::string code;
    };

    AssetData ProcessTexture(const std::string& inputPath, const Settings& settings);
    AssetData ProcessMesh(const std::string& inputPath, const Settings& settings);
    AssetData ProcessMaterial(const std::string& inputPath, const Settings& settings);
    AssetData ProcessShader(const std::string& inputPath, const Settings& settings);

    std::vector<ShaderStageSource> ParseShaderStages(const std::string& source);
    void ProcessShaderReflection(const std::vector<uint32_t>& spirv, std::vector<AssetLib::DescriptorBinding>& bindings, std::vector<AssetLib::PushConstantRange>& pushConstants, AssetLib::ShaderStage stage);
    static std::vector<uint8_t> CompressBC7(const uint8_t* rgba, uint32_t width, uint32_t height);
    static uint32_t PadToMultipleOf4(uint32_t value);
    static void ValidateTextureDimensions(int width, int height);
};

namespace {
    template <typename T>
    inline void hash_combine(size_t& seed, const T& v) {
        std::hash<T> hasher;
        seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }

    struct DynamicVertex {
        std::array<float, 3> pos;
        std::array<float, 3> norm;
        std::array<float, 2> uv;
        bool hasNormals;
        bool hasTexCoords;

        bool operator==(const DynamicVertex& other) const {
            return pos == other.pos &&
                (!hasNormals || norm == other.norm) &&
                (!hasTexCoords || uv == other.uv);
        }
    };

    struct DynamicVertexHasher {
        size_t operator()(const DynamicVertex& v) const {
            size_t seed = 0;
            for (float val : v.pos) hash_combine(seed, val);
            if (v.hasNormals) for (float val : v.norm) hash_combine(seed, val);
            if (v.hasTexCoords) for (float val : v.uv) hash_combine(seed, val);
            return seed;
        }
    };

    struct ShaderReflectionData {
        std::vector<AssetLib::DescriptorBinding> bindings;
    };
}

#pragma once
#include <string>
#include "AssetLib.h"
#include <json.hpp>

using namespace AssetLib;
using json = nlohmann::json;

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
    AssetData ProcessTexture(const std::string& inputPath, const Settings& settings);
    AssetData ProcessMesh(const std::string& inputPath, const Settings& settings);
    AssetData ProcessMaterial(const std::string& inputPath, const Settings& settings);

    AssetLib::SamplerDescription ParseSamplerDescription(const json& value);

    static std::vector<uint8_t> CompressBC7(const uint8_t* rgba, uint32_t width, uint32_t height);
    static uint32_t PadToMultipleOf4(uint32_t value);
    static void ValidateTextureDimensions(int width, int height);

    // =============================================
    // Supported file extensions
    // =============================================
    static constexpr std::array<std::string_view, 3> TEXTURE_EXTENSIONS = { "png", "jpg", "tga" };
    static constexpr std::array<std::string_view, 1> MESH_EXTENSIONS = { "obj" };
    static constexpr std::array<std::string_view, 1> MATERIAL_EXTENSIONS = { "mat" };
    static constexpr std::array<std::string_view, 1> SHADER_EXTENSIONS = { "glsl" };

public:
    // Utility functions for extensions
    static std::vector<std::string_view> GetSupportedExtensions(AssetType type);
    static std::vector<std::string_view> GetAllSupportedExtensions();
    static bool IsExtensionSupported(const std::string& extension);
    static AssetType GetAssetTypeFromExtension(const std::string& extension);
};

namespace {
    template <typename T>
    inline void hash_combine(size_t& seed, const T& v) {
        std::hash<T> hasher;
        seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }

    struct DynamicVertex {
        std::array<float, 3> pos;      // Pozycja (zawsze)
        std::array<float, 3> norm;     // Normalna
        std::array<float, 2> uv;       // Współrzędne tekstury
        std::array<float, 4> color;    // Kolor (RGBA)
        bool hasNormals;               // Czy wierzchołek ma normalne
        bool hasTexCoords;             // Czy wierzchołek ma współrzędne tekstury
        bool hasColors;                // Czy wierzchołek ma kolory

        bool operator==(const DynamicVertex& other) const {
            // Zawsze porównujemy pozycję
            if (pos != other.pos) return false;

            // Porównujemy normalne tylko jeśli oba wierzchołki je mają
            if (hasNormals && other.hasNormals && norm != other.norm) return false;

            // Porównujemy współrzędne tekstury tylko jeśli oba wierzchołki je mają
            if (hasTexCoords && other.hasTexCoords && uv != other.uv) return false;

            // Porównujemy kolory tylko jeśli oba wierzchołki je mają
            if (hasColors && other.hasColors && color != other.color) return false;

            return true;
        }
    };

    struct DynamicVertexHasher {
        size_t operator()(const DynamicVertex& v) const {
            size_t seed = 0;

            // Zawsze zahaszuj pozycję
            for (float val : v.pos) hash_combine(seed, val);

            // Zahaszuj normalne tylko jeśli wierzchołek je ma
            if (v.hasNormals) {
                for (float val : v.norm) hash_combine(seed, val);
            }

            // Zahaszuj współrzędne tekstury tylko jeśli wierzchołek je ma
            if (v.hasTexCoords) {
                for (float val : v.uv) hash_combine(seed, val);
            }

            // Zahaszuj kolory tylko jeśli wierzchołek je ma
            if (v.hasColors) {
                for (float val : v.color) hash_combine(seed, val);
            }

            return seed;
        }
    };
}

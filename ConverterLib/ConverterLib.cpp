#include "pch.h"
#include "framework.h"
#include "ConverterLib.h"

#ifdef _DEBUG
#pragma comment(lib, "shaderc_combinedd.lib")
#pragma comment(lib, "spirv-cross-cored.lib")
#pragma comment(lib, "spirv-cross-glsld.lib") 
#else
#pragma comment(lib, "shaderc_combined.lib")
#pragma comment(lib, "spirv-cross-core.lib")
#pragma comment(lib, "spirv-cross-glsl.lib") 
#endif
#pragma comment(lib, "AssetLib.lib")
#pragma comment(lib, "ShaderLib.lib")

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include <json.hpp>
#include <lz4.h>
#include "bc7enc/bc7enc.h"

#include <fstream>
#include <unordered_map>

#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv_cross.hpp>
#include "Shader.h"
#include <iostream>
#include <MaterialTypes.h>
#include <MaterialSerializer.h>

#pragma pack(push, 1)
struct Vertex {
    float pos[3];
    float norm[3];
    float uv[2];

    bool operator==(const Vertex& other) const {
        return memcmp(this, &other, sizeof(Vertex)) == 0;
    }
};
#pragma pack(pop)

namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(const Vertex& v) const {
            size_t seed = 0;
            auto hash_combine = [](size_t& seed, auto val) {
                seed ^= hash<decltype(val)>()(val) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
                };

            for (int i = 0; i < 3; i++) hash_combine(seed, v.pos[i]);
            for (int i = 0; i < 3; i++) hash_combine(seed, v.norm[i]);
            for (int i = 0; i < 2; i++) hash_combine(seed, v.uv[i]);
            return seed;
        }
    };
}


void Converter::Convert(const std::string& inputPath,
    const std::string& outputPath,
    const Settings& settings)
{
    const std::string ext = inputPath.substr(inputPath.find_last_of(".") + 1);

    AssetData asset;

    // Check texture extensions
    for (const auto& texExt : TEXTURE_EXTENSIONS) {
        if (ext == texExt) {
            asset = ProcessTexture(inputPath, settings);
            AssetLib::WriteAsset(outputPath, asset);
            return;
        }
    }

    // Check mesh extensions
    for (const auto& meshExt : MESH_EXTENSIONS) {
        if (ext == meshExt) {
            asset = ProcessMesh(inputPath, settings);
            AssetLib::WriteAsset(outputPath, asset);
            return;
        }
    }

    // Check material extensions
    for (const auto& matExt : MATERIAL_EXTENSIONS) {
        if (ext == matExt) {
            asset = ProcessMaterial(inputPath, settings);
            AssetLib::WriteAsset(outputPath, asset);
            return;
        }
    }

    // Check shader extensions
    for (const auto& shaderExt : SHADER_EXTENSIONS) {
        if (ext == shaderExt) {
            asset = Shader::ProcessShader(inputPath, settings);
            AssetLib::WriteAsset(outputPath, asset);
            return;
        }
    }

    throw std::runtime_error("Unsupported file type: " + ext);
}

AssetData Converter::ProcessTexture(const std::string& inputPath, const Settings& settings)
{
    struct StbiDeleter {
        void operator()(stbi_uc* p) const { stbi_image_free(p); }
    };

    // Load image with STBI
    int width, height, channels;
    std::unique_ptr<stbi_uc, StbiDeleter> pixels(
        stbi_load(inputPath.c_str(), &width, &height, &channels, STBI_rgb_alpha),
        StbiDeleter{}
    );

    ValidateTextureDimensions(width, height);

    if (!pixels) {
        throw std::runtime_error("Failed to load texture: " + inputPath);
    }

    // Ensure we're working with RGBA (4 bytes per pixel)
    const int bytesPerPixel = 4;

    // Generate mipmaps
    std::vector<std::vector<uint8_t>> mipLevels;
    uint32_t currentWidth = width;
    uint32_t currentHeight = height;

    // Add original level (level 0)
    std::vector<uint8_t> originalLevel(pixels.get(), pixels.get() + width * height * bytesPerPixel);
    mipLevels.push_back(std::move(originalLevel));

    // Generate additional mipmap levels if required
    if (settings.generateMipmaps) {
        while (currentWidth > 1 || currentHeight > 1) {
            // Calculate dimensions for next level
            uint32_t nextWidth = std::max(currentWidth / 2, 1U);
            uint32_t nextHeight = std::max(currentHeight / 2, 1U);

            // Allocate memory for next level
            std::vector<uint8_t> nextLevel(nextWidth * nextHeight * bytesPerPixel);

            // Resize image to next mipmap level using stbir
            stbir_resize_uint8(
                mipLevels.back().data(), currentWidth, currentHeight, 0,
                nextLevel.data(), nextWidth, nextHeight, 0, bytesPerPixel
            );

            // Store the mip level
            mipLevels.push_back(std::move(nextLevel));

            // Update dimensions for next iteration
            currentWidth = nextWidth;
            currentHeight = nextHeight;
        }
    }

    // Prepare texture info
    AssetLib::TextureInfo texInfo;
    texInfo.format = AssetLib::TextureFormat::RGBA8; // Ensure we're working with RGBA8
    texInfo.width = static_cast<uint32_t>(width);
    texInfo.height = static_cast<uint32_t>(height);
    texInfo.mipLevels = static_cast<uint32_t>(mipLevels.size());
    texInfo.flags = 0;
    texInfo.mips.reserve(mipLevels.size());

    // Combine all mipmap levels into a single buffer and fill the mip info
    std::vector<uint8_t> pixelData;
    size_t totalSize = 0;

    // First, calculate total size needed
    for (const auto& mip : mipLevels) {
        totalSize += mip.size();
    }

    // Pre-allocate the buffer
    pixelData.reserve(totalSize);

    // Copy each mip level into the combined buffer and record its info
    uint32_t offset = 0;
    uint32_t mipIndex = 0;
    uint32_t mipWidth = width;
    uint32_t mipHeight = height;

    for (const auto& mip : mipLevels) {
        // Create MipLevel entry
        AssetLib::MipLevel mipInfo;
        mipInfo.width = mipWidth;
        mipInfo.height = mipHeight;
        mipInfo.dataOffset = offset;
        mipInfo.dataSize = static_cast<uint32_t>(mip.size());

        // Add mip info to texture
        texInfo.mips.push_back(mipInfo);

        // Copy data to combined buffer
        pixelData.insert(pixelData.end(), mip.begin(), mip.end());

        // Update offset for next mip level
        offset += mipInfo.dataSize;

        // Update dimensions for next mip level
        mipWidth = std::max(mipWidth / 2, 1U);
        mipHeight = std::max(mipHeight / 2, 1U);
        mipIndex++;
    }

    std::string filename = std::filesystem::path(inputPath).filename().string();

    // Write using standardized function
    AssetData asset = AssetLib::WriteTexture(
        filename,
        texInfo,
        pixelData,
        AssetLib::CompressionType::LZ4,
        settings.compressionLevel
    );

    return asset;
}

AssetData Converter::ProcessMesh(const std::string& inputPath, const Settings& settings)
{
    try {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, inputPath.c_str())) {
            throw std::runtime_error("OBJ load error: " + warn + err);
        }

        // Sprawdź, które atrybuty są dostępne w modelu
        const bool hasNormals = !attrib.normals.empty();
        const bool hasTexCoords = !attrib.texcoords.empty();
        const bool hasColors = !attrib.colors.empty();

        // Weryfikacja indeksów dla dostępnych atrybutów
        for (const auto& shape : shapes) {
            for (const auto& index : shape.mesh.indices) {
                if (index.vertex_index < 0) throw std::runtime_error("Missing vertex index");
                if (hasNormals && index.normal_index < 0) throw std::runtime_error("Missing normal index");
                if (hasTexCoords && index.texcoord_index < 0) throw std::runtime_error("Missing UV index");
            }
        }

        // Zdefiniujmy wartości domyślne dla brakujących atrybutów
        const std::array<float, 3> defaultNormal = { 0.0f, 0.0f, 1.0f };  // Domyślnie w kierunku Z
        const std::array<float, 2> defaultTexCoord = { 0.0f, 0.0f };      // Lewy dolny róg tekstury
        const std::array<float, 4> defaultColor = { 1.0f, 1.0f, 1.0f, 1.0f }; // Biały kolor

        std::vector<DynamicVertex> vertices;
        std::vector<uint32_t> indices;
        std::unordered_map<DynamicVertex, uint32_t, DynamicVertexHasher> uniqueVertices;

        // Definicja jakie atrybuty chcemy mieć w każdej siatce
        const bool generateNormals = true;    // Zawsze chcemy mieć normalne
        const bool generateTexCoords = true;  // Zawsze chcemy mieć współrzędne tekstury
        const bool generateColors = true;     // Zawsze chcemy mieć kolory

        // Obliczanie normalnych, jeśli ich brak
        std::vector<std::array<float, 3>> calculatedNormals;
        if (!hasNormals && generateNormals) {

            // Alokuj miejsce na normalne dla każdego wierzchołka
            calculatedNormals.resize(attrib.vertices.size() / 3, { 0.0f, 0.0f, 0.0f });

            // Oblicz normalne dla każdego trójkąta i dodaj do wierzchołków
            for (const auto& shape : shapes) {
                for (size_t f = 0; f < shape.mesh.indices.size() / 3; f++) {
                    // Pobierz indeksy wierzchołków trójkąta
                    tinyobj::index_t idx0 = shape.mesh.indices[3 * f + 0];
                    tinyobj::index_t idx1 = shape.mesh.indices[3 * f + 1];
                    tinyobj::index_t idx2 = shape.mesh.indices[3 * f + 2];

                    // Pobierz współrzędne wierzchołków
                    float v0[3] = {
                        attrib.vertices[3 * idx0.vertex_index + 0],
                        attrib.vertices[3 * idx0.vertex_index + 1],
                        attrib.vertices[3 * idx0.vertex_index + 2]
                    };
                    float v1[3] = {
                        attrib.vertices[3 * idx1.vertex_index + 0],
                        attrib.vertices[3 * idx1.vertex_index + 1],
                        attrib.vertices[3 * idx1.vertex_index + 2]
                    };
                    float v2[3] = {
                        attrib.vertices[3 * idx2.vertex_index + 0],
                        attrib.vertices[3 * idx2.vertex_index + 1],
                        attrib.vertices[3 * idx2.vertex_index + 2]
                    };

                    // Oblicz wektory krawędzi
                    float e1[3] = { v1[0] - v0[0], v1[1] - v0[1], v1[2] - v0[2] };
                    float e2[3] = { v2[0] - v0[0], v2[1] - v0[1], v2[2] - v0[2] };

                    // Oblicz iloczyn wektorowy, aby uzyskać normalną
                    float normal[3] = {
                        e1[1] * e2[2] - e1[2] * e2[1],
                        e1[2] * e2[0] - e1[0] * e2[2],
                        e1[0] * e2[1] - e1[1] * e2[0]
                    };

                    // Dodaj normalną do wszystkich trzech wierzchołków trójkąta
                    for (int i = 0; i < 3; i++) {
                        calculatedNormals[idx0.vertex_index][i] += normal[i];
                        calculatedNormals[idx1.vertex_index][i] += normal[i];
                        calculatedNormals[idx2.vertex_index][i] += normal[i];
                    }
                }
            }

            // Normalizuj wszystkie normalne
            for (auto& n : calculatedNormals) {
                float len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
                if (len > 0.00001f) {
                    n[0] /= len;
                    n[1] /= len;
                    n[2] /= len;
                }
                else {
                    // Jeśli długość jest bliska zeru, ustaw domyślną normalną
                    n = defaultNormal;
                }
            }
        }

        for (const auto& shape : shapes) {
            for (const auto& index : shape.mesh.indices) {
                DynamicVertex vertex{};

                // Pozycja (zawsze dostępna)
                const int vi = 3 * index.vertex_index;
                vertex.pos = { attrib.vertices[vi], attrib.vertices[vi + 1], attrib.vertices[vi + 2] };

                // Normalne (oryginalne lub wygenerowane)
                vertex.hasNormals = generateNormals;
                if (hasNormals) {
                    const int ni = 3 * index.normal_index;
                    vertex.norm = { attrib.normals[ni], attrib.normals[ni + 1], attrib.normals[ni + 2] };
                }
                else if (generateNormals) {
                    vertex.norm = calculatedNormals[index.vertex_index];
                }
                else {
                    vertex.norm = defaultNormal;
                }

                // Współrzędne tekstury (oryginalne lub domyślne)
                vertex.hasTexCoords = generateTexCoords;
                if (hasTexCoords) {
                    const int ti = 2 * index.texcoord_index;
                    vertex.uv = { attrib.texcoords[ti], 1.0f - attrib.texcoords[ti + 1] }; // Obróć Y dla Vulkana
                }
                else if (generateTexCoords) {
                    vertex.uv = defaultTexCoord;
                }

                // Kolory (oryginalne lub domyślne)
                vertex.hasColors = generateColors;
                if (hasColors) {
                    const int ci = 3 * index.vertex_index;
                    vertex.color = {
                        attrib.colors[ci],
                        attrib.colors[ci + 1],
                        attrib.colors[ci + 2],
                        1.0f  // Domyślna wartość alpha
                    };
                }
                else if (generateColors) {
                    vertex.color = defaultColor;
                }

                auto it = uniqueVertices.find(vertex);
                if (it == uniqueVertices.end()) {
                    uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                    vertices.push_back(vertex);
                }
                indices.push_back(uniqueVertices[vertex]);
            }
        }

        // Określ typ indeksów w zależności od liczby wierzchołków
        const bool use16Bit = (vertices.size() - 1) <= std::numeric_limits<uint16_t>::max();
        uint8_t indexType = use16Bit ? 0 : 1;
        std::vector<uint8_t> indexBuffer;

        if (use16Bit) {
            std::vector<uint16_t> indices16(indices.begin(), indices.end());
            indexBuffer.resize(indices16.size() * sizeof(uint16_t));
            memcpy(indexBuffer.data(), indices16.data(), indexBuffer.size());
        }
        else {
            indexBuffer.resize(indices.size() * sizeof(uint32_t));
            memcpy(indexBuffer.data(), indices.data(), indexBuffer.size());
        }

        // Określ rozmiar wierzchołka na podstawie wymaganych atrybutów
        const size_t positionSize = 3 * sizeof(float);
        const size_t normalSize = generateNormals ? 3 * sizeof(float) : 0;
        const size_t texCoordSize = generateTexCoords ? 2 * sizeof(float) : 0;
        const size_t colorSize = generateColors ? 4 * sizeof(float) : 0;

        const size_t vertexSize = positionSize + normalSize + texCoordSize + colorSize;

        std::vector<uint8_t> vertexData(vertices.size() * vertexSize);
        uint8_t* ptr = vertexData.data();

        for (const auto& v : vertices) {
            // Pozycja (zawsze)
            memcpy(ptr, v.pos.data(), positionSize);
            ptr += positionSize;

            // Normalne (jeśli wymagane)
            if (generateNormals) {
                memcpy(ptr, v.norm.data(), normalSize);
                ptr += normalSize;
            }

            // Współrzędne tekstury (jeśli wymagane)
            if (generateTexCoords) {
                memcpy(ptr, v.uv.data(), texCoordSize);
                ptr += texCoordSize;
            }

            // Kolory (jeśli wymagane)
            if (generateColors) {
                memcpy(ptr, v.color.data(), colorSize);
                ptr += colorSize;
            }
        }

        // Określ flagi atrybutów
        uint32_t attributeFlags = static_cast<uint32_t>(AssetLib::VertexAttribute::Position);
        if (generateNormals) attributeFlags |= static_cast<uint32_t>(AssetLib::VertexAttribute::Normal);
        if (generateTexCoords) attributeFlags |= static_cast<uint32_t>(AssetLib::VertexAttribute::TexCoord);
        if (generateColors) attributeFlags |= static_cast<uint32_t>(AssetLib::VertexAttribute::Color);

        // Przygotuj informacje o siatce
        AssetLib::MeshInfo meshInfo{};
        meshInfo.vertexCount = static_cast<uint32_t>(vertices.size());
        meshInfo.indexCount = static_cast<uint32_t>(indices.size());
        meshInfo.vertexStride = static_cast<uint32_t>(vertexSize);
        meshInfo.attributes = attributeFlags;
        meshInfo.indexType = indexType;

        uint32_t currentOffset = 0;

        // Position (zawsze pierwsza)
        meshInfo.attributeLayout.push_back({
            AssetLib::VertexAttribute::Position,
            currentOffset,
            3,  // xyz
            sizeof(float)
            });
        currentOffset += 3 * sizeof(float);

        // Normal (jeśli obecna)
        if (generateNormals) {
            meshInfo.attributeLayout.push_back({
                AssetLib::VertexAttribute::Normal,
                currentOffset,
                3,  // xyz
                sizeof(float)
                });
            currentOffset += 3 * sizeof(float);
        }

        // TexCoord (jeśli obecne)
        if (generateTexCoords) {
            meshInfo.attributeLayout.push_back({
                AssetLib::VertexAttribute::TexCoord,
                currentOffset,
                2,  // uv
                sizeof(float)
                });
            currentOffset += 2 * sizeof(float);
        }

        // Color (jeśli obecny)
        if (generateColors) {
            meshInfo.attributeLayout.push_back({
                AssetLib::VertexAttribute::Color,
                currentOffset,
                4,  // rgba
                sizeof(float)
                });
            currentOffset += 4 * sizeof(float);
        }

        std::string filename = std::filesystem::path(inputPath).filename().string();

        // Zapis przy użyciu ustandaryzowanej funkcji
        AssetData asset = AssetLib::WriteMesh(
            filename,
            meshInfo,
            vertexData,
            indexBuffer,
            AssetLib::CompressionType::LZ4,
            settings.compressionLevel
        );

        return asset;
    }
    catch (const std::exception& e) {
        throw std::runtime_error("Mesh processing failed: " + std::string(e.what()));
    }
}

AssetLib::AssetData Converter::ProcessMaterial(const std::string& inputPath, const Settings& settings)
{
    // Load JSON file
    std::ifstream file(inputPath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open material file: " + inputPath);
    }

    json materialJson;
    file >> materialJson;

    // Deserialize using new helper
    AssetLib::MaterialDefinition material = AssetLib::MaterialFromJson(materialJson);

    // Generate asset data
    std::string filename = std::filesystem::path(inputPath).filename().string();

    return AssetLib::WriteMaterial(
        filename,
        material,
        settings.compressionLevel > 0 ? AssetLib::CompressionType::LZ4 : AssetLib::CompressionType::None,
        settings.compressionLevel
    );
}

std::vector<uint8_t> Converter::CompressBC7(const uint8_t* rgba, uint32_t width, uint32_t height) {
    if (width % 4 != 0 || height % 4 != 0) {
        throw std::runtime_error("BC7 requires dimensions divisible by 4");
    }

    bc7enc_compress_block_params params;
    bc7enc_compress_block_params_init(&params);
    params.m_uber_level = 3; // Zmień na niższy poziom (np. 1) dla testów

    const uint32_t blocksX = (width + 3) / 4;
    const uint32_t blocksY = (height + 3) / 4;
    std::vector<uint8_t> output(blocksX * blocksY * 16);

    for (uint32_t y = 0; y < blocksY; ++y) {
        for (uint32_t x = 0; x < blocksX; ++x) {
            uint8_t block[4][4][4] = { 0 };

            // Poprawna ekstrakcja z uwzględnieniem paddingu
            for (uint32_t py = 0; py < 4; ++py) {
                const uint32_t srcY = y * 4 + py;
                if (srcY >= height) continue; // Pomijaj wiersze poza teksturą

                for (uint32_t px = 0; px < 4; ++px) {
                    const uint32_t srcX = x * 4 + px;
                    if (srcX >= width) continue; // Pomijaj kolumny poza teksturą

                    const size_t srcOffset = (srcY * width + srcX) * 4;
                    memcpy(block[py][px], &rgba[srcOffset], 4);
                }
            }

            bc7enc_compress_block(&output[(y * blocksX + x) * 16], block, &params);
        }
    }

    return output;
}

uint32_t Converter::PadToMultipleOf4(uint32_t value) {
    return (value + 3) & ~3;
}

void Converter::ValidateTextureDimensions(int width, int height) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument(
            "Invalid texture dimensions: " +
            std::to_string(width) + "x" + std::to_string(height)
        );
    }
}

std::vector<std::string_view> Converter::GetSupportedExtensions(AssetType type)
{
    std::vector<std::string_view> extensions;

    switch (type) {
    case AssetType::Texture:
        extensions.insert(extensions.end(), TEXTURE_EXTENSIONS.begin(), TEXTURE_EXTENSIONS.end());
        break;
    case AssetType::Mesh:
        extensions.insert(extensions.end(), MESH_EXTENSIONS.begin(), MESH_EXTENSIONS.end());
        break;
    case AssetType::Material:
        extensions.insert(extensions.end(), MATERIAL_EXTENSIONS.begin(), MATERIAL_EXTENSIONS.end());
        break;
    case AssetType::Shader:
        extensions.insert(extensions.end(), SHADER_EXTENSIONS.begin(), SHADER_EXTENSIONS.end());
        break;
    }

    return extensions;
}

std::vector<std::string_view> Converter::GetAllSupportedExtensions()
{
    std::vector<std::string_view> allExtensions;

    allExtensions.insert(allExtensions.end(), TEXTURE_EXTENSIONS.begin(), TEXTURE_EXTENSIONS.end());
    allExtensions.insert(allExtensions.end(), MESH_EXTENSIONS.begin(), MESH_EXTENSIONS.end());
    allExtensions.insert(allExtensions.end(), MATERIAL_EXTENSIONS.begin(), MATERIAL_EXTENSIONS.end());
    allExtensions.insert(allExtensions.end(), SHADER_EXTENSIONS.begin(), SHADER_EXTENSIONS.end());

    return allExtensions;
}

bool Converter::IsExtensionSupported(const std::string& extension)
{
    auto allExtensions = GetAllSupportedExtensions();
    return std::find(allExtensions.begin(), allExtensions.end(), extension) != allExtensions.end();
}

AssetType Converter::GetAssetTypeFromExtension(const std::string& extension)
{
    // Check each type's extensions
    for (const auto& ext : TEXTURE_EXTENSIONS) {
        if (extension == ext) return AssetType::Texture;
    }
    for (const auto& ext : MESH_EXTENSIONS) {
        if (extension == ext) return AssetType::Mesh;
    }
    for (const auto& ext : MATERIAL_EXTENSIONS) {
        if (extension == ext) return AssetType::Material;
    }
    for (const auto& ext : SHADER_EXTENSIONS) {
        if (extension == ext) return AssetType::Shader;
    }

    throw std::runtime_error("Unsupported extension: " + extension);
}
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


using json = nlohmann::json;

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

    if (ext == "png" || ext == "jpg" || ext == "tga") {
        asset = ProcessTexture(inputPath, settings);
    }
    else if (ext == "obj") {
        asset = ProcessMesh(inputPath, settings);
    }
    else if (ext == "mat") {
        asset = ProcessMaterial(inputPath, settings);
    }
    else if (ext == "glsl") {
        asset = Shader::ProcessShader(inputPath, settings);
    }
    else {
        throw std::runtime_error("Unsupported file type: " + ext);
    }

	AssetLib::WriteAsset(outputPath, asset);
}

AssetData Converter::ProcessTexture(const std::string& inputPath, const Settings& settings)
{
    struct StbiDeleter {
        void operator()(stbi_uc* p) const { stbi_image_free(p); }
    };

    // Ładowanie obrazu
    int width, height, channels;
    std::unique_ptr<stbi_uc, StbiDeleter> pixels(
        stbi_load(inputPath.c_str(), &width, &height, &channels, STBI_rgb_alpha),
        StbiDeleter{}
    );

    ValidateTextureDimensions(width, height);
    if (!pixels) {
        throw std::runtime_error("Failed to load texture: " + inputPath);
    }

    // Generowanie mipmap
    std::vector<std::vector<uint8_t>> mipLevels;
    std::vector<std::pair<uint32_t, uint32_t>> mipSizes;
    uint32_t currentWidth = width;
    uint32_t currentHeight = height;
    const uint8_t* currentPixels = pixels.get();

    while (true) {
        uint32_t paddedWidth = currentWidth;
        uint32_t paddedHeight = currentHeight;
        if (settings.textureFormat == AssetLib::TextureFormat::BC7) {
            paddedWidth = PadToMultipleOf4(paddedWidth);
            paddedHeight = PadToMultipleOf4(paddedHeight);
        }

        std::vector<uint8_t> resized(paddedWidth * paddedHeight * 4, 0);
        stbir_resize_uint8(
            currentPixels, currentWidth, currentHeight, 0,
            resized.data(), paddedWidth, paddedHeight, 0, 4
        );

        mipLevels.push_back(std::move(resized));
        mipSizes.emplace_back(paddedWidth, paddedHeight);

        if ((paddedWidth <= 1 && paddedHeight <= 1) || !settings.generateMipmaps) break;

        currentWidth = std::max(paddedWidth / 2, 1U);
        currentHeight = std::max(paddedHeight / 2, 1U);
        currentPixels = mipLevels.back().data();
    }

    // Przygotowanie danych tekstury i zapis przy użyciu AssetLib
    AssetLib::TextureInfo texInfo;
    texInfo.format = settings.textureFormat;
    texInfo.width = static_cast<uint32_t>(width);
    texInfo.height = static_cast<uint32_t>(height);
    texInfo.mipLevels = static_cast<uint32_t>(mipLevels.size());
    texInfo.flags = 0;

    std::vector<uint8_t> pixelData;

    if (settings.textureFormat == AssetLib::TextureFormat::BC7) {
        // Kompresja BC7 dla każdego poziomu mipmapy
        for (size_t i = 0; i < mipLevels.size(); ++i) {
            auto compressed = CompressBC7(
                mipLevels[i].data(),
                mipSizes[i].first,
                mipSizes[i].second
            );
            pixelData.insert(pixelData.end(), compressed.begin(), compressed.end());
        }
    }
    else {
        // Składanie surowych danych RGBA8
        for (const auto& mip : mipLevels) {
            pixelData.insert(pixelData.end(), mip.begin(), mip.end());
        }
    }

    std::string filename = std::filesystem::path(inputPath).filename().string();

    // Zapis przy użyciu ustandaryzowanej funkcji
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

        const bool hasNormals = !attrib.normals.empty();
        const bool hasTexCoords = !attrib.texcoords.empty();

        for (const auto& shape : shapes) {
            for (const auto& index : shape.mesh.indices) {
                if (index.vertex_index < 0) throw std::runtime_error("Missing vertex index");
                if (hasNormals && index.normal_index < 0) throw std::runtime_error("Missing normal index");
                if (hasTexCoords && index.texcoord_index < 0) throw std::runtime_error("Missing UV index");
            }
        }

        std::vector<DynamicVertex> vertices;
        std::vector<uint32_t> indices;
        std::unordered_map<DynamicVertex, uint32_t, DynamicVertexHasher> uniqueVertices;

        for (const auto& shape : shapes) {
            for (const auto& index : shape.mesh.indices) {
                DynamicVertex vertex{};
                vertex.hasNormals = hasNormals;
                vertex.hasTexCoords = hasTexCoords;

                const int vi = 3 * index.vertex_index;
                vertex.pos = { attrib.vertices[vi], attrib.vertices[vi + 1], attrib.vertices[vi + 2] };

                if (hasNormals) {
                    const int ni = 3 * index.normal_index;
                    vertex.norm = { attrib.normals[ni], attrib.normals[ni + 1], attrib.normals[ni + 2] };
                }

                if (hasTexCoords) {
                    const int ti = 2 * index.texcoord_index;
                    vertex.uv = { attrib.texcoords[ti], attrib.texcoords[ti + 1] };
                }

                auto it = uniqueVertices.find(vertex);
                if (it == uniqueVertices.end()) {
                    uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                    vertices.push_back(vertex);
                }
                indices.push_back(uniqueVertices[vertex]);
            }
        }

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

        const size_t vertexSize = 3 * sizeof(float)
            + (hasNormals ? 3 * sizeof(float) : 0)
            + (hasTexCoords ? 2 * sizeof(float) : 0);

        std::vector<uint8_t> vertexData(vertices.size() * vertexSize);
        uint8_t* ptr = vertexData.data();

        for (const auto& v : vertices) {
            memcpy(ptr, v.pos.data(), 3 * sizeof(float));
            ptr += 3 * sizeof(float);

            if (hasNormals) {
                memcpy(ptr, v.norm.data(), 3 * sizeof(float));
                ptr += 3 * sizeof(float);
            }

            if (hasTexCoords) {
                memcpy(ptr, v.uv.data(), 2 * sizeof(float));
                ptr += 2 * sizeof(float);
            }
        }

        std::vector<uint8_t> meshData;
        meshData.reserve(vertexData.size() + indexBuffer.size());
        meshData.insert(meshData.end(), vertexData.begin(), vertexData.end());
        meshData.insert(meshData.end(), indexBuffer.begin(), indexBuffer.end());

        AssetLib::MeshInfo meshInfo{};
        meshInfo.vertexCount = static_cast<uint32_t>(vertices.size());
        meshInfo.indexCount = static_cast<uint32_t>(indices.size());
        meshInfo.vertexStride = static_cast<uint32_t>(vertexSize);
        meshInfo.attributes = static_cast<uint32_t>(
            AssetLib::VertexAttribute::Position |
            (hasNormals ? AssetLib::VertexAttribute::Normal : AssetLib::VertexAttribute(0)) |
            (hasTexCoords ? AssetLib::VertexAttribute::TexCoord : AssetLib::VertexAttribute(0))
            );
        meshInfo.indexType = indexType;

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

AssetData Converter::ProcessMaterial(const std::string& inputPath, const Settings& settings)
{
    // Wczytaj plik materiału
    std::ifstream file(inputPath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open material file: " + inputPath);
    }

    json materialJson;
    file >> materialJson;

    // Walidacja struktury
    if (!materialJson.contains("shader") || !materialJson.contains("parameters")) {
        throw std::runtime_error("Invalid material structure");
    }

    // Przygotuj struktury danych
    AssetLib::MaterialInfo matInfo{};
    std::vector<AssetLib::MaterialParameter> params;
    std::vector<uint8_t> paramData;

    // Wczytaj nazwę shadera
    const std::string shaderName = materialJson["shader"].get<std::string>();
    strncpy_s(matInfo.shaderName.data(), matInfo.shaderName.size(), shaderName.c_str(), _TRUNCATE);

    // Przetwarzaj parametry
    for (auto& [key, value] : materialJson["parameters"].items()) {
        AssetLib::MaterialParameter param{};
        strncpy_s(param.name.data(), param.name.size(), key.c_str(), _TRUNCATE);
        param.arraySize = value.value("arraySize", 1);
        param.dataOffset = static_cast<uint32_t>(paramData.size());

        // Każdy parametr musi zawierać descriptorType
        if (!value.contains("descriptorType")) {
            throw std::runtime_error("Parameter missing descriptorType: " + key);
        }

        const std::string descriptorTypeStr = value["descriptorType"].get<std::string>();
        param.descriptorType = AssetLib::ConvertDescriptorType(descriptorTypeStr);

        // Pobierz uniformType jeśli istnieje
        if (value.contains("uniformType")) {
            const std::string uniformTypeStr = value["uniformType"].get<std::string>();
            param.uniformType = AssetLib::ConvertUniformType(uniformTypeStr);
        }
        else {
            param.uniformType = ShaderLib::UniformType::Unknown;
        }

        // Obsługa ImageSampler
        if (param.descriptorType == AssetLib::DescriptorType::CombinedImageSampler ||
            param.descriptorType == AssetLib::DescriptorType::SeparateImage) {
            // Parsuj ścieżkę tekstury
            if (!value.contains("path")) {
                throw std::runtime_error("Texture parameter missing path: " + key);
            }

            const std::string texPath = value["path"].get<std::string>();
            paramData.insert(paramData.end(), texPath.begin(), texPath.end());
            paramData.push_back('\0');  // Null-terminated string
            param.dataSize = static_cast<uint32_t>(texPath.length() + 1);

            // Parsuj sampler
            if (value.contains("sampler")) {
                auto& s = value["sampler"];
                param.samplerDesc.magFilter = AssetLib::ConvertSamplerFilter(s.value("magFilter", "Linear"));
                param.samplerDesc.minFilter = AssetLib::ConvertSamplerFilter(s.value("minFilter", "Linear"));
                param.samplerDesc.addressModeU = AssetLib::ConvertAddressMode(s.value("addressModeU", "Repeat"));
                param.samplerDesc.addressModeV = AssetLib::ConvertAddressMode(s.value("addressModeV", "Repeat"));
                param.samplerDesc.addressModeW = AssetLib::ConvertAddressMode(s.value("addressModeW", "Repeat"));
                param.samplerDesc.anisotropy = s.value("anisotropy", 1.0f);
                param.samplerDesc.minLod = s.value("minLod", 0.0f);
                param.samplerDesc.maxLod = s.value("maxLod", 16.0f);
            }
            else {
                // Domyślny sampler jeśli nie podano
                param.samplerDesc = {
                    AssetLib::SamplerDescription::Filter::Linear,
                    AssetLib::SamplerDescription::Filter::Linear,
                    AssetLib::SamplerDescription::AddressMode::Repeat,
                    AssetLib::SamplerDescription::AddressMode::Repeat,
                    AssetLib::SamplerDescription::AddressMode::Repeat,
                    1.0f,
                    0.0f,
                    16.0f
                };
            }
        }
        // UniformBuffer parameters
        else if (param.descriptorType == AssetLib::DescriptorType::UniformBuffer) {
            if (!value.contains("data")) {
                throw std::runtime_error("UniformBuffer parameter missing data: " + key);
            }

            const auto& data = value["data"];

            // Handle different uniform types
            switch (param.uniformType) {
            case ShaderLib::UniformType::Float: {
                float val = data.get<float>();
                const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&val);
                paramData.insert(paramData.end(), bytes, bytes + sizeof(float));
                param.dataSize = sizeof(float);
                break;
            }
            case ShaderLib::UniformType::Vec2: {
                std::vector<float> vec = data.get<std::vector<float>>();
                if (vec.size() != 2) throw std::runtime_error("Vec2 requires 2 values for: " + key);
                const uint8_t* bytes = reinterpret_cast<const uint8_t*>(vec.data());
                paramData.insert(paramData.end(), bytes, bytes + 2 * sizeof(float));
                param.dataSize = 2 * sizeof(float);
                break;
            }
            case ShaderLib::UniformType::Vec3: {
                std::vector<float> vec = data.get<std::vector<float>>();
                if (vec.size() != 3) throw std::runtime_error("Vec3 requires 3 values for: " + key);
                const uint8_t* bytes = reinterpret_cast<const uint8_t*>(vec.data());
                paramData.insert(paramData.end(), bytes, bytes + 3 * sizeof(float));
                param.dataSize = 3 * sizeof(float);
                break;
            }
            case ShaderLib::UniformType::Vec4: {
                std::vector<float> vec = data.get<std::vector<float>>();
                if (vec.size() != 4) throw std::runtime_error("Vec4 requires 4 values for: " + key);
                const uint8_t* bytes = reinterpret_cast<const uint8_t*>(vec.data());
                paramData.insert(paramData.end(), bytes, bytes + 4 * sizeof(float));
                param.dataSize = 4 * sizeof(float);
                break;
            }
            case ShaderLib::UniformType::Mat3: {
                std::vector<float> mat = data.get<std::vector<float>>();
                if (mat.size() != 9) throw std::runtime_error("Mat3 requires 9 values for: " + key);
                const uint8_t* bytes = reinterpret_cast<const uint8_t*>(mat.data());
                paramData.insert(paramData.end(), bytes, bytes + 9 * sizeof(float));
                param.dataSize = 9 * sizeof(float);
                break;
            }
            case ShaderLib::UniformType::Mat4: {
                std::vector<float> mat = data.get<std::vector<float>>();
                if (mat.size() != 16) throw std::runtime_error("Mat4 requires 16 values for: " + key);
                const uint8_t* bytes = reinterpret_cast<const uint8_t*>(mat.data());
                paramData.insert(paramData.end(), bytes, bytes + 16 * sizeof(float));
                param.dataSize = 16 * sizeof(float);
                break;
            }
            case ShaderLib::UniformType::Int:
            case ShaderLib::UniformType::UInt: {
                int val = data.get<int>();
                const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&val);
                paramData.insert(paramData.end(), bytes, bytes + sizeof(int));
                param.dataSize = sizeof(int);
                break;
            }
            case ShaderLib::UniformType::IVec2:
            case ShaderLib::UniformType::UVec2: {
                std::vector<int> vec = data.get<std::vector<int>>();
                if (vec.size() != 2) throw std::runtime_error("IVec2/UVec2 requires 2 values for: " + key);
                const uint8_t* bytes = reinterpret_cast<const uint8_t*>(vec.data());
                paramData.insert(paramData.end(), bytes, bytes + 2 * sizeof(int));
                param.dataSize = 2 * sizeof(int);
                break;
            }
            case ShaderLib::UniformType::IVec3:
            case ShaderLib::UniformType::UVec3: {
                std::vector<int> vec = data.get<std::vector<int>>();
                if (vec.size() != 3) throw std::runtime_error("IVec3/UVec3 requires 3 values for: " + key);
                const uint8_t* bytes = reinterpret_cast<const uint8_t*>(vec.data());
                paramData.insert(paramData.end(), bytes, bytes + 3 * sizeof(int));
                param.dataSize = 3 * sizeof(int);
                break;
            }
            case ShaderLib::UniformType::IVec4:
            case ShaderLib::UniformType::UVec4: {
                std::vector<int> vec = data.get<std::vector<int>>();
                if (vec.size() != 4) throw std::runtime_error("IVec4/UVec4 requires 4 values for: " + key);
                const uint8_t* bytes = reinterpret_cast<const uint8_t*>(vec.data());
                paramData.insert(paramData.end(), bytes, bytes + 4 * sizeof(int));
                param.dataSize = 4 * sizeof(int);
                break;
            }
            case ShaderLib::UniformType::Double: {
                double val = data.get<double>();
                const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&val);
                paramData.insert(paramData.end(), bytes, bytes + sizeof(double));
                param.dataSize = sizeof(double);
                break;
            }
            case ShaderLib::UniformType::DVec2: {
                std::vector<double> vec = data.get<std::vector<double>>();
                if (vec.size() != 2) throw std::runtime_error("DVec2 requires 2 values for: " + key);
                const uint8_t* bytes = reinterpret_cast<const uint8_t*>(vec.data());
                paramData.insert(paramData.end(), bytes, bytes + 2 * sizeof(double));
                param.dataSize = 2 * sizeof(double);
                break;
            }
            case ShaderLib::UniformType::DVec3: {
                std::vector<double> vec = data.get<std::vector<double>>();
                if (vec.size() != 3) throw std::runtime_error("DVec3 requires 3 values for: " + key);
                const uint8_t* bytes = reinterpret_cast<const uint8_t*>(vec.data());
                paramData.insert(paramData.end(), bytes, bytes + 3 * sizeof(double));
                param.dataSize = 3 * sizeof(double);
                break;
            }
            case ShaderLib::UniformType::DVec4: {
                std::vector<double> vec = data.get<std::vector<double>>();
                if (vec.size() != 4) throw std::runtime_error("DVec4 requires 4 values for: " + key);
                const uint8_t* bytes = reinterpret_cast<const uint8_t*>(vec.data());
                paramData.insert(paramData.end(), bytes, bytes + 4 * sizeof(double));
                param.dataSize = 4 * sizeof(double);
                break;
            }
            case ShaderLib::UniformType::Struct:
            case ShaderLib::UniformType::Array:
                // Te typy mogą wymagać dodatkowej obsługi
                throw std::runtime_error("Complex type not yet supported for parameter: " + key);
            default:
                throw std::runtime_error("Unsupported uniform type for parameter: " + key);
            }
        }
        else if (param.descriptorType == AssetLib::DescriptorType::StorageBuffer) {
            // StorageBuffer może wymagać bardziej złożonej obsługi
            throw std::runtime_error("StorageBuffer not yet supported for parameter: " + key);
        }
        else if (param.descriptorType == AssetLib::DescriptorType::SeparateSampler) {
            // Sampler bez obrazu
            if (value.contains("sampler")) {
                auto& s = value["sampler"];
                param.samplerDesc.magFilter = AssetLib::ConvertSamplerFilter(s.value("magFilter", "Linear"));
                param.samplerDesc.minFilter = AssetLib::ConvertSamplerFilter(s.value("minFilter", "Linear"));
                param.samplerDesc.addressModeU = AssetLib::ConvertAddressMode(s.value("addressModeU", "Repeat"));
                param.samplerDesc.addressModeV = AssetLib::ConvertAddressMode(s.value("addressModeV", "Repeat"));
                param.samplerDesc.addressModeW = AssetLib::ConvertAddressMode(s.value("addressModeW", "Repeat"));
                param.samplerDesc.anisotropy = s.value("anisotropy", 1.0f);
                param.samplerDesc.minLod = s.value("minLod", 0.0f);
                param.samplerDesc.maxLod = s.value("maxLod", 16.0f);
            }
            else {
                throw std::runtime_error("SeparateSampler requires sampler settings for: " + key);
            }
            param.dataSize = 0; // Sampler nie ma osobnych danych
        }
        else {
            throw std::runtime_error("Unsupported descriptor type for: " + key);
        }

        params.push_back(param);
    }

    // Ustaw brakujące pola w MaterialInfo
    matInfo.parameterCount = static_cast<uint32_t>(params.size());
    matInfo.dataSize = static_cast<uint32_t>(paramData.size());
    std::string filename = std::filesystem::path(inputPath).filename().string();

    // Utwórz dane zasobu przy użyciu nowych funkcji AssetLib
    AssetLib::AssetData assetData = AssetLib::WriteMaterial(
        filename,
        matInfo,
        params,
        paramData,
        settings.compressionLevel > 0 ? AssetLib::CompressionType::LZ4 : AssetLib::CompressionType::None,
        settings.compressionLevel
    );

    return assetData;
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


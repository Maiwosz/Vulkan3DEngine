#include "pch.h"
#include "AssetLib.h"
#include "lz4.h"
#include <stdexcept>
#include <string>
#include <fstream>

using json = nlohmann::json;

namespace AssetLib {

    namespace {
        constexpr uint32_t CURRENT_VERSION = 1;
        constexpr std::array<char, 4> EXPECTED_MAGIC{ 'A', 'S', 'E', 'T' };
    
        json SerializeTextureInfo(const TextureInfo& info) {
            return {
                {"format", static_cast<uint8_t>(info.format)},
                {"width", info.width},
                {"height", info.height},
                {"mipLevels", info.mipLevels},
                {"flags", info.flags}
            };
        }

        TextureInfo DeserializeTextureInfo(const json& j) {
            TextureInfo info;
            info.format = static_cast<TextureFormat>(j["format"].get<uint8_t>());
            if (info.format > TextureFormat::BC5) {
                throw std::runtime_error("Invalid texture format");
            }
            info.width = j["width"].get<uint32_t>();
            info.height = j["height"].get<uint32_t>();
            info.mipLevels = j["mipLevels"].get<uint32_t>();
            info.flags = j["flags"].get<uint32_t>();
            return info;
        }

        json SerializeMeshInfo(const MeshInfo& info) {
            return {
                {"vertexCount", info.vertexCount},
                {"indexCount", info.indexCount},
                {"vertexStride", info.vertexStride},
                {"attributes", info.attributes},
                {"indexType", info.indexType}
            };
        }

        MeshInfo DeserializeMeshInfo(const json& j) {
            MeshInfo info;
            info.vertexCount = j["vertexCount"].get<uint32_t>();
            info.indexCount = j["indexCount"].get<uint32_t>();
            info.vertexStride = j["vertexStride"].get<uint32_t>();
            info.attributes = j["attributes"].get<uint32_t>();
            info.indexType = j["indexType"].get<uint8_t>();
            return info;
        }

        json SerializeMaterialInfo(const MaterialInfo& info) {
            return {
                {"shaderName", std::string(info.shaderName.data())},
                {"parameterCount", info.parameterCount},
                {"dataSize", info.dataSize}
            };
        }

        MaterialInfo DeserializeMaterialInfo(const json& j) {
            MaterialInfo info;
            std::string name = j["shaderName"];
            std::copy_n(name.c_str(), std::min(name.size(), info.shaderName.size()), info.shaderName.data());
            info.parameterCount = j["parameterCount"].get<uint32_t>();
            info.dataSize = j["dataSize"].get<uint32_t>();
            return info;
        }

        json SerializeShaderStageInfo(const ShaderStageInfo& stage) {
            return {
                {"stage", static_cast<uint8_t>(stage.stage)},
                {"spirvVersion", stage.spirvVersion},
                {"spirvSize", stage.spirvCode.size()},
                {"reflectionDataSize", stage.reflectionData.size()}
            };
        }

        // Deserializacja pojedynczego etapu shadera z JSON
        ShaderStageInfo DeserializeShaderStageInfo(const json& j) {
            ShaderStageInfo stage;
            stage.stage = static_cast<ShaderStage>(j["stage"].get<uint8_t>());
            stage.spirvVersion = j["spirvVersion"].get<uint32_t>();
            return stage;
        }
    }

    bool ValidateHeader(const Header& header) {
        return header.magic == EXPECTED_MAGIC &&
            header.version <= CURRENT_VERSION &&
            static_cast<uint8_t>(header.assetType) <= 3 &&
            static_cast<uint8_t>(header.compression) <= 1;
    }

    std::vector<uint8_t> Decompress(const void* data, size_t size, size_t decompressedSize) {
        std::vector<uint8_t> output(decompressedSize);

        const int result = LZ4_decompress_safe(
            static_cast<const char*>(data),
            reinterpret_cast<char*>(output.data()),
            static_cast<int>(size),
            static_cast<int>(decompressedSize)
        );

        if (result < 0) {
            throw std::runtime_error("LZ4 decompression failed with error code: "
                + std::to_string(result));
        }

        if (static_cast<size_t>(result) != decompressedSize) {
            throw std::runtime_error("Decompressed size mismatch. Expected: "
                + std::to_string(decompressedSize)
                + ", got: " + std::to_string(result));
        }

        return output;
    }

    std::vector<uint8_t> Compress(const void* data, size_t size, CompressionType compressionType, int compressionLevel)
    {
        if (compressionType == CompressionType::LZ4)
        {
            const int maxCompressedSize = LZ4_compressBound(static_cast<int>(size));
            std::vector<uint8_t> compressed(maxCompressedSize);

            const int compressedSize = LZ4_compress_fast( 
                static_cast<const char*>(data),
                reinterpret_cast<char*>(compressed.data()),
                static_cast<int>(size),
                maxCompressedSize,
                compressionLevel
            );

            if (compressedSize <= 0) {
                throw std::runtime_error("LZ4 compression failed");
            }

            compressed.resize(compressedSize);
            return compressed;
        }
        else if (compressionType == CompressionType::None) {
            return std::vector<uint8_t>(static_cast<const uint8_t*>(data),
                static_cast<const uint8_t*>(data) + size);
        }

        throw std::runtime_error("Unsupported compression type");
    }

    void WriteAsset(const std::string& path, const AssetData& assetData) {
        // Walidacja nagłówka
        if (!ValidateHeader(assetData.header)) {
            throw std::runtime_error("Invalid asset header");
        }

        // Serializacja metadanych do msgpack
        std::vector<uint8_t> msgpackData;
        if (!assetData.metadata.is_null()) {
            msgpackData = nlohmann::json::to_msgpack(assetData.metadata);
        }

        // Aktualizacja rozmiarów w nagłówku
        Header header = assetData.header; // kopia do modyfikacji
        header.metadataSize = static_cast<uint32_t>(msgpackData.size());
        header.dataSize = assetData.compressedData.size();

        // Zapis do pliku
        std::ofstream file(path, std::ios::binary);
        if (!file.good()) {
            throw std::runtime_error("Failed to open file: " + path);
        }

        // Nagłówek
        file.write(reinterpret_cast<const char*>(&header), sizeof(Header));

        // Metadane (msgpack)
        if (!msgpackData.empty()) {
            file.write(reinterpret_cast<const char*>(msgpackData.data()), msgpackData.size());
        }

        // Dane
        if (!assetData.compressedData.empty()) {
            file.write(reinterpret_cast<const char*>(assetData.compressedData.data()), assetData.compressedData.size());
        }

        if (!file.good()) {
            throw std::runtime_error("Error during file write: " + path);
        }
    }

    AssetData ReadAsset(const std::string& path) {
        AssetData result;

        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.good()) {
            throw std::runtime_error("Failed to open file: " + path);
        }

        // Wczytaj nagłówek
        file.seekg(0);
        file.read(reinterpret_cast<char*>(&result.header), sizeof(Header));

        if (!ValidateHeader(result.header)) {
            throw std::runtime_error("Invalid asset header in file: " + path);
        }

        // Wczytaj metadane (msgpack)
        std::vector<uint8_t> msgpackData(result.header.metadataSize);
        if (result.header.metadataSize > 0) {
            file.read(reinterpret_cast<char*>(msgpackData.data()), msgpackData.size());
            result.metadata = nlohmann::json::from_msgpack(msgpackData);
        }

        // Wczytaj dane
        result.compressedData.resize(result.header.dataSize);
        if (result.header.dataSize > 0) {
            file.read(reinterpret_cast<char*>(result.compressedData.data()), result.compressedData.size());
        }

        return result;
    }

    AssetData WriteTexture(const std::string& source, const TextureInfo& info, const std::vector<uint8_t>& pixelData, CompressionType compression, int compressionLevel) {
        AssetData asset;
        asset.header.assetType = AssetType::Texture;
        asset.header.compression = compression;
        asset.header.decompressedSize = pixelData.size();
        asset.metadata["source"] = source;
        asset.metadata["texture"] = SerializeTextureInfo(info);
        asset.compressedData = Compress(pixelData.data(), pixelData.size(), compression, compressionLevel);
        
        return asset;
    }

    std::pair<TextureInfo, std::vector<uint8_t>> ReadTexture(const AssetData asset) {
        if (asset.header.assetType != AssetType::Texture) {
            throw std::runtime_error("Not a texture asset");
        }

        TextureInfo info = DeserializeTextureInfo(asset.metadata["texture"]);
        std::vector<uint8_t> data = Decompress(asset.compressedData.data(), asset.compressedData.size(), asset.header.decompressedSize);

        return { info, data };
    }

    AssetData WriteMesh(const std::string& source, const MeshInfo& info, const std::vector<uint8_t>& vertexData, const std::vector<uint8_t>& indexData, CompressionType compression, int compressionLevel) {
        // Walidacja rozmiarów danych
        if (vertexData.size() != info.vertexCount * info.vertexStride) {
            throw std::invalid_argument("Vertex data size mismatch");
        }

        const size_t indexSize = info.indexType == 0 ? sizeof(uint16_t) : sizeof(uint32_t);
        if (indexData.size() != info.indexCount * indexSize) {
            throw std::invalid_argument("Index data size mismatch");
        }

        // Łączenie danych
        std::vector<uint8_t> combined;
        combined.reserve(vertexData.size() + indexData.size());
        combined.insert(combined.end(), vertexData.begin(), vertexData.end());
        combined.insert(combined.end(), indexData.begin(), indexData.end());

        AssetData asset;
        asset.header.assetType = AssetType::Mesh;
        asset.header.compression = compression;
        asset.header.decompressedSize = combined.size();
        asset.metadata["source"] = source;
        asset.metadata.update(SerializeMeshInfo(info));
        asset.compressedData = Compress(combined.data(), combined.size(), compression, compressionLevel);

        return asset;
    }

    std::tuple<MeshInfo, std::vector<uint8_t>, std::vector<uint8_t>> ReadMesh(const AssetData asset) {
        if (asset.header.assetType != AssetType::Mesh) {
            throw std::runtime_error("Not a mesh asset");
        }

        MeshInfo info = DeserializeMeshInfo(asset.metadata["mesh"]);
        std::vector<uint8_t> data = Decompress(asset.compressedData.data(), asset.compressedData.size(), asset.header.decompressedSize);

        // Rozdzielenie danych
        const size_t vertexSize = info.vertexCount * info.vertexStride;
        if (data.size() < vertexSize) {
            throw std::runtime_error("Invalid mesh data size");
        }

        std::vector<uint8_t> vertexData(data.begin(), data.begin() + vertexSize);
        std::vector<uint8_t> indexData(data.begin() + vertexSize, data.end());

        return { info, vertexData, indexData };
    }

    AssetData WriteMaterial(const std::string& source, const MaterialInfo& info, const std::vector<MaterialParameter>& parameters, const std::vector<uint8_t>& parameterData, CompressionType compression, int compressionLevel) {
        if (parameters.size() != info.parameterCount) {
            throw std::invalid_argument("Parameter count mismatch");
        }

        if (parameterData.size() != info.dataSize) {
            throw std::invalid_argument("Parameter data size mismatch");
        }

        // Serializacja parametrów
        std::vector<uint8_t> paramBytes;
        paramBytes.reserve(parameters.size() * sizeof(MaterialParameter));
        for (const auto& param : parameters) {
            const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&param);
            paramBytes.insert(paramBytes.end(), ptr, ptr + sizeof(MaterialParameter));
        }

        // Łączenie danych
        std::vector<uint8_t> combined;
        combined.reserve(paramBytes.size() + parameterData.size());
        combined.insert(combined.end(), paramBytes.begin(), paramBytes.end());
        combined.insert(combined.end(), parameterData.begin(), parameterData.end());

        AssetData asset;
        asset.header.assetType = AssetType::Material;
        asset.header.compression = compression;
        asset.header.decompressedSize = combined.size();
        asset.metadata["source"] = source;
        asset.metadata.update(SerializeMaterialInfo(info));
        asset.compressedData = Compress(combined.data(), combined.size(), compression, compressionLevel);

		return asset;
    }

    std::tuple<MaterialInfo, std::vector<MaterialParameter>, std::vector<uint8_t>> ReadMaterial(const AssetData asset) {
        if (asset.header.assetType != AssetType::Material) {
            throw std::runtime_error("Not a material asset");
        }

        MaterialInfo info = DeserializeMaterialInfo(asset.metadata["material"]);
        std::vector<uint8_t> data = Decompress(asset.compressedData.data(), asset.compressedData.size(), asset.header.decompressedSize);

        // Deserializacja parametrów
        const size_t paramSize = info.parameterCount * sizeof(MaterialParameter);
        if (data.size() < paramSize) {
            throw std::runtime_error("Invalid material data size");
        }

        std::vector<MaterialParameter> parameters(info.parameterCount);
        memcpy(parameters.data(), data.data(), paramSize);

        // Pozostałe dane
        std::vector<uint8_t> paramData(data.begin() + paramSize, data.end());

        return { info, parameters, paramData };
    }

    AssetData WriteShader(const std::string& source, const std::vector<ShaderStageInfo>& stages, uint32_t flags, CompressionType compression, int compressionLevel) {
        std::vector<uint8_t> combined;
        for (const auto& stage : stages) {
            combined.insert(combined.end(), stage.spirvCode.begin(), stage.spirvCode.end());
            combined.insert(combined.end(), stage.reflectionData.begin(), stage.reflectionData.end());
        }

        AssetData asset;
        asset.header.assetType = AssetType::Shader;
        asset.header.compression = compression;
        asset.header.decompressedSize = combined.size();
        asset.metadata["source"] = source;
        asset.metadata["flags"] = flags;

        // Serializacja wszystkich etapów przy użyciu funkcji pomocniczej
        json stagesJson = json::array();
        for (const auto& stage : stages) {
            stagesJson.push_back(SerializeShaderStageInfo(stage));
        }
        asset.metadata["stages"] = stagesJson;

        asset.compressedData = Compress(combined.data(), combined.size(), compression, compressionLevel);
        return asset;
    }

    std::tuple<std::vector<ShaderStageInfo>, std::string, uint32_t> ReadShader(const AssetData& asset) {
        if (asset.header.assetType != AssetType::Shader) {
            throw std::runtime_error("Not a shader asset");
        }

        std::string source = asset.metadata["source"];
        uint32_t flags = asset.metadata["flags"].get<uint32_t>();
        const json& stagesJson = asset.metadata["stages"];

        std::vector<ShaderStageInfo> stages;
        std::vector<uint8_t> data = Decompress(asset.compressedData.data(), asset.compressedData.size(), asset.header.decompressedSize);

        size_t dataOffset = 0;
        for (const auto& stageJson : stagesJson) {
            ShaderStageInfo stage = DeserializeShaderStageInfo(stageJson);

            // Wczytaj SPIR-V i reflection data na podstawie rozmiarów z JSON
            uint32_t spirvSize = stageJson["spirvSize"].get<uint32_t>();
            uint32_t reflectionSize = stageJson["reflectionDataSize"].get<uint32_t>();

            stage.spirvCode.assign(data.begin() + dataOffset, data.begin() + dataOffset + spirvSize);
            dataOffset += spirvSize;

            stage.reflectionData.assign(data.begin() + dataOffset, data.begin() + dataOffset + reflectionSize);
            dataOffset += reflectionSize;

            stages.push_back(stage);
        }

        return { stages, source, flags };
    }

}
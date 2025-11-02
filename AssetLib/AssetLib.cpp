#include "pch.h"
#include "AssetLib.h"
#include "lz4.h"
#include <stdexcept>
#include <string>
#include <fstream>
#include <Serialization.h>

using json = nlohmann::json;

namespace AssetLib {

    namespace {
        constexpr uint32_t CURRENT_VERSION = 1;
        constexpr std::array<char, 4> EXPECTED_MAGIC{ 'A', 'S', 'E', 'T' };
    
        json SerializeMipLevel(const MipLevel& mip) {
            return {
                {"width", mip.width},
                {"height", mip.height},
                {"dataOffset", mip.dataOffset},
                {"dataSize", mip.dataSize}
            };
        }

        MipLevel DeserializeMipLevel(const json& j) {
            MipLevel mip;
            mip.width = j["width"].get<uint32_t>();
            mip.height = j["height"].get<uint32_t>();
            mip.dataOffset = j["dataOffset"].get<uint32_t>();
            mip.dataSize = j["dataSize"].get<uint32_t>();
            return mip;
        }

        json SerializeTextureInfo(const TextureInfo& info) {
            json j = {
                {"format", static_cast<uint8_t>(info.format)},
                {"width", info.width},
                {"height", info.height},
                {"mipLevels", info.mipLevels},
                {"flags", info.flags}
            };

            // Serializacja informacji o każdym poziomie mipmapy
            json mipsArray = json::array();
            for (const auto& mip : info.mips) {
                mipsArray.push_back(SerializeMipLevel(mip));
            }
            j["mips"] = std::move(mipsArray);

            return j;
        }

        TextureInfo DeserializeTextureInfo(const json& j) {
            TextureInfo info;
            info.format = static_cast<TextureFormat>(j["format"].get<uint8_t>());
            info.width = j["width"].get<uint32_t>();
            info.height = j["height"].get<uint32_t>();
            info.mipLevels = j["mipLevels"].get<uint32_t>();
            info.flags = j["flags"].get<uint32_t>();

            // Deserializacja informacji o każdym poziomie mipmapy
            const json& mipsArray = j["mips"];
            info.mips.reserve(info.mipLevels);
            for (const auto& mipJson : mipsArray) {
                info.mips.push_back(DeserializeMipLevel(mipJson));
            }

            return info;
        }

        json SerializeVertexAttributeDesc(const VertexAttributeDesc& desc) {
            return {
                {"type", static_cast<uint32_t>(desc.type)},
                {"offset", desc.offset},
                {"componentCount", desc.componentCount},
                {"componentSize", desc.componentSize}
            };
        }

        VertexAttributeDesc DeserializeVertexAttributeDesc(const json& j) {
            VertexAttributeDesc desc;
            desc.type = static_cast<VertexAttribute>(j["type"].get<uint32_t>());
            desc.offset = j["offset"].get<uint32_t>();
            desc.componentCount = j["componentCount"].get<uint8_t>();
            desc.componentSize = j["componentSize"].get<uint8_t>();
            return desc;
        }

        json SerializeMeshInfo(const MeshInfo& info) {
            json j = {
                {"vertexCount", info.vertexCount},
                {"indexCount", info.indexCount},
                {"vertexStride", info.vertexStride},
                {"attributes", info.attributes},
                {"indexType", info.indexType}
            };

            // Serializuj layout atrybutów
            json layoutArray = json::array();
            for (const auto& attr : info.attributeLayout) {
                layoutArray.push_back(SerializeVertexAttributeDesc(attr));
            }
            j["attributeLayout"] = layoutArray;

            return j;
        }

        MeshInfo DeserializeMeshInfo(const json& j) {
            MeshInfo info;
            info.vertexCount = j["vertexCount"].get<uint32_t>();
            info.indexCount = j["indexCount"].get<uint32_t>();
            info.vertexStride = j["vertexStride"].get<uint32_t>();
            info.attributes = j["attributes"].get<uint32_t>();
            info.indexType = j["indexType"].get<uint8_t>();

            // Deserializuj layout atrybutów
            if (j.contains("attributeLayout")) {
                for (const auto& attrJson : j["attributeLayout"]) {
                    info.attributeLayout.push_back(
                        DeserializeVertexAttributeDesc(attrJson)
                    );
                }
            }

            return info;
        }

        json SerializePrefabInfo(const PrefabInfo& info) {
            return {
                {"entityCount", info.entityCount}
            };
        }

        PrefabInfo DeserializePrefabInfo(const json& j) {
            PrefabInfo info;
            info.entityCount = j["entityCount"].get<uint32_t>();
            return info;
        }

        json SerializeSceneInfo(const SceneInfo& info) {
            return {
                {"entityCount", info.entityCount}
            };
        }

        SceneInfo DeserializeSceneInfo(const json& j) {
            SceneInfo info;
            info.entityCount = j["entityCount"].get<uint32_t>();
            return info;
        }

        // Serializacja/Deserializacja RenderGraphInfo
        json SerializeRenderGraphInfo(const RenderGraphInfo& info) {
            return {
                {"nodeCount", info.nodeCount},
                {"connectionCount", info.connectionCount}
            };
        }

        RenderGraphInfo DeserializeRenderGraphInfo(const json& j) {
            RenderGraphInfo info;
            info.nodeCount = j["nodeCount"].get<uint32_t>();
            info.connectionCount = j["connectionCount"].get<uint32_t>();
            return info;
        }
    }

    bool ValidateHeader(const Header& header) {
        return header.magic == EXPECTED_MAGIC &&
            header.version <= CURRENT_VERSION &&
            static_cast<uint8_t>(header.assetType) <= 5 &&
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

    std::pair<TextureInfo, std::vector<uint8_t>> ReadTexture(const AssetData& asset) {
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

    std::tuple<MeshInfo, std::vector<uint8_t>, std::vector<uint8_t>> ReadMesh(const AssetData& asset) {
        if (asset.header.assetType != AssetType::Mesh) {
            throw std::runtime_error("Not a mesh asset");
        }

        MeshInfo info = DeserializeMeshInfo(asset.metadata);
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

    AssetData WriteShader(const std::string& source, const ShaderLib::ShaderData shaderData, CompressionType compression, int compressionLevel) {
     
        std::vector<uint8_t> shaderDataBinary = ShaderLib::SerializeStages(shaderData.stages);

        AssetData asset;
        asset.header.assetType = AssetType::Shader;
        asset.header.compression = compression;
        asset.header.decompressedSize = shaderDataBinary.size();
        asset.metadata["source"] = source;
        asset.metadata["shaderData"] = shaderData.metadata;

        asset.compressedData = Compress(shaderDataBinary.data(), shaderDataBinary.size(), compression, compressionLevel);
        return asset;
    }

    std::tuple<ShaderLib::ShaderMetadata, std::vector<ShaderLib::CompiledStage>> ReadShader(const AssetData& asset) {
        if (asset.header.assetType != AssetType::Shader) {
            throw std::runtime_error("Not a shader asset");
        }

        std::string source = asset.metadata["source"];
        ShaderLib::ShaderMetadata metadata = ShaderLib::DeserializeMetadata(asset.metadata["shaderData"]) ;


        std::vector<uint8_t> data = Decompress(asset.compressedData.data(), asset.compressedData.size(), asset.header.decompressedSize);

        std::vector<ShaderLib::CompiledStage> stages = ShaderLib::DeserializeStages(data);

        size_t dataOffset = 0;

        return { metadata, stages };
    }

    AssetData WritePrefab(
        const std::string& sourceName,
        const PrefabInfo& info,
        const nlohmann::json& prefabData,
        CompressionType compression) {

        AssetData asset;
        asset.header.assetType = AssetType::Prefab;
        asset.header.compression = compression;

        // Serializacja JSON do string, potem do binary
        std::string jsonString = prefabData.dump();
        std::vector<uint8_t> jsonBytes(jsonString.begin(), jsonString.end());

        asset.header.decompressedSize = jsonBytes.size();
        asset.metadata["source"] = sourceName.empty() ? "generated_prefab" : sourceName;
        asset.metadata["prefab"] = SerializePrefabInfo(info);
        asset.compressedData = Compress(jsonBytes.data(), jsonBytes.size(), compression);

        return asset;
    }

    std::pair<PrefabInfo, nlohmann::json> ReadPrefab(const AssetData& asset) {
        if (asset.header.assetType != AssetType::Prefab) {
            throw std::runtime_error("Not a prefab asset");
        }

        PrefabInfo info = DeserializePrefabInfo(asset.metadata["prefab"]);

        std::vector<uint8_t> data;

        // Sprawdź typ kompresji z nagłówka
        if (asset.header.compression == CompressionType::None) {
            data = asset.compressedData;
        }
        else if (asset.header.compression == CompressionType::LZ4) {
            data = Decompress(asset.compressedData.data(),
                asset.compressedData.size(),
                asset.header.decompressedSize);
        }
        else {
            throw std::runtime_error("Unsupported compression type in prefab");
        }

        // Konwertuj bajty z powrotem na string i parsuj JSON
        std::string jsonString(data.begin(), data.end());
        nlohmann::json prefabData = nlohmann::json::parse(jsonString);

        return { info, prefabData };
    }

    AssetData WriteScene(
        const std::string& sourceName,
        const SceneInfo& info,
        const nlohmann::json& sceneData,
        CompressionType compression) {

        AssetData asset;
        asset.header.assetType = AssetType::Scene;
        asset.header.compression = compression;

        // Serializacja JSON do string, potem do binary
        std::string jsonString = sceneData.dump();
        std::vector<uint8_t> jsonBytes(jsonString.begin(), jsonString.end());

        asset.header.decompressedSize = jsonBytes.size();
        asset.metadata["source"] = sourceName.empty() ? "generated_scene" : sourceName;
        asset.metadata["scene"] = SerializeSceneInfo(info);

        asset.compressedData = Compress(jsonBytes.data(), jsonBytes.size(), compression);

        return asset;
    }

    std::pair<SceneInfo, nlohmann::json> ReadScene(const AssetData& asset) {
        if (asset.header.assetType != AssetType::Scene) {
            throw std::runtime_error("Not a scene asset");
        }

        SceneInfo info = DeserializeSceneInfo(asset.metadata["scene"]);

        std::vector<uint8_t> data;

        // Sprawdź typ kompresji z nagłówka
        if (asset.header.compression == CompressionType::None) {
            data = asset.compressedData;
        }
        else if (asset.header.compression == CompressionType::LZ4) {
            data = Decompress(asset.compressedData.data(),
                asset.compressedData.size(),
                asset.header.decompressedSize);
        }
        else {
            throw std::runtime_error("Unsupported compression type in scene");
        }

        // Konwertuj bajty z powrotem na string i parsuj JSON
        std::string jsonString(data.begin(), data.end());
        nlohmann::json sceneData = nlohmann::json::parse(jsonString);

        return { info, sceneData };
    }

    AssetData WriteRenderGraph(
        const std::string& sourceName,
        const RenderGraphInfo& info,
        const nlohmann::json& graphData,
        CompressionType compression) {

        AssetData asset;
        asset.header.assetType = AssetType::RenderGraph;
        asset.header.compression = compression;

        // Serializacja JSON do string, potem do binary
        std::string jsonString = graphData.dump();
        std::vector<uint8_t> jsonBytes(jsonString.begin(), jsonString.end());

        asset.header.decompressedSize = jsonBytes.size();

        // Metadata
        asset.metadata["source"] = sourceName.empty() ? "generated_graph" : sourceName;
        asset.metadata["renderGraph"] = SerializeRenderGraphInfo(info);

        // Kompresja danych
        asset.compressedData = Compress(jsonBytes.data(), jsonBytes.size(), compression);

        return asset;
    }

    std::pair<RenderGraphInfo, nlohmann::json> ReadRenderGraph(const AssetData& asset) {
        if (asset.header.assetType != AssetType::RenderGraph) {
            throw std::runtime_error("Not a render graph asset");
        }

        RenderGraphInfo info = DeserializeRenderGraphInfo(asset.metadata["renderGraph"]);

        std::vector<uint8_t> data;

        // Dekompresja jeśli potrzebna
        if (asset.header.compression == CompressionType::None) {
            data = asset.compressedData;
        }
        else if (asset.header.compression == CompressionType::LZ4) {
            data = Decompress(asset.compressedData.data(),
                asset.compressedData.size(),
                asset.header.decompressedSize);
        }
        else {
            throw std::runtime_error("Unsupported compression type in render graph");
        }

        // Konwertuj bajty z powrotem na string i parsuj JSON
        std::string jsonString(data.begin(), data.end());
        nlohmann::json graphData = nlohmann::json::parse(jsonString);

        return { info, graphData };
    }
}
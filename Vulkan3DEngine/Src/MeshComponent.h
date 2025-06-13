#pragma once
#include "Component.h"
#include "AssetHandle.h"
#include "BinaryWriter.h"

struct MeshComponent : public Component {
public:
    void setMesh(AssetHandle mesh) {
        m_mesh = mesh;
        incrementVersion();
    }

    AssetHandle getMesh() {
        return m_mesh;
    }

    // ISerializable implementation
    json serialize() const override {
        json j;
        j["assetType"] = static_cast<int>(m_mesh.type);
        j["filename"] = m_mesh.filename;
        return j;
    }

    void deserialize(const json& j) override {
        if (j.contains("assetType") && j["assetType"].is_number_integer() &&
            j.contains("filename") && j["filename"].is_string()) {
            m_mesh.type = static_cast<AssetType>(j["assetType"]);
            m_mesh.filename = j["filename"];
            incrementVersion();
        }
    }

    // IBinarySerializable implementation
    std::vector<uint8_t> serializeBinary() const override {
        BinaryWriter writer;

        // Write asset type
        writer.write(static_cast<uint32_t>(m_mesh.type));

        // Write filename
        writer.write(m_mesh.filename);

        return writer.getData();
    }

    size_t deserializeBinary(const uint8_t* data, size_t size) override {
        BinaryReader reader(data, size);

        uint32_t assetType;
        if (!reader.read(assetType)) return 0;

        std::string filename;
        if (!reader.read(filename)) return 0;

        m_mesh.type = static_cast<AssetType>(assetType);
        m_mesh.filename = filename;
        incrementVersion();

        return reader.getPosition();
    }

private:
    AssetHandle m_mesh;
};
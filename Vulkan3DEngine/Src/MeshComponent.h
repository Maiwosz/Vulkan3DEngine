#pragma once
#include "Component.h"
#include "AssetHandle.h"
#include "BinaryWriter.h"

struct MeshComponent : public Component {
public:
    const char* getName() const override {
        return "MeshComponent";
    }

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

private:
    AssetHandle m_mesh;
};
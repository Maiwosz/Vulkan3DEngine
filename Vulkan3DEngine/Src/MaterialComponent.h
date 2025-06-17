#pragma once
#include "Component.h"
#include "AssetHandle.h"
#include "BinaryWriter.h"

struct MaterialComponent : public Component {
public:
    const char* getName() const override {
        return "MaterialComponent";
    }

    void setMaterial(AssetHandle material) {
        m_material = material;
        incrementVersion();
    }

    AssetHandle getMaterial() {
        return m_material;
    }

    // ISerializable implementation
    json serialize() const override {
        json j;
        j["assetType"] = static_cast<int>(m_material.type);
        j["filename"] = m_material.filename;
        return j;
    }

    void deserialize(const json& j) override {
        if (j.contains("assetType") && j["assetType"].is_number_integer() &&
            j.contains("filename") && j["filename"].is_string()) {
            m_material.type = static_cast<AssetType>(j["assetType"]);
            m_material.filename = j["filename"];
            incrementVersion();
        }
    }

private:
    AssetHandle m_material;
};
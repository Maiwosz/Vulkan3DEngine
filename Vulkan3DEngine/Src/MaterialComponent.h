#pragma once
#include "Component.h"
#include "AssetHandle.h"
#include "BinaryWriter.h"
#include "AssetLoader.h"
#include "Paths.h"
#include <filesystem>
#include <vector>

// Forward declarations
class MaterialManager;

struct MaterialComponent : public Component {
public:
    const char* getName() const override {
        return "MaterialComponent";
    }

    void setMaterial(AssetHandle material);
    AssetHandle getMaterial() const;

    // ISerializable implementation
    json serialize() const override;
    void deserialize(const json& j) override;
    void renderUI() override;

private:
    AssetHandle m_material;

    std::vector<std::string> getAvailableMaterialFiles();
    void renderMaterialParametersUI();
};
#pragma once
#include "Component.h"
#include "AssetHandle.h"
#include <vector>
#include <string>

// Forward declarations
class Material;

/**
 * MaterialComponent - Assigns material to an entity
 *
 * Simplified component that:
 * - Stores material asset handle
 * - Provides UI for material selection
 * - Provides UI for material parameter editing via FieldProxy
 */
struct MaterialComponent : public Component {
public:
    const char* getName() const override {
        return "MaterialComponent";
    }

    // Material management
    void setMaterial(AssetHandle material);
    AssetHandle getMaterial() const;

    // ISerializable implementation
    json serialize() const override;
    void deserialize(const json& j) override;
    void renderUI() override;

private:
    AssetHandle m_material;

    // UI helpers
    std::vector<std::string> getAvailableMaterialFiles();
    void renderMaterialSelectionUI();
    void renderMaterialParametersUI();

    // Field UI rendering
    void renderFieldUI(const std::string& fieldName, Material* material);
    bool renderBaseTypeUI(const std::string& label, ShaderLib::BaseType type,
        ShaderLib::FieldProxy& proxy);

    // Texture UI rendering
    void renderTextureUI(const std::string& name, Material* material);
};

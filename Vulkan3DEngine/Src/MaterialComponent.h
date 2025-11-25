#pragma once
#include "Component.h"
#include "AssetHandle.h"
#include <vector>
#include <string>
#include <FieldProxy.h>

// Forward declarations
class Material;

/**
 * MaterialComponent - Assigns material to an entity
 *
 * Refactored to work with named buffers:
 * - Displays each buffer separately in UI
 * - Shows buffer fields in hierarchical structure
 * - Handles array fields properly
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

    // Buffer UI rendering
    void renderBufferUI(const std::string& bufferName, Material* material);
    void renderBufferFieldsUI(const std::string& bufferName, Material* material);

    // Field UI rendering (recursive for nested structures)
    void renderFieldUI(
        const std::string& fieldPath,
        ShaderLib::FieldProxy& fieldProxy,
        Material* material,
        int depth = 0
    );

    void renderStructureFieldUI(
        const std::string& fieldPath,
        ShaderLib::FieldProxy& fieldProxy,
        Material* material,
        int depth
    );

    void renderArrayFieldUI(
        const std::string& fieldPath,
        ShaderLib::FieldProxy& fieldProxy,
        Material* material,
        int depth
    );

    void renderBaseTypeFieldUI(
        const std::string& fieldPath,
        ShaderLib::FieldProxy& fieldProxy,
        Material* material
    );

    // Base type value UI
    bool renderBaseTypeValueUI(
        const std::string& label,
        ShaderLib::BaseType type,
        ShaderLib::FieldProxy& proxy
    );

    // Texture UI rendering
    void renderTexturesUI(Material* material);
    void renderTextureUI(const std::string& name, Material* material);

    // UI state for collapsible headers
    struct UIState {
        std::unordered_map<std::string, bool> expandedPaths;
    };
    mutable UIState m_uiState;
};

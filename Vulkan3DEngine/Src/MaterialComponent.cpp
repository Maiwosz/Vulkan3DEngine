#include "MaterialComponent.h"
#include "MaterialManager.h"
#include "Material.h"
#include "AssetManager.h"
#include "AssetLoader.h"
#include "Engine.h"
#include "Paths.h"
#include <imgui.h>
#include <algorithm>
#include <filesystem>

void MaterialComponent::setMaterial(AssetHandle material) {
    m_material = material;
    incrementVersion();
}

AssetHandle MaterialComponent::getMaterial() const {
    return m_material;
}

json MaterialComponent::serialize() const {
    json j;
    j["assetType"] = static_cast<int>(m_material.type);
    j["filename"] = m_material.filename;
    return j;
}

void MaterialComponent::deserialize(const json& j) {
    if (j.contains("assetType") && j["assetType"].is_number_integer() &&
        j.contains("filename") && j["filename"].is_string()) {
        m_material.type = static_cast<AssetType>(j["assetType"]);
        m_material.filename = j["filename"];
        incrementVersion();
    }
}

void MaterialComponent::renderUI() {
    ImGui::Text("Material Component");
    ImGui::Separator();

    renderMaterialSelectionUI();

    ImGui::Separator();

    renderMaterialParametersUI();
}

// =============================================================================
// MATERIAL SELECTION UI
// =============================================================================

void MaterialComponent::renderMaterialSelectionUI() {
    // Display current material info
    if (!m_material.filename.empty()) {
        ImGui::Text("File: %s", m_material.filename.c_str());
        ImGui::Text("Type: %s", std::string(m_material.GetTypeName()).c_str());
    }
    else {
        ImGui::TextDisabled("No material assigned");
    }

    // Get available material files
    std::vector<std::string> materialFiles = getAvailableMaterialFiles();

    if (materialFiles.empty()) {
        ImGui::TextDisabled("No material files found in assets directory");
        return;
    }

    // Find current selection index
    int currentSelection = -1;
    for (int i = 0; i < materialFiles.size(); i++) {
        if (materialFiles[i] == m_material.filename) {
            currentSelection = i;
            break;
        }
    }

    // Create combo items
    std::vector<const char*> items;
    items.push_back("None");
    for (const auto& file : materialFiles) {
        items.push_back(file.c_str());
    }

    int comboSelection = currentSelection + 1;

    if (ImGui::Combo("Material File", &comboSelection, items.data(), items.size())) {
        if (comboSelection == 0) {
            setMaterial(AssetHandle());
        }
        else {
            AssetHandle newHandle(AssetType::Material, materialFiles[comboSelection - 1]);
            setMaterial(newHandle);
        }
    }
}

std::vector<std::string> MaterialComponent::getAvailableMaterialFiles() {
    std::vector<std::string> files;

    try {
        std::string materialDir = std::string(ASSETS_COMP) +
            AssetLoader::GetAssetSubdirectory(AssetType::Material);

        if (std::filesystem::exists(materialDir) &&
            std::filesystem::is_directory(materialDir)) {
            for (const auto& entry : std::filesystem::directory_iterator(materialDir)) {
                if (entry.is_regular_file()) {
                    files.push_back(entry.path().stem().string());
                }
            }
        }
    }
    catch (const std::filesystem::filesystem_error&) {
        // Handle silently in UI context
    }

    std::sort(files.begin(), files.end());
    return files;
}

// =============================================================================
// MATERIAL PARAMETERS UI
// =============================================================================

void MaterialComponent::renderMaterialParametersUI() {
    if (m_material.filename.empty()) {
        ImGui::TextDisabled("No material selected");
        return;
    }

    AssetManager& assetManager = getEngine()->assetSystem().assetManager();
    MaterialManager& materialManager = getEngine()->assetSystem().materialManager();

    // Ensure material is loaded
    assetManager.ensureReady(m_material);

    if (!materialManager.isAssetReady(m_material.filename)) {
        ImGui::TextDisabled("Material loading...");
        return;
    }

    // Get material
    MaterialHandle materialHandle = materialManager.getHandle<MaterialHandle>(m_material.filename);
    if (!materialHandle.isValid()) {
        ImGui::TextDisabled("Invalid material handle");
        return;
    }

    Material* material = materialManager.getMaterial(materialHandle);
    if (!material) {
        ImGui::TextDisabled("Material not found");
        return;
    }

    // Display shader info
    ImGui::Text("Shader: %s", material->GetName().c_str());
    ImGui::Separator();

    // Buffer Fields
    if (ImGui::CollapsingHeader("Buffer Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
        std::vector<std::string> fieldNames = material->GetFieldNames();

        if (fieldNames.empty()) {
            ImGui::TextDisabled("No buffer parameters");
        }
        else {
            for (const auto& fieldName : fieldNames) {
                renderFieldUI(fieldName, material);
            }
        }
    }

    // Texture Samplers
    if (ImGui::CollapsingHeader("Textures", ImGuiTreeNodeFlags_DefaultOpen)) {
        std::vector<std::string> textureNames = material->GetTextureNames();

        if (textureNames.empty()) {
            ImGui::TextDisabled("No texture parameters");
        }
        else {
            for (const auto& textureName : textureNames) {
                renderTextureUI(textureName, material);
            }
        }
    }
}

// =============================================================================
// FIELD UI RENDERING
// =============================================================================

void MaterialComponent::renderFieldUI(const std::string& fieldName, Material* material) {
    try {
        // Get buffer containing the field
        auto buffer = material->GetBufferForField(fieldName);
        if (!buffer) {
            ImGui::TextDisabled("%s: Buffer not found", fieldName.c_str());
            return;
        }

        // Get field descriptor through FindField (O(1) lookup)
        auto bufferDef = buffer->GetDefinition();
        const ShaderLib::FieldDescriptor* fieldDesc = bufferDef->FindField(fieldName);

        if (!fieldDesc) {
            ImGui::TextDisabled("%s: Field not found", fieldName.c_str());
            return;
        }

        // Skip non-base types (structures)
        if (!fieldDesc->isBaseType) {
            ImGui::TextDisabled("%s: Structure type (not editable)", fieldName.c_str());
            return;
        }

        // Get field proxy for value access
        auto fieldProxy = (*material)[fieldName];

        ImGui::PushID(fieldName.c_str());
        ImGui::Text("%s:", fieldName.c_str());
        ImGui::SameLine();
        ImGui::PushItemWidth(200.0f);

        // Render UI based on base type
        bool changed = renderBaseTypeUI("##value", fieldDesc->baseType, fieldProxy);

        ImGui::PopItemWidth();
        ImGui::PopID();

        if (changed) {
            // Sync field to GPU buffer
            material->SyncToGPU();
            incrementVersion();
        }
    }
    catch (const std::exception& e) {
        ImGui::TextDisabled("%s: Error (%s)", fieldName.c_str(), e.what());
    }
}

bool MaterialComponent::renderBaseTypeUI(const std::string& label,
    ShaderLib::BaseType type,
    ShaderLib::FieldProxy& proxy) {
    using namespace ShaderLib;

    bool changed = false;

    switch (type) {
    case BaseType::Bool: {
        bool value = proxy;
        if (ImGui::Checkbox(label.c_str(), &value)) {
            proxy = value;
            changed = true;
        }
        break;
    }

    case BaseType::Float: {
        float value = proxy;
        if (ImGui::DragFloat(label.c_str(), &value, 0.01f)) {
            proxy = value;
            changed = true;
        }
        break;
    }

    case BaseType::Vec2: {
        glm::vec2 value = proxy;
        if (ImGui::DragFloat2(label.c_str(), &value.x, 0.01f)) {
            proxy = value;
            changed = true;
        }
        break;
    }

    case BaseType::Vec3: {
        glm::vec3 value = proxy;
        if (ImGui::DragFloat3(label.c_str(), &value.x, 0.01f)) {
            proxy = value;
            changed = true;
        }
        break;
    }

    case BaseType::Vec4: {
        glm::vec4 value = proxy;
        if (ImGui::DragFloat4(label.c_str(), &value.x, 0.01f)) {
            proxy = value;
            changed = true;
        }
        break;
    }

    case BaseType::Int: {
        int32_t value = proxy;
        if (ImGui::DragInt(label.c_str(), &value)) {
            proxy = value;
            changed = true;
        }
        break;
    }

    case BaseType::IVec2: {
        glm::ivec2 value = proxy;
        if (ImGui::DragInt2(label.c_str(), &value.x)) {
            proxy = value;
            changed = true;
        }
        break;
    }

    case BaseType::IVec3: {
        glm::ivec3 value = proxy;
        if (ImGui::DragInt3(label.c_str(), &value.x)) {
            proxy = value;
            changed = true;
        }
        break;
    }

    case BaseType::IVec4: {
        glm::ivec4 value = proxy;
        if (ImGui::DragInt4(label.c_str(), &value.x)) {
            proxy = value;
            changed = true;
        }
        break;
    }

    case BaseType::UInt: {
        uint32_t value = proxy;
        int temp = static_cast<int>(value);
        if (ImGui::DragInt(label.c_str(), &temp, 1.0f, 0)) {
            proxy = static_cast<uint32_t>(std::max(0, temp));
            changed = true;
        }
        break;
    }

    case BaseType::UVec2: {
        glm::uvec2 value = proxy;
        int temp[2] = { static_cast<int>(value.x), static_cast<int>(value.y) };
        if (ImGui::DragInt2(label.c_str(), temp, 1.0f, 0)) {
            proxy = glm::uvec2(std::max(0, temp[0]), std::max(0, temp[1]));
            changed = true;
        }
        break;
    }

    case BaseType::UVec3: {
        glm::uvec3 value = proxy;
        int temp[3] = {
            static_cast<int>(value.x),
            static_cast<int>(value.y),
            static_cast<int>(value.z)
        };
        if (ImGui::DragInt3(label.c_str(), temp, 1.0f, 0)) {
            proxy = glm::uvec3(
                std::max(0, temp[0]),
                std::max(0, temp[1]),
                std::max(0, temp[2])
            );
            changed = true;
        }
        break;
    }

    case BaseType::UVec4: {
        glm::uvec4 value = proxy;
        int temp[4] = {
            static_cast<int>(value.x),
            static_cast<int>(value.y),
            static_cast<int>(value.z),
            static_cast<int>(value.w)
        };
        if (ImGui::DragInt4(label.c_str(), temp, 1.0f, 0)) {
            proxy = glm::uvec4(
                std::max(0, temp[0]),
                std::max(0, temp[1]),
                std::max(0, temp[2]),
                std::max(0, temp[3])
            );
            changed = true;
        }
        break;
    }

    case BaseType::Double: {
        double value = proxy;
        float temp = static_cast<float>(value);
        if (ImGui::DragFloat(label.c_str(), &temp, 0.01f)) {
            proxy = static_cast<double>(temp);
            changed = true;
        }
        break;
    }

    default:
        ImGui::TextDisabled("Unsupported type");
        break;
    }

    return changed;
}

// =============================================================================
// TEXTURE UI RENDERING
// =============================================================================

void MaterialComponent::renderTextureUI(const std::string& name, Material* material) {
    ImGui::PushID(name.c_str());

    Material::TextureParam texture;
    if (material->GetTexture(name, texture)) {
        ImGui::Text("%s:", name.c_str());

        if (!texture.assetHandle.filename.empty()) {
            ImGui::SameLine();
            ImGui::Text("%s", texture.assetHandle.filename.c_str());

            // Could add texture selection UI here
            // if (ImGui::Button("Change...")) { ... }
        }
        else {
            ImGui::SameLine();
            ImGui::TextDisabled("No texture assigned");
        }
    }

    ImGui::PopID();
}

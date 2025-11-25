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
    m_uiState.expandedPaths.clear();
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
    if (!m_material.filename.empty()) {
        ImGui::Text("File: %s", m_material.filename.c_str());
        ImGui::Text("Type: %s", std::string(m_material.GetTypeName()).c_str());
    }
    else {
        ImGui::TextDisabled("No material assigned");
    }

    std::vector<std::string> materialFiles = getAvailableMaterialFiles();

    if (materialFiles.empty()) {
        ImGui::TextDisabled("No material files found in assets directory");
        return;
    }

    int currentSelection = -1;
    for (int i = 0; i < materialFiles.size(); i++) {
        if (materialFiles[i] == m_material.filename) {
            currentSelection = i;
            break;
        }
    }

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

    assetManager.ensureReady(m_material);

    if (!materialManager.isAssetReady(m_material.filename)) {
        ImGui::TextDisabled("Material loading...");
        return;
    }

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

    // Render each buffer separately
    std::vector<std::string> bufferNames = material->GetBufferNames();

    if (!bufferNames.empty()) {
        if (ImGui::CollapsingHeader("Buffers", ImGuiTreeNodeFlags_DefaultOpen)) {
            for (const auto& bufferName : bufferNames) {
                renderBufferUI(bufferName, material);
            }
        }
    }

    // Render textures
    renderTexturesUI(material);
}

// =============================================================================
// BUFFER UI RENDERING
// =============================================================================

void MaterialComponent::renderBufferUI(const std::string& bufferName, Material* material) {
    ImGui::PushID(bufferName.c_str());

    bool isExpanded = ImGui::CollapsingHeader(bufferName.c_str());

    if (isExpanded) {
        ImGui::Indent();
        renderBufferFieldsUI(bufferName, material);
        ImGui::Unindent();
    }

    ImGui::PopID();
}

void MaterialComponent::renderBufferFieldsUI(const std::string& bufferName, Material* material) {
    auto bufferInstance = material->GetBuffer(bufferName);
    if (!bufferInstance) {
        ImGui::TextDisabled("Buffer not found");
        return;
    }

    auto layout = bufferInstance->GetLayout();
    if (!layout) {
        ImGui::TextDisabled("Invalid buffer layout");
        return;
    }

    // Display buffer info
    ImGui::TextDisabled("Size: %u bytes", layout->GetTotalSize());
    ImGui::TextDisabled("Alignment: %u", layout->GetAlignment());
    ImGui::Separator();

    // Get top-level field indices
    const auto& topLevelIndices = layout->GetTopLevelIndices();
    const auto& allFields = layout->GetAllFields();

    if (topLevelIndices.empty()) {
        ImGui::TextDisabled("No fields");
        return;
    }

    // Render each top-level field
    for (size_t index : topLevelIndices) {
        if (index >= allFields.size()) {
            continue;
        }

        const auto& fieldDesc = allFields[index];

        try {
            ShaderLib::FieldProxy fieldProxy = (*bufferInstance)[fieldDesc.name];
            renderFieldUI(fieldDesc.path, fieldProxy, material, 0);
        }
        catch (const std::exception& e) {
            ImGui::TextDisabled("%s: Error (%s)", fieldDesc.name.c_str(), e.what());
        }
    }
}

// =============================================================================
// FIELD UI RENDERING (HIERARCHICAL)
// =============================================================================

void MaterialComponent::renderFieldUI(
    const std::string& fieldPath,
    ShaderLib::FieldProxy& fieldProxy,
    Material* material,
    int depth
) {
    if (fieldProxy.IsArray()) {
        renderArrayFieldUI(fieldPath, fieldProxy, material, depth);
    }
    else if (fieldProxy.IsBaseType()) {
        renderBaseTypeFieldUI(fieldPath, fieldProxy, material);
    }
    else {
        renderStructureFieldUI(fieldPath, fieldProxy, material, depth);
    }
}

void MaterialComponent::renderStructureFieldUI(
    const std::string& fieldPath,
    ShaderLib::FieldProxy& fieldProxy,
    Material* material,
    int depth
) {
    ImGui::PushID(fieldPath.c_str());

    // Structure header
    std::string headerLabel = fieldProxy.GetName();
    bool isExpanded = ImGui::TreeNodeEx(
        headerLabel.c_str(),
        ImGuiTreeNodeFlags_DefaultOpen
    );

    if (isExpanded) {
        // Get buffer to find child fields
        auto bufferInstance = material->GetBuffer(material->GetBufferNames()[0]); // TODO: track which buffer
        auto layout = bufferInstance->GetLayout();

        // Get child paths
        std::vector<std::string> childPaths = layout->GetChildPaths(fieldPath);

        for (const auto& childPath : childPaths) {
            // Extract child name from path
            size_t lastDot = childPath.find_last_of('.');
            std::string childName = (lastDot != std::string::npos)
                ? childPath.substr(lastDot + 1)
                : childPath;

            try {
                ShaderLib::FieldProxy childProxy = fieldProxy[childName];
                renderFieldUI(childPath, childProxy, material, depth + 1);
            }
            catch (const std::exception& e) {
                ImGui::TextDisabled("%s: Error (%s)", childName.c_str(), e.what());
            }
        }

        ImGui::TreePop();
    }

    ImGui::PopID();
}

void MaterialComponent::renderArrayFieldUI(
    const std::string& fieldPath,
    ShaderLib::FieldProxy& fieldProxy,
    Material* material,
    int depth
) {
    ImGui::PushID(fieldPath.c_str());

    uint32_t arraySize = fieldProxy.GetArraySize();
    std::string headerLabel = fieldProxy.GetName() + " [" + std::to_string(arraySize) + "]";

    bool isExpanded = ImGui::TreeNodeEx(
        headerLabel.c_str(),
        ImGuiTreeNodeFlags_DefaultOpen
    );

    if (isExpanded) {
        // For large arrays, show only a subset or add pagination
        const uint32_t maxDisplayCount = 100;
        uint32_t displayCount = std::min(arraySize, maxDisplayCount);

        if (arraySize > maxDisplayCount) {
            ImGui::TextDisabled("Showing first %u of %u elements", maxDisplayCount, arraySize);
        }

        for (uint32_t i = 0; i < displayCount; i++) {
            ImGui::PushID(static_cast<int>(i));

            try {
                ShaderLib::FieldProxy elementProxy = fieldProxy[i];
                std::string elementPath = fieldPath + "[" + std::to_string(i) + "]";

                if (elementProxy.IsBaseType()) {
                    // Inline display for base type arrays
                    std::string label = "[" + std::to_string(i) + "]";
                    renderBaseTypeValueUI(label, elementProxy.GetBaseType(), elementProxy);

                    // Check if value changed
                    if (ImGui::IsItemDeactivatedAfterEdit()) {
                        material->SyncAllToGPU();
                        incrementVersion();
                    }
                }
                else {
                    // Nested display for structure arrays
                    renderFieldUI(elementPath, elementProxy, material, depth + 1);
                }
            }
            catch (const std::exception& e) {
                ImGui::TextDisabled("[%u]: Error (%s)", i, e.what());
            }

            ImGui::PopID();
        }

        ImGui::TreePop();
    }

    ImGui::PopID();
}

void MaterialComponent::renderBaseTypeFieldUI(
    const std::string& fieldPath,
    ShaderLib::FieldProxy& fieldProxy,
    Material* material
) {
    ImGui::PushID(fieldPath.c_str());

    std::string label = fieldProxy.GetName();
    bool changed = renderBaseTypeValueUI(label, fieldProxy.GetBaseType(), fieldProxy);

    if (changed) {
        material->SyncAllToGPU();
        incrementVersion();
    }

    ImGui::PopID();
}

// =============================================================================
// BASE TYPE VALUE UI
// =============================================================================

bool MaterialComponent::renderBaseTypeValueUI(
    const std::string& label,
    ShaderLib::BaseType type,
    ShaderLib::FieldProxy& proxy
) {
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

    case BaseType::Mat3: {
        // Matrix types - display as read-only for now
        ImGui::TextDisabled("%s: mat3 (not editable)", label.c_str());
        break;
    }

    case BaseType::Mat4: {
        ImGui::TextDisabled("%s: mat4 (not editable)", label.c_str());
        break;
    }

    default:
        ImGui::TextDisabled("%s: Unsupported type", label.c_str());
        break;
    }

    return changed;
}

// =============================================================================
// TEXTURE UI RENDERING
// =============================================================================

void MaterialComponent::renderTexturesUI(Material* material) {
    std::vector<std::string> textureNames = material->GetTextureNames();

    if (textureNames.empty()) {
        return;
    }

    if (ImGui::CollapsingHeader("Textures", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();

        for (const auto& textureName : textureNames) {
            renderTextureUI(textureName, material);
        }

        ImGui::Unindent();
    }
}

void MaterialComponent::renderTextureUI(const std::string& name, Material* material) {
    ImGui::PushID(name.c_str());

    Material::TextureParam texture;
    if (material->GetTexture(name, texture)) {
        ImGui::Text("%s:", name.c_str());

        if (!texture.assetHandle.filename.empty()) {
            ImGui::SameLine();
            ImGui::Text("%s", texture.assetHandle.filename.c_str());

            // Future: Add texture selection UI
            // if (ImGui::Button("Change...")) { 
            //     // Open texture browser
            // }
        }
        else {
            ImGui::SameLine();
            ImGui::TextDisabled("No texture assigned");
        }
    }

    ImGui::PopID();
}

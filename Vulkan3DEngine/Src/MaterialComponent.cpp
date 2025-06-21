#include "MaterialComponent.h"
#include "MaterialManager.h"
#include "Material.h"
#include "AssetManager.h"
#include <imgui.h>
#include <algorithm>
#include "Engine.h"

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
    ImGui::Text("Material Asset");

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
    }
    else {
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
        items.push_back("None"); // First option for no selection
        for (const auto& file : materialFiles) {
            items.push_back(file.c_str());
        }

        int comboSelection = currentSelection + 1; // +1 because "None" is at index 0

        if (ImGui::Combo("Material File", &comboSelection, items.data(), items.size())) {
            if (comboSelection == 0) {
                // "None" selected
                setMaterial(AssetHandle());
            }
            else {
                // Material file selected
                AssetHandle newHandle(AssetType::Material, materialFiles[comboSelection - 1]);
                setMaterial(newHandle);
            }
        }
    }

    // Manual input fallback
    ImGui::Separator();
    ImGui::Text("Manual Input:");

    char filenameBuffer[256];
    size_t copyLen = std::min(m_material.filename.length(), sizeof(filenameBuffer) - 1);
    m_material.filename.copy(filenameBuffer, copyLen);
    filenameBuffer[copyLen] = '\0';

    if (ImGui::InputText("Filename (without extension)", filenameBuffer, sizeof(filenameBuffer))) {
        AssetHandle newHandle(AssetType::Material, std::string(filenameBuffer));
        setMaterial(newHandle);
    }

    // Material parameters section
    ImGui::Separator();
    renderMaterialParametersUI();
}

std::vector<std::string> MaterialComponent::getAvailableMaterialFiles() {
    std::vector<std::string> files;

    try {
        std::string materialDir = std::string(ASSETS_COMP) + AssetLoader::GetAssetSubdirectory(AssetType::Material);

        if (std::filesystem::exists(materialDir) && std::filesystem::is_directory(materialDir)) {
            for (const auto& entry : std::filesystem::directory_iterator(materialDir)) {
                if (entry.is_regular_file()) {
                    std::string filename = entry.path().stem().string(); // Get filename without extension
                    files.push_back(filename);
                }
            }
        }
    }
    catch (const std::filesystem::filesystem_error& e) {
        // Handle filesystem errors silently in UI context
    }

    std::sort(files.begin(), files.end());
    return files;
}

void MaterialComponent::renderMaterialParametersUI() {
    ImGui::Text("Material Parameters");

	MaterialManager& materialManager = getEngine()->assetSystem().materialManager();

    if (m_material.filename.empty()) {
        ImGui::TextDisabled("No material selected");
        return;
    }

    // Check if material is ready
    if (!materialManager.isAssetReady(m_material.filename)) {
        ImGui::TextDisabled("Material not loaded");
        return;
    }

    // Get material handle
    MaterialHandle materialHandle = materialManager.getHandle<MaterialHandle>(m_material.filename);
    if (!materialHandle.isValid()) {
        ImGui::TextDisabled("Invalid material handle");
        return;
    }

    // Get material
    Material* material = materialManager.getMaterial(materialHandle);
    if (!material) {
        ImGui::TextDisabled("Material not found");
        return;
    }

    // Display shader info
    ImGui::Text("Shader: %s", material->name().c_str());

    ImGui::Separator();

    // Render parameter controls
    if (ImGui::CollapsingHeader("Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
        const auto& parameters = material->parameters();

        if (parameters.empty()) {
            ImGui::TextDisabled("No parameters");
        }
        else {
            for (const auto& param : parameters) {
                ImGui::PushID(param.name.c_str());

                // Create a mutable copy for editing
                Material::ParamValue paramValue = param.value;

                ImGui::Text("%s:", param.name.c_str());
                ImGui::SameLine();

                // Render the parameter UI
                bool changed = false;
                ImGui::PushItemWidth(200.0f);

                if (std::holds_alternative<bool>(paramValue)) {
                    bool val = std::get<bool>(paramValue);
                    if (ImGui::Checkbox("##value", &val)) {
                        paramValue = val;
                        changed = true;
                    }
                }
                else if (std::holds_alternative<float>(paramValue)) {
                    float val = std::get<float>(paramValue);
                    if (ImGui::DragFloat("##value", &val, 0.01f)) {
                        paramValue = val;
                        changed = true;
                    }
                }
                else if (std::holds_alternative<glm::vec2>(paramValue)) {
                    glm::vec2 val = std::get<glm::vec2>(paramValue);
                    if (ImGui::DragFloat2("##value", &val.x, 0.01f)) {
                        paramValue = val;
                        changed = true;
                    }
                }
                else if (std::holds_alternative<glm::vec3>(paramValue)) {
                    glm::vec3 val = std::get<glm::vec3>(paramValue);
                    if (ImGui::DragFloat3("##value", &val.x, 0.01f)) {
                        paramValue = val;
                        changed = true;
                    }
                }
                else if (std::holds_alternative<glm::vec4>(paramValue)) {
                    glm::vec4 val = std::get<glm::vec4>(paramValue);
                    if (ImGui::DragFloat4("##value", &val.x, 0.01f)) {
                        paramValue = val;
                        changed = true;
                    }
                }
                else if (std::holds_alternative<int32_t>(paramValue)) {
                    int32_t val = std::get<int32_t>(paramValue);
                    if (ImGui::DragInt("##value", &val)) {
                        paramValue = val;
                        changed = true;
                    }
                }
                else if (std::holds_alternative<glm::ivec2>(paramValue)) {
                    glm::ivec2 val = std::get<glm::ivec2>(paramValue);
                    if (ImGui::DragInt2("##value", &val.x)) {
                        paramValue = val;
                        changed = true;
                    }
                }
                else if (std::holds_alternative<glm::ivec3>(paramValue)) {
                    glm::ivec3 val = std::get<glm::ivec3>(paramValue);
                    if (ImGui::DragInt3("##value", &val.x)) {
                        paramValue = val;
                        changed = true;
                    }
                }
                else if (std::holds_alternative<glm::ivec4>(paramValue)) {
                    glm::ivec4 val = std::get<glm::ivec4>(paramValue);
                    if (ImGui::DragInt4("##value", &val.x)) {
                        paramValue = val;
                        changed = true;
                    }
                }
                else if (std::holds_alternative<uint32_t>(paramValue)) {
                    int val = static_cast<int>(std::get<uint32_t>(paramValue));
                    if (ImGui::DragInt("##value", &val, 1.0f, 0)) {
                        paramValue = static_cast<uint32_t>(std::max(0, val));
                        changed = true;
                    }
                }
                else if (std::holds_alternative<Material::TextureParam>(paramValue)) {
                    const auto& texParam = std::get<Material::TextureParam>(paramValue);
                    if (!texParam.handle.filename.empty()) {
                        ImGui::Text("Texture: %s", texParam.handle.filename.c_str());
                    }
                    else {
                        ImGui::TextDisabled("No texture");
                    }
                }
                else {
                    ImGui::TextDisabled("Unsupported parameter type");
                }

                ImGui::PopItemWidth();

                // Update parameter if changed
                if (changed) {
                    materialManager.setMaterialParameter(materialHandle, param.name, paramValue);
                    incrementVersion(); // Mark component as changed
                }

                ImGui::PopID();
            }
        }
    }
}
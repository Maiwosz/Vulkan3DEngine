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
    AssetManager& assetManager = getEngine()->assetSystem().assetManager();
    MaterialManager& materialManager = getEngine()->assetSystem().materialManager();

    assetManager.ensureReady(m_material);

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

                // Check if this is a buffer parameter
                if (auto* bufferVal = std::get_if<ShaderLib::BufferValue>(&paramValue)) {
                    // Now check the actual type inside BufferValue
                    if (auto* val = std::get_if<bool>(bufferVal)) {
                        bool temp = *val;
                        if (ImGui::Checkbox("##value", &temp)) {
                            *bufferVal = temp;
                            changed = true;
                        }
                    }
                    else if (auto* val = std::get_if<float>(bufferVal)) {
                        float temp = *val;
                        if (ImGui::DragFloat("##value", &temp, 0.01f)) {
                            *bufferVal = temp;
                            changed = true;
                        }
                    }
                    else if (auto* val = std::get_if<glm::vec2>(bufferVal)) {
                        glm::vec2 temp = *val;
                        if (ImGui::DragFloat2("##value", &temp.x, 0.01f)) {
                            *bufferVal = temp;
                            changed = true;
                        }
                    }
                    else if (auto* val = std::get_if<glm::vec3>(bufferVal)) {
                        glm::vec3 temp = *val;
                        if (ImGui::DragFloat3("##value", &temp.x, 0.01f)) {
                            *bufferVal = temp;
                            changed = true;
                        }
                    }
                    else if (auto* val = std::get_if<glm::vec4>(bufferVal)) {
                        glm::vec4 temp = *val;
                        if (ImGui::DragFloat4("##value", &temp.x, 0.01f)) {
                            *bufferVal = temp;
                            changed = true;
                        }
                    }
                    else if (auto* val = std::get_if<int32_t>(bufferVal)) {
                        int32_t temp = *val;
                        if (ImGui::DragInt("##value", &temp)) {
                            *bufferVal = temp;
                            changed = true;
                        }
                    }
                    else if (auto* val = std::get_if<glm::ivec2>(bufferVal)) {
                        glm::ivec2 temp = *val;
                        if (ImGui::DragInt2("##value", &temp.x)) {
                            *bufferVal = temp;
                            changed = true;
                        }
                    }
                    else if (auto* val = std::get_if<glm::ivec3>(bufferVal)) {
                        glm::ivec3 temp = *val;
                        if (ImGui::DragInt3("##value", &temp.x)) {
                            *bufferVal = temp;
                            changed = true;
                        }
                    }
                    else if (auto* val = std::get_if<glm::ivec4>(bufferVal)) {
                        glm::ivec4 temp = *val;
                        if (ImGui::DragInt4("##value", &temp.x)) {
                            *bufferVal = temp;
                            changed = true;
                        }
                    }
                    else if (auto* val = std::get_if<uint32_t>(bufferVal)) {
                        int temp = static_cast<int>(*val);
                        if (ImGui::DragInt("##value", &temp, 1.0f, 0)) {
                            *bufferVal = static_cast<uint32_t>(std::max(0, temp));
                            changed = true;
                        }
                    }
                    else if (auto* val = std::get_if<glm::uvec2>(bufferVal)) {
                        int temp[2] = { static_cast<int>(val->x), static_cast<int>(val->y) };
                        if (ImGui::DragInt2("##value", temp, 1.0f, 0)) {
                            *bufferVal = glm::uvec2(std::max(0, temp[0]), std::max(0, temp[1]));
                            changed = true;
                        }
                    }
                    else if (auto* val = std::get_if<glm::uvec3>(bufferVal)) {
                        int temp[3] = { static_cast<int>(val->x), static_cast<int>(val->y), static_cast<int>(val->z) };
                        if (ImGui::DragInt3("##value", temp, 1.0f, 0)) {
                            *bufferVal = glm::uvec3(std::max(0, temp[0]), std::max(0, temp[1]), std::max(0, temp[2]));
                            changed = true;
                        }
                    }
                    else if (auto* val = std::get_if<glm::uvec4>(bufferVal)) {
                        int temp[4] = { static_cast<int>(val->x), static_cast<int>(val->y), static_cast<int>(val->z), static_cast<int>(val->w) };
                        if (ImGui::DragInt4("##value", temp, 1.0f, 0)) {
                            *bufferVal = glm::uvec4(std::max(0, temp[0]), std::max(0, temp[1]), std::max(0, temp[2]), std::max(0, temp[3]));
                            changed = true;
                        }
                    }
                    else if (auto* val = std::get_if<double>(bufferVal)) {
                        float temp = static_cast<float>(*val);
                        if (ImGui::DragFloat("##value", &temp, 0.01f)) {
                            *bufferVal = static_cast<double>(temp);
                            changed = true;
                        }
                    }
                    else {
                        ImGui::TextDisabled("Unsupported buffer type");
                    }
                }
                else if (auto* texParam = std::get_if<Material::TextureParam>(&paramValue)) {
                    if (!texParam->handle.filename.empty()) {
                        ImGui::Text("Texture: %s", texParam->handle.filename.c_str());
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
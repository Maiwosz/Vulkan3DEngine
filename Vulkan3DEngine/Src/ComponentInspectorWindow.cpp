#include "ComponentInspectorWindow.h"
#include "Registry.h"
#include "EntityManager.h"
#include "ComponentManager.h"
#include "imgui.h"
#include <format>
#include <algorithm>
#include "SelectionManager.h"
#include "LoggerManager.h"

ComponentInspectorWindow::ComponentInspectorWindow(Registry& registry, SelectionManager& selectionManager)
    : m_registry(registry), m_selectionManager(selectionManager)
{
    // Subscribe to selection changes
    m_selectionManager.addSelectionCallback([this](Entity entity) {
        if (entity.id != 0 && m_registry.entities().valid(entity)) {
            EDITOR_LOG_DEBUG("Selected entity {} for inspection", entity.id);
        }
        });

    EDITOR_LOG_DEBUG("ComponentInspectorWindow initialized");
}

void ComponentInspectorWindow::render() {
    if (!m_showWindow) {
        return;
    }

    if (ImGui::Begin("Component Inspector", &m_showWindow)) {
        if (!m_selectionManager.hasSelection() || !m_registry.entities().valid(m_selectionManager.getSelectedEntity())) {
            ImGui::TextDisabled("No entity selected");
        }
        else {
            renderEntityInfo();
            ImGui::Separator();
            renderComponentList();
            ImGui::Separator();
            renderAddComponentSection();
        }
    }
    ImGui::End();

    // Handle add component popup
    if (m_showAddComponentPopup) {
        ImGui::OpenPopup("Add Component");
        m_showAddComponentPopup = false;
    }

    if (ImGui::BeginPopupModal("Add Component", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Select component to add:");

        auto availableComponents = m_registry.components().getAllComponentNames();
        auto existingComponents = m_registry.components().getEntityComponentTypes(m_selectionManager.getSelectedEntity());

        // Filter out components that already exist on this entity
        std::vector<std::string> filteredComponents;
        for (const auto& component : availableComponents) {
            if (std::find(existingComponents.begin(), existingComponents.end(), component) == existingComponents.end()) {
                filteredComponents.push_back(component);
            }
        }

        if (filteredComponents.empty()) {
            ImGui::TextDisabled("All components already added");
        }
        else {
            for (const auto& componentName : filteredComponents) {
                if (ImGui::Selectable(componentName.c_str())) {
                    addComponent(componentName);
                    ImGui::CloseCurrentPopup();
                }
            }
        }

        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void ComponentInspectorWindow::setSelectedEntity(Entity entity) {
    if (m_selectionManager.getSelectedEntity() != entity) {
        m_selectionManager.selectEntity(entity);

        if (entity.id != 0 && m_registry.entities().valid(entity)) {
            EDITOR_LOG_DEBUG("Selected entity {} for inspection", entity.id);
        }
    }
}

void ComponentInspectorWindow::renderEntityInfo() {
    Entity selectedEntity = m_selectionManager.getSelectedEntity();
    std::string entityName = m_registry.entities().getEntityName(selectedEntity);
    if (entityName.empty()) {
        entityName = std::format("Entity_{}", selectedEntity.id);
    }

    ImGui::Text("Entity: %s (ID: %u)", entityName.c_str(), selectedEntity.id);

    // Allow editing entity name
    char nameBuffer[256];
    size_t copyLen = std::min(entityName.length(), sizeof(nameBuffer) - 1);
    entityName.copy(nameBuffer, copyLen);
    nameBuffer[copyLen] = '\0';

    ImGui::PushItemWidth(-1);
    if (ImGui::InputText("##EntityName", nameBuffer, sizeof(nameBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
        m_registry.entities().setEntityName(selectedEntity, std::string(nameBuffer));
        EDITOR_LOG_INFO("Renamed entity {} to '{}'", selectedEntity.id, nameBuffer);
    }
    ImGui::PopItemWidth();
}

void ComponentInspectorWindow::renderComponentList() {
    auto componentTypes = m_registry.components().getEntityComponentTypes(m_selectionManager.getSelectedEntity());

    if (componentTypes.empty()) {
        ImGui::TextDisabled("No components");
        return;
    }

    ImGui::Text("Components:");

    for (const auto& componentName : componentTypes) {
        ImGui::PushID(componentName.c_str());

        bool isOpen = ImGui::CollapsingHeader(componentName.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

        // Context menu for component
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Remove Component")) {
                removeComponent(componentName);
            }
            ImGui::EndPopup();
        }

        if (isOpen) {
            ImGui::Indent();
            renderComponentEditor(componentName);
            ImGui::Unindent();
        }

        ImGui::PopID();
    }
}

void ComponentInspectorWindow::renderAddComponentSection() {
    if (ImGui::Button("Add Component", ImVec2(-1, 0))) {
        m_showAddComponentPopup = true;
    }
}

void ComponentInspectorWindow::renderComponentEditor(const std::string& componentName) {
    try {
        // Get component and call its renderUI method
        Component* component = m_registry.components().getComponentByName(m_selectionManager.getSelectedEntity(), componentName);
        if (component) {
            component->renderUI();
        }
        else {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Component not found");
        }
    }
    catch (const std::exception& e) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Error: %s", e.what());
    }
}

void ComponentInspectorWindow::addComponent(const std::string& componentName) {
    try {
        // Create component with default data
        nlohmann::json defaultData = nlohmann::json::object();

        if (m_registry.components().createComponentFromData(m_selectionManager.getSelectedEntity(), componentName, defaultData)) {
            EDITOR_LOG_INFO("Added component '{}' to entity {}", componentName, m_selectionManager.getSelectedEntity().id);
        }
        else {
            EDITOR_LOG_ERROR("Failed to add component '{}' to entity {}", componentName, m_selectionManager.getSelectedEntity().id);
        }
    }
    catch (const std::exception& e) {
        EDITOR_LOG_ERROR("Exception adding component '{}': {}", componentName, e.what());
    }
}

void ComponentInspectorWindow::removeComponent(const std::string& componentName) {
    try {
        m_registry.components().removeComponentByName(m_selectionManager.getSelectedEntity(), componentName);
        EDITOR_LOG_INFO("Removed component '{}' from entity {}", componentName, m_selectionManager.getSelectedEntity().id);
    }
    catch (const std::exception& e) {
        EDITOR_LOG_ERROR("Exception removing component '{}': {}", componentName, e.what());
    }
}
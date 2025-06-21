#include "HierarchyWindow.h"
#include "Registry.h"
#include "SelectionManager.h"
#include "EntityManager.h"
#include "imgui.h"
#include <format>
#include "LoggerConfig.h"

HierarchyWindow::HierarchyWindow(Registry& registry, SelectionManager& selectionManager)
    : m_registry(registry), m_selectionManager(selectionManager)
{

    EDITOR_LOG_DEBUG("HierarchyWindow initialized");
}

void HierarchyWindow::render() {
    if (!m_showWindow) {
        return;
    }

    if (ImGui::Begin("Scene Hierarchy", &m_showWindow)) {
        renderEntityHierarchy();
    }
    ImGui::End();
}

void HierarchyWindow::renderEntityHierarchy() {
    // Add buttons for entity management
    if (ImGui::Button("Create Entity")) {
        Entity newEntity = m_registry.entities().create("New Entity");
        EDITOR_LOG_INFO("Created new entity: {} (ID: {})",
            m_registry.entities().getEntityName(newEntity), newEntity.id);
    }

    ImGui::SameLine();

    if (ImGui::Button("Delete Selected") && m_selectionManager.hasSelection()) {
        Entity selectedEntity = m_selectionManager.getSelectedEntity();
        if (m_registry.entities().valid(selectedEntity)) {
            std::string entityName = m_registry.entities().getEntityName(selectedEntity);
            m_registry.entities().destroy(selectedEntity);
            EDITOR_LOG_INFO("Deleted entity: {} (ID: {})", entityName, selectedEntity.id);
            m_selectionManager.clearSelection();
        }
    }


    ImGui::Separator();

    // Show selected entity info
    if (m_selectionManager.hasSelection()) {
        Entity selectedEntity = m_selectionManager.getSelectedEntity();
        if (m_registry.entities().valid(selectedEntity)) {
            ImGui::Text("Selected: %s (ID: %u)",
                getEntityDisplayName(selectedEntity).c_str(),
                selectedEntity.id);
            ImGui::Separator();
        }
    }

    // Render the entity tree
    renderRootEntities();
}

void HierarchyWindow::renderRootEntities() {
    auto rootEntities = m_registry.entities().getRootEntities();

    if (rootEntities.empty()) {
        ImGui::TextDisabled("No entities in scene");
        return;
    }

    // Sort root entities by name for consistent display
    std::vector<Entity> sortedRoots(rootEntities.begin(), rootEntities.end());
    std::sort(sortedRoots.begin(), sortedRoots.end(), [this](Entity a, Entity b) {
        return getEntityDisplayName(a) < getEntityDisplayName(b);
        });

    for (Entity entity : sortedRoots) {
        if (m_registry.entities().valid(entity)) {
            renderEntityNode(entity, 0);
        }
    }
}

void HierarchyWindow::renderEntityNode(Entity entity, int depth) {
    if (!m_registry.entities().valid(entity)) {
        return;
    }

    std::string displayName = getEntityDisplayName(entity);
    bool hasChildEntities = hasChildren(entity);
    bool isExpanded = isEntityExpanded(entity);
    bool isSelected = (m_selectionManager.getSelectedEntity() == entity);

    // Create unique ID for ImGui
    ImGui::PushID(static_cast<int>(entity.id));

    // Set selection color
    if (isSelected) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f)); // Yellow for selected
    }

    // Render tree node
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
        ImGuiTreeNodeFlags_OpenOnDoubleClick |
        ImGuiTreeNodeFlags_SpanAvailWidth;

    if (!hasChildEntities) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    if (isSelected) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    // Use different icon based on whether entity has children
    const char* icon = hasChildEntities ? (isExpanded ? "📂" : "📁") : "📄";
    std::string nodeLabel = std::format("{} {}", icon, displayName);

    bool nodeOpen = false;
    if (hasChildEntities) {
        ImGui::SetNextItemOpen(isExpanded);
        nodeOpen = ImGui::TreeNodeEx(nodeLabel.c_str(), flags);
        setEntityExpanded(entity, nodeOpen);
    }
    else {
        ImGui::TreeNodeEx(nodeLabel.c_str(), flags);
        nodeOpen = false;
    }

    // Handle selection
    if (ImGui::IsItemClicked()) {
        handleEntitySelection(entity);
    }

    // Handle context menu
    if (ImGui::BeginPopupContextItem()) {
        handleEntityContextMenu(entity);
        ImGui::EndPopup();
    }

    // Drag and drop source
    if (ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload("ENTITY", &entity, sizeof(Entity));
        ImGui::Text("Moving: %s", displayName.c_str());
        ImGui::EndDragDropSource();
    }

    // Drag and drop target
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY")) {
            Entity draggedEntity = *(Entity*)payload->Data;
            if (draggedEntity != entity && m_registry.entities().valid(draggedEntity)) {
                m_registry.entities().setParent(draggedEntity, entity);
                EDITOR_LOG_INFO("Moved entity {} to parent {}",
                    m_registry.entities().getEntityName(draggedEntity),
                    m_registry.entities().getEntityName(entity));
            }
        }
        ImGui::EndDragDropTarget();
    }

    if (isSelected) {
        ImGui::PopStyleColor();
    }

    // Render children if node is open
    if (nodeOpen && hasChildEntities) {
        const auto& children = m_registry.entities().getChildren(entity);

        // Sort children by name
        std::vector<Entity> sortedChildren(children.begin(), children.end());
        std::sort(sortedChildren.begin(), sortedChildren.end(), [this](Entity a, Entity b) {
            return getEntityDisplayName(a) < getEntityDisplayName(b);
            });

        for (Entity child : sortedChildren) {
            renderEntityNode(child, depth + 1);
        }

        if (hasChildEntities) {
            ImGui::TreePop();
        }
    }

    ImGui::PopID();
}

bool HierarchyWindow::isEntityExpanded(Entity entity) const {
    return m_expandedEntities.find(entity) != m_expandedEntities.end();
}

void HierarchyWindow::setEntityExpanded(Entity entity, bool expanded) {
    if (expanded) {
        m_expandedEntities.insert(entity);
    }
    else {
        m_expandedEntities.erase(entity);
    }
}

std::string HierarchyWindow::getEntityDisplayName(Entity entity) const {
    std::string name = m_registry.entities().getEntityName(entity);
    if (name.empty()) {
        return std::format("Entity_{}", entity.id);
    }
    return name;
}

bool HierarchyWindow::hasChildren(Entity entity) const {
    const auto& children = m_registry.entities().getChildren(entity);
    return !children.empty();
}

void HierarchyWindow::handleEntitySelection(Entity entity) {
    m_selectionManager.selectEntity(entity);
    EDITOR_LOG_DEBUG("Selected entity: {} (ID: {})",
        getEntityDisplayName(entity), entity.id);
}

void HierarchyWindow::handleEntityContextMenu(Entity entity) {
    if (ImGui::MenuItem("Create Child")) {
        Entity child = m_registry.entities().create("Child Entity");
        m_registry.entities().setParent(child, entity);
        EDITOR_LOG_INFO("Created child entity: {} for parent: {}",
            m_registry.entities().getEntityName(child),
            m_registry.entities().getEntityName(entity));
    }

    if (ImGui::MenuItem("Duplicate")) {
        Entity duplicate = m_registry.entities().cloneEntityHierarchy(entity, m_registry.entities().getParent(entity));
        EDITOR_LOG_INFO("Duplicated entity: {} -> {}",
            m_registry.entities().getEntityName(entity),
            m_registry.entities().getEntityName(duplicate));
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Rename")) {
        // TODO: Implement rename dialog
        EDITOR_LOG_INFO("Rename requested for entity: {}", getEntityDisplayName(entity));
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Delete", "Del")) {
        if (m_selectionManager.getSelectedEntity() == entity) {
            m_selectionManager.clearSelection();
        }
        std::string entityName = m_registry.entities().getEntityName(entity);
        m_registry.entities().destroy(entity);
        EDITOR_LOG_INFO("Deleted entity: {} (ID: {})", entityName, entity.id);
    }
}
#include "HierarchyWindow.h"
#include "Registry.h"
#include "SelectionManager.h"
#include "EntityManager.h"
#include "PrefabInstanceManager.h"
#include "imgui.h"
#include <format>
#include "LoggerManager.h"
#include "AssetLoader.h"
#include "Paths.h"
#include <filesystem>

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
    // Show selected entity info
    if (m_selectionManager.hasSelection()) {
        Entity selectedEntity = m_selectionManager.getSelectedEntity();
        if (m_registry.entities().valid(selectedEntity)) {
            std::string displayText = std::format("Selected: {} (ID: {})",
                getEntityDisplayName(selectedEntity), selectedEntity.id);

            // Add prefab instance info
            if (m_registry.prefabs().isEntityPartOfInstance(selectedEntity)) {
                PrefabInstanceHandle instanceHandle = m_registry.prefabs().getInstanceForEntity(selectedEntity);
                std::string instanceName = m_registry.prefabs().getInstanceName(instanceHandle);
                displayText += std::format(" [Instance: {}]", instanceName);

                if (m_registry.prefabs().isInstanceRoot(selectedEntity)) {
                    displayText += " [ROOT]";
                }
            }

            ImGui::Text("%s", displayText.c_str());
            ImGui::Separator();
        }
    }

    // Add horizontal scrollbar
    ImGui::BeginChild("EntityHierarchy", ImVec2(0, 0), false,
        ImGuiWindowFlags_HorizontalScrollbar);

    // Render the entity tree
    renderRootEntities();

    // Create invisible button that covers remaining space for drag and drop
    ImVec2 availableSpace = ImGui::GetContentRegionAvail();
    if (availableSpace.y > 0) {
        ImGui::InvisibleButton("EmptySpaceDragTarget", availableSpace);

        // Handle context menu for empty space
        if (ImGui::BeginPopupContextItem("EmptySpaceMenu")) {
            handleEmptySpaceContextMenu();
            ImGui::EndPopup();
        }

        // Handle drag and drop to empty space (root)
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY")) {
                Entity draggedEntity = *(Entity*)payload->Data;
                if (m_registry.entities().valid(draggedEntity)) {
                    // Allow moving regular entities or instance roots to root
                    bool canMove = !m_registry.prefabs().isEntityPartOfInstance(draggedEntity) ||
                        m_registry.prefabs().isInstanceRoot(draggedEntity);

                    if (canMove) {
                        m_registry.entities().removeParent(draggedEntity);
                        EDITOR_LOG_INFO("Moved entity {} to root",
                            m_registry.entities().getEntityName(draggedEntity));
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }
    }

    ImGui::EndChild();

    // Handle rename dialog
    handleRenameDialog();
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

void HierarchyWindow::renderPrefabMenu() {
    ImGui::Text("Select Prefab:");
    ImGui::Separator();

    auto availablePrefabs = getAvailablePrefabs();

    if (availablePrefabs.empty()) {
        ImGui::TextDisabled("No prefabs available");
        return;
    }

    Entity targetParent = Entity(0);
    if (m_selectionManager.hasSelection()) {
        Entity selected = m_selectionManager.getSelectedEntity();
        if (m_registry.entities().valid(selected) &&
            !m_registry.prefabs().isEntityPartOfInstance(selected)) {
            targetParent = selected;
        }
    }

    for (const std::string& prefabName : availablePrefabs) {
        if (ImGui::MenuItem(prefabName.c_str())) {
            PrefabInstanceHandle instanceHandle = m_registry.prefabs().createInstance(prefabName, targetParent);
            if (instanceHandle.id != 0) {
                EDITOR_LOG_INFO("Created prefab instance '{}' with parent entity {}",
                    prefabName, targetParent.id);
            }
            else {
                EDITOR_LOG_ERROR("Failed to create prefab instance '{}'", prefabName);
            }
            ImGui::CloseCurrentPopup();
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
    bool isLocked = m_registry.entities().isEntityLocked(entity);
    bool isPartOfInstance = m_registry.prefabs().isEntityPartOfInstance(entity);
    bool isInstanceRoot = isPartOfInstance && m_registry.prefabs().isInstanceRoot(entity);

    // Create unique ID for ImGui
    ImGui::PushID(static_cast<int>(entity.id));

    // Set color based on state
    if (isLocked) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f)); // Grayed out for locked
    }
    else if (isSelected) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f)); // Yellow for selected
    }
    else if (isInstanceRoot) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f)); // Light blue for prefab root
    }
    else if (isPartOfInstance) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.9f, 1.0f, 1.0f)); // Lighter blue for prefab children
    }
    else {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f)); // Normal white
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

    // Create node label with prefab indicator
    std::string nodeLabel = displayName;
    if (isInstanceRoot) {
        PrefabInstanceHandle instanceHandle = m_registry.prefabs().getInstanceForEntity(entity);
        std::string instanceName = m_registry.prefabs().getInstanceName(instanceHandle);
        nodeLabel = std::format("[P] {} ({})", displayName, instanceName);
    }
    else if (isPartOfInstance) {
        nodeLabel = std::format("  {}", displayName); // Indent child entities
    }

    if (isLocked) {
        nodeLabel += " [LOCKED]";
    }

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

    // Handle selection (only if not locked)
    if (ImGui::IsItemClicked()) {
        handleEntitySelection(entity);
    }

    // Handle context menu
    if (ImGui::BeginPopupContextItem()) {
        handleEntityContextMenu(entity);
        ImGui::EndPopup();
    }

    // Drag and drop source (only if not locked and not part of instance)
    if (!isLocked && (!isPartOfInstance || isInstanceRoot) && ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload("ENTITY", &entity, sizeof(Entity));
        ImGui::Text("Moving: %s", displayName.c_str());
        ImGui::EndDragDropSource();
    }

    // Drag and drop target (only if not locked and not part of instance)
    if (!isLocked && (!isPartOfInstance) && ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY")) {
            Entity draggedEntity = *(Entity*)payload->Data;
            if (draggedEntity != entity && m_registry.entities().valid(draggedEntity)) {
                // Allow moving regular entities or instance roots
                bool canMove = !m_registry.prefabs().isEntityPartOfInstance(draggedEntity) ||
                    m_registry.prefabs().isInstanceRoot(draggedEntity);

                if (canMove) {
                    m_registry.entities().setParent(draggedEntity, entity);
                    EDITOR_LOG_INFO("Moved entity {} to parent {}",
                        m_registry.entities().getEntityName(draggedEntity),
                        m_registry.entities().getEntityName(entity));
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::PopStyleColor();

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
    bool isPartOfInstance = m_registry.prefabs().isEntityPartOfInstance(entity);
    bool isInstanceRoot = isPartOfInstance && m_registry.prefabs().isInstanceRoot(entity);
    bool isLocked = m_registry.entities().isEntityLocked(entity);

    // Create Child - only for non-instance entities
    if (!isPartOfInstance && ImGui::MenuItem("Create Child")) {
        Entity child = m_registry.entities().create("Child Entity");
        m_registry.entities().setParent(child, entity);
        EDITOR_LOG_INFO("Created child entity: {} for parent: {}",
            m_registry.entities().getEntityName(child),
            m_registry.entities().getEntityName(entity));
    }

    // Create Instance submenu - only for non-instance entities
    if (!isPartOfInstance && ImGui::BeginMenu("Create Prefab Instance")) {
        renderPrefabMenuWithParent(entity);
        ImGui::EndMenu();
    }

    // Duplicate - only for non-instance entities
    if (!isPartOfInstance && ImGui::MenuItem("Duplicate")) {
        Entity duplicate = m_registry.entities().cloneEntityHierarchy(entity,
            m_registry.entities().getParent(entity));
        EDITOR_LOG_INFO("Duplicated entity: {} -> {}",
            m_registry.entities().getEntityName(entity),
            m_registry.entities().getEntityName(duplicate));
    }

    // Create Prefab - only for non-instance entities
    if (!isPartOfInstance && ImGui::MenuItem("Create Prefab")) {
        PrefabHandle prefabHandle = m_registry.prefabs().createPrefabFromEntity(entity);
        if (prefabHandle.id != 0) {
            EDITOR_LOG_INFO("Created prefab from entity: {}", getEntityDisplayName(entity));
        }
        else {
            EDITOR_LOG_ERROR("Failed to create prefab from entity: {}", getEntityDisplayName(entity));
        }
    }

    // Prefab instance operations
    if (isPartOfInstance) {
        PrefabInstanceHandle instanceHandle = m_registry.prefabs().getInstanceForEntity(entity);

        ImGui::Separator();
        ImGui::Text("Prefab Instance Operations:");

        if (isInstanceRoot) {
            if (ImGui::MenuItem("Unpack Instance")) {
                if (m_registry.prefabs().unpackInstance(instanceHandle)) {
                    EDITOR_LOG_INFO("Unpacked prefab instance");
                }
                else {
                    EDITOR_LOG_ERROR("Failed to unpack prefab instance");
                }
            }

            if (ImGui::MenuItem("Sync with Prefab")) {
                if (m_registry.prefabs().syncWithPrefab(instanceHandle)) {
                    EDITOR_LOG_INFO("Synced instance with prefab");
                }
                else {
                    EDITOR_LOG_ERROR("Failed to sync instance with prefab");
                }
            }
        }

        // Component override operations
        if (ImGui::BeginMenu("Component Overrides")) {
            if (ImGui::MenuItem("Override All Components")) {
                m_registry.prefabs().overrideAllComponents(instanceHandle, entity);
                EDITOR_LOG_INFO("Overrode all components for entity {}", entity.id);
            }

            if (ImGui::MenuItem("Restore All Components")) {
                m_registry.prefabs().restoreAllComponents(instanceHandle, entity);
                EDITOR_LOG_INFO("Restored all components for entity {}", entity.id);
            }

            ImGui::EndMenu();
        }
    }

    ImGui::Separator();

    // Rename - enabled for non-instance entities OR instance root
    bool canRename = !isPartOfInstance || isInstanceRoot;
    if (ImGui::MenuItem("Rename", nullptr, false, canRename)) {
        startRename(entity);
    }

    ImGui::Separator();

    // Delete
    if (isInstanceRoot) {
        if (ImGui::MenuItem("Delete Instance", "Del")) {
            PrefabInstanceHandle instanceHandle = m_registry.prefabs().getInstanceForEntity(entity);
            if (m_selectionManager.getSelectedEntity() == entity) {
                m_selectionManager.clearSelection();
            }
            m_registry.prefabs().destroyInstance(instanceHandle);
            EDITOR_LOG_INFO("Deleted prefab instance");
        }
    }
    else if (!isPartOfInstance) {
        if (ImGui::MenuItem("Delete", "Del")) {
            if (m_selectionManager.getSelectedEntity() == entity) {
                m_selectionManager.clearSelection();
            }
            std::string entityName = m_registry.entities().getEntityName(entity);
            m_registry.entities().destroy(entity);
            EDITOR_LOG_INFO("Deleted entity: {} (ID: {})", entityName, entity.id);
        }
    }
    else {
        // Entity is part of instance but not root
        ImGui::MenuItem("Delete (Prefab Entity)", "Del", false, false);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Cannot delete individual entities from prefab instance. Delete the root entity instead.");
        }
    }
}

std::vector<std::string> HierarchyWindow::getAvailablePrefabs() const {
    std::vector<std::string> prefabs;

    std::string prefabDir = std::string(ASSETS_COMP) + AssetLoader::GetAssetSubdirectory(AssetLib::AssetType::Prefab);

    try {
        if (std::filesystem::exists(prefabDir) && std::filesystem::is_directory(prefabDir)) {
            for (const auto& entry : std::filesystem::directory_iterator(prefabDir)) {
                if (entry.is_regular_file()) {
                    std::string filename = entry.path().filename().string();
                    std::string prefabName = extractPrefabName(filename);
                    if (!prefabName.empty()) {
                        prefabs.push_back(prefabName);
                    }
                }
            }
        }
    }
    catch (const std::filesystem::filesystem_error& ex) {
        EDITOR_LOG_ERROR("Error accessing prefab directory '{}': {}", prefabDir, ex.what());
    }

    // Sort alphabetically
    std::sort(prefabs.begin(), prefabs.end());

    return prefabs;
}

std::string HierarchyWindow::extractPrefabName(const std::string& filename) const {
    // Remove file extension to get prefab name
    size_t lastDot = filename.find_last_of('.');
    if (lastDot != std::string::npos) {
        return filename.substr(0, lastDot);
    }
    return filename;
}

void HierarchyWindow::handleEmptySpaceContextMenu() {
    if (ImGui::MenuItem("Create Entity")) {
        Entity newEntity = m_registry.entities().create("New Entity");
        EDITOR_LOG_INFO("Created new entity: {} (ID: {})",
            m_registry.entities().getEntityName(newEntity), newEntity.id);
    }

    // Create Instance submenu
    if (ImGui::BeginMenu("Create Prefab Instance")) {
        renderPrefabMenu();
        ImGui::EndMenu();
    }

    if (m_selectionManager.hasSelection()) {
        ImGui::Separator();

        Entity selectedEntity = m_selectionManager.getSelectedEntity();
        if (m_registry.entities().valid(selectedEntity)) {
            if (ImGui::MenuItem("Delete Selected")) {
                std::string entityName = m_registry.entities().getEntityName(selectedEntity);

                // Check if it's a prefab instance
                if (m_registry.prefabs().isEntityPartOfInstance(selectedEntity)) {
                    PrefabInstanceHandle instanceHandle = m_registry.prefabs().getInstanceForEntity(selectedEntity);
                    if (m_registry.prefabs().isInstanceRoot(selectedEntity)) {
                        // Delete entire prefab instance
                        m_registry.prefabs().destroyInstance(instanceHandle);
                        EDITOR_LOG_INFO("Deleted prefab instance: {}", entityName);
                    }
                    else {
                        EDITOR_LOG_WARN("Cannot delete individual entities from prefab instance. Delete the root entity instead.");
                    }
                }
                else {
                    // Regular entity deletion
                    m_registry.entities().destroy(selectedEntity);
                    EDITOR_LOG_INFO("Deleted entity: {} (ID: {})", entityName, selectedEntity.id);
                }
                m_selectionManager.clearSelection();
            }

            if (!m_registry.prefabs().isEntityPartOfInstance(selectedEntity)) {
                if (ImGui::MenuItem("Create Prefab from Selected")) {
                    PrefabHandle prefabHandle = m_registry.prefabs().createPrefabFromEntity(selectedEntity);
                    if (prefabHandle.id != 0) {
                        EDITOR_LOG_INFO("Created prefab from entity: {}",
                            m_registry.entities().getEntityName(selectedEntity));
                    }
                    else {
                        EDITOR_LOG_ERROR("Failed to create prefab from entity: {}",
                            m_registry.entities().getEntityName(selectedEntity));
                    }
                }
            }
        }
    }
}

void HierarchyWindow::renderPrefabMenuWithParent(Entity parent) {
    ImGui::Text("Select Prefab:");
    ImGui::Separator();

    auto availablePrefabs = getAvailablePrefabs();

    if (availablePrefabs.empty()) {
        ImGui::TextDisabled("No prefabs available");
        return;
    }

    for (const std::string& prefabName : availablePrefabs) {
        if (ImGui::MenuItem(prefabName.c_str())) {
            PrefabInstanceHandle instanceHandle = m_registry.prefabs().createInstance(prefabName, parent);
            if (instanceHandle.id != 0) {
                EDITOR_LOG_INFO("Created prefab instance '{}' with parent entity {}",
                    prefabName, parent.id);
            }
            else {
                EDITOR_LOG_ERROR("Failed to create prefab instance '{}'", prefabName);
            }
        }
    }
}

// Nowa funkcja startRename()
void HierarchyWindow::startRename(Entity entity) {
    m_renameEntity = entity;
    m_showRenameDialog = true;

    std::string currentName = getEntityDisplayName(entity);
    strncpy_s(m_renameBuffer, sizeof(m_renameBuffer), currentName.c_str(), _TRUNCATE);
    m_renameBuffer[sizeof(m_renameBuffer) - 1] = '\0';
}

// Nowa funkcja handleRenameDialog()
void HierarchyWindow::handleRenameDialog() {
    if (m_showRenameDialog) {
        ImGui::OpenPopup("Rename Entity");
        m_showRenameDialog = false;
    }

    if (ImGui::BeginPopupModal("Rename Entity", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Rename entity:");

        ImGui::SetKeyboardFocusHere();
        bool enterPressed = ImGui::InputText("##rename", m_renameBuffer, sizeof(m_renameBuffer),
            ImGuiInputTextFlags_EnterReturnsTrue);

        ImGui::Separator();

        if (ImGui::Button("OK") || enterPressed) {
            if (m_registry.entities().valid(m_renameEntity)) {
                std::string newName = m_renameBuffer;
                if (!newName.empty()) {
                    // Check if it's a prefab instance root
                    if (m_registry.prefabs().isEntityPartOfInstance(m_renameEntity) &&
                        m_registry.prefabs().isInstanceRoot(m_renameEntity)) {
                        // Rename the instance
                        PrefabInstanceHandle instanceHandle = m_registry.prefabs().getInstanceForEntity(m_renameEntity);
                        // Note: You might need to add a renameInstance method to PrefabInstanceManager
                        // For now, just rename the entity
                        m_registry.entities().setEntityName(m_renameEntity, newName);
                        EDITOR_LOG_INFO("Renamed prefab instance root to: {}", newName);
                    }
                    else {
                        // Regular entity rename
                        m_registry.entities().setEntityName(m_renameEntity, newName);
                        EDITOR_LOG_INFO("Renamed entity to: {}", newName);
                    }
                }
            }
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}
#include "EditorUI.h"
#include "Engine.h"
#include "Registry.h"
#include "FrameManager.h"
#include "Renderer.h"
#include "imgui.h"
#include "LoggerManager.h"
#include "AssetLoader.h"
#include "Paths.h"
#include <filesystem>
#include <algorithm>

EditorUI::EditorUI(Engine& engine)
    : m_engine(engine)
    , m_registry(engine.registry())
    , m_frameManager(engine.renderer().frameManager())
    , m_showOpenSceneDialog(false)
    , m_showSaveSceneDialog(false)
    , m_selectedSceneIndex(-1)
{
    EDITOR_LOG_INFO("Initializing EditorUI");

    // Create selection manager first
    m_selectionManager = std::make_unique<SelectionManager>();

    // Create UI windows with selection manager
    m_hierarchyWindow = std::make_unique<HierarchyWindow>(m_registry, *m_selectionManager);
    m_componentInspectorWindow = std::make_unique<ComponentInspectorWindow>(m_registry, *m_selectionManager);
    m_materialCreatorWindow = std::make_unique<MaterialCreatorUI>();
    m_settingsWindow = std::make_unique<SettingsWindow>(m_engine.settings());

    // Clear scene name buffer
    memset(m_sceneNameBuffer, 0, sizeof(m_sceneNameBuffer));

    EDITOR_LOG_INFO("EditorUI initialized successfully");
}

EditorUI::~EditorUI() {
    EDITOR_LOG_INFO("EditorUI destroyed");
}

void EditorUI::renderWindows() {
    // Render main menu bar
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene")) {
                EDITOR_LOG_INFO("New Scene requested");
                createNewScene();
            }
            if (ImGui::MenuItem("Open Scene")) {
                EDITOR_LOG_INFO("Open Scene requested");
                m_showOpenSceneDialog = true;
                refreshSceneList();
            }
            if (ImGui::MenuItem("Save Scene")) {
                EDITOR_LOG_INFO("Save Scene requested");
                if (m_registry.scenes().hasCurrentScene()) {
                    // Save current scene with current name
                    saveCurrentScene();
                }
                else {
                    // No current scene, show save as dialog
                    m_showSaveSceneDialog = true;
                }
            }
            if (ImGui::MenuItem("Save Scene As...")) {
                EDITOR_LOG_INFO("Save Scene As requested");
                m_showSaveSceneDialog = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) {
                EDITOR_LOG_INFO("Exit requested");
                m_engine.requestShutdown();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Windows")) {
            ImGui::MenuItem("Hierarchy", nullptr, &m_hierarchyWindow->m_showWindow);
            ImGui::MenuItem("Component Inspector", nullptr, &m_componentInspectorWindow->m_showWindow);
            ImGui::MenuItem("Material Creator", nullptr, &m_materialCreatorWindow->m_showWindow);
            ImGui::MenuItem("Settings", nullptr, &m_settingsWindow->m_showWindow);
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    // Render scene dialogs
    renderOpenSceneDialog();
    renderSaveSceneDialog();

    // Render individual windows
    m_hierarchyWindow->render();
    m_componentInspectorWindow->render();
    m_materialCreatorWindow->render();
    m_settingsWindow->render();

    // Render engine stats window
    if (ImGui::Begin("Engine Stats")) {
        ImGui::Text("FPS: %.1f", m_engine.fps());
        ImGui::Text("Frame Time: %.3f ms", m_engine.deltaTime() * 1000.0f);
        ImGui::Text("Total Time: %.2f s", m_engine.totalTime());
        ImGui::Text("Frame Count: %llu", m_engine.frameCount());

        ImGui::Separator();

        // Registry stats
        auto rootEntities = m_registry.entities().getRootEntities();
        ImGui::Text("Root Entities: %zu", rootEntities.size());
        ImGui::Text("Total Entities: %zu", m_registry.entities().getAllEntities().size());

        if (m_registry.scenes().hasCurrentScene()) {
            ImGui::Text("Current Scene: %s", m_registry.scenes().getCurrentSceneName().c_str());
        }
        else {
            ImGui::Text("No scene loaded");
        }
    }
    ImGui::End();
}

void EditorUI::createNewScene() {
    // Clear current scene
    m_registry.scenes().clearScene();

    EDITOR_LOG_INFO("Created new empty scene");
}

void EditorUI::saveCurrentScene() {
    if (!m_registry.scenes().hasCurrentScene()) {
        EDITOR_LOG_WARN("No current scene to save");
        return;
    }

    const std::string& currentSceneName = m_registry.scenes().getCurrentSceneName();
    if (m_registry.scenes().saveScene(currentSceneName)) {
        EDITOR_LOG_INFO("Successfully saved scene: {}", currentSceneName);
    }
    else {
        EDITOR_LOG_ERROR("Failed to save scene: {}", currentSceneName);
    }
}

void EditorUI::refreshSceneList() {
    m_availableScenes.clear();

    // Construct scenes directory path
    std::string scenesDir = std::string(ASSETS_COMP) + AssetLoader::GetAssetSubdirectory(AssetLib::AssetType::Scene);

    try {
        if (std::filesystem::exists(scenesDir) && std::filesystem::is_directory(scenesDir)) {
            for (const auto& entry : std::filesystem::directory_iterator(scenesDir)) {
                if (entry.is_regular_file()) {
                    std::string filename = entry.path().stem().string(); // Get filename without extension
                    m_availableScenes.push_back(filename);
                }
            }
        }

        // Sort scenes alphabetically
        std::sort(m_availableScenes.begin(), m_availableScenes.end());

        EDITOR_LOG_INFO("Found {} available scenes", m_availableScenes.size());
    }
    catch (const std::exception& e) {
        EDITOR_LOG_ERROR("Failed to scan scenes directory: {}", e.what());
    }
}

void EditorUI::renderOpenSceneDialog() {
    if (!m_showOpenSceneDialog) return;

    ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Open Scene", &m_showOpenSceneDialog)) {
        ImGui::Text("Available Scenes:");
        ImGui::Separator();

        // List available scenes
        if (m_availableScenes.empty()) {
            ImGui::Text("No scenes found");
        }
        else {
            for (size_t i = 0; i < m_availableScenes.size(); ++i) {
                bool isSelected = (m_selectedSceneIndex == static_cast<int>(i));

                if (ImGui::Selectable(m_availableScenes[i].c_str(), isSelected)) {
                    m_selectedSceneIndex = static_cast<int>(i);
                }

                // Double-click to load
                if (isSelected && ImGui::IsMouseDoubleClicked(0)) {
                    loadScene(m_availableScenes[i]);
                    m_showOpenSceneDialog = false;
                    m_selectedSceneIndex = -1;
                }
            }
        }

        ImGui::Separator();

        // Buttons
        bool hasSelection = (m_selectedSceneIndex >= 0 && m_selectedSceneIndex < static_cast<int>(m_availableScenes.size()));

        if (ImGui::Button("Load") && hasSelection) {
            loadScene(m_availableScenes[m_selectedSceneIndex]);
            m_showOpenSceneDialog = false;
            m_selectedSceneIndex = -1;
        }

        ImGui::SameLine();

        if (ImGui::Button("Refresh")) {
            refreshSceneList();
            m_selectedSceneIndex = -1;
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel")) {
            m_showOpenSceneDialog = false;
            m_selectedSceneIndex = -1;
        }
    }
    ImGui::End();
}

void EditorUI::renderSaveSceneDialog() {
    if (!m_showSaveSceneDialog) return;

    ImGui::SetNextWindowSize(ImVec2(400, 150), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Save Scene", &m_showSaveSceneDialog)) {
        ImGui::Text("Scene Name:");

        // Input field for scene name
        ImGui::InputText("##SceneName", m_sceneNameBuffer, sizeof(m_sceneNameBuffer));

        // Suggest current scene name if available
        if (m_registry.scenes().hasCurrentScene()) {
            ImGui::SameLine();
            if (ImGui::Button("Use Current")) {
                const std::string& currentName = m_registry.scenes().getCurrentSceneName();
                strncpy_s(m_sceneNameBuffer, sizeof(m_sceneNameBuffer), currentName.c_str(), currentName.length());
            }
        }

        ImGui::Separator();

        // Buttons
        bool hasValidName = (strlen(m_sceneNameBuffer) > 0);

        if (ImGui::Button("Save") && hasValidName) {
            saveSceneAs(std::string(m_sceneNameBuffer));
            m_showSaveSceneDialog = false;
            memset(m_sceneNameBuffer, 0, sizeof(m_sceneNameBuffer));
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel")) {
            m_showSaveSceneDialog = false;
            memset(m_sceneNameBuffer, 0, sizeof(m_sceneNameBuffer));
        }

        if (!hasValidName) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Please enter a scene name");
        }
    }
    ImGui::End();
}

void EditorUI::loadScene(const std::string& sceneName) {
    if (m_registry.scenes().loadScene(sceneName)) {
        EDITOR_LOG_INFO("Successfully loaded scene: {}", sceneName);
    }
    else {
        EDITOR_LOG_ERROR("Failed to load scene: {}", sceneName);
    }
}

void EditorUI::saveSceneAs(const std::string& sceneName) {
    if (m_registry.scenes().saveScene(sceneName)) {
        EDITOR_LOG_INFO("Successfully saved scene as: {}", sceneName);
    }
    else {
        EDITOR_LOG_ERROR("Failed to save scene as: {}", sceneName);
    }
}
//#pragma once
//#include "MaterialCreator.h"
//#include "ShaderLib.h"
//#include <memory>
//#include <string>
//#include <vector>
//#include <optional>
//
//// ============================================================================
//// FORWARD DECLARATIONS
//// ============================================================================
//
//class ParameterEditorWidget;
//class ShaderSelectorWidget;
//class TextureBrowserWidget;
//
//// ============================================================================
//// SHADER RESOURCE
//// ============================================================================
//
//struct ShaderResource {
//    std::string name;
//    std::string path;
//    ShaderLib::ShaderMetadata metadata;
//
//    bool isValid() const { return !name.empty() && !path.empty(); }
//};
//
//// ============================================================================
//// TEXTURE RESOURCE
//// ============================================================================
//
//struct TextureResource {
//    std::string name;
//    std::string path;
//
//    bool isValid() const { return !name.empty() && !path.empty(); }
//};
//
//// ============================================================================
//// UI STATE
//// ============================================================================
//
//struct MaterialCreatorUIState {
//    // Current definition being edited
//    MaterialCreator::MaterialDefinition currentDefinition;
//
//    // Selected shader
//    std::optional<ShaderResource> selectedShader;
//
//    // Available resources
//    std::vector<ShaderResource> availableShaders;
//    std::vector<TextureResource> availableTextures;
//
//    // UI status
//    std::string statusMessage;
//    float statusTimer = 0.0f;
//    bool showSuccess = false;
//    bool showError = false;
//
//    // Validation
//    std::optional<MaterialCreator::ValidationResult> lastValidation;
//
//    void clearStatus();
//    void setSuccess(const std::string& message, float duration = 3.0f);
//    void setError(const std::string& message, float duration = 5.0f);
//    void updateTimer(float deltaTime);
//};
//
//// ============================================================================
//// RESOURCE MANAGER
//// ============================================================================
//
//class MaterialResourceManager {
//public:
//    MaterialResourceManager();
//
//    void refreshShaders();
//    void refreshTextures();
//
//    const std::vector<ShaderResource>& getShaders() const { return m_shaders; }
//    const std::vector<TextureResource>& getTextures() const { return m_textures; }
//
//    std::optional<ShaderResource> findShader(const std::string& name) const;
//    std::optional<TextureResource> findTexture(const std::string& name) const;
//
//private:
//    std::vector<ShaderResource> m_shaders;
//    std::vector<TextureResource> m_textures;
//
//    void loadShadersFromDirectory(const std::string& directory);
//    void loadTexturesFromDirectory(const std::string& directory);
//};
//
//// ============================================================================
//// PARAMETER EDITOR WIDGET
//// ============================================================================
//
//class ParameterEditorWidget {
//public:
//    ParameterEditorWidget(MaterialResourceManager& resourceManager);
//
//    /**
//     * Render editor for a single parameter
//     * @return true if parameter was modified
//     */
//    bool render(MaterialCreator::ParameterDefinition& parameter);
//
//private:
//    MaterialResourceManager& m_resourceManager;
//
//    bool renderBufferParameter(MaterialCreator::ParameterDefinition& param);
//    bool renderTextureParameter(MaterialCreator::ParameterDefinition& param);
//    bool renderCompositeParameter(MaterialCreator::ParameterDefinition& param);
//
//    bool renderBaseTypeValue(ShaderLib::BufferValue& value, ShaderLib::BaseType type);
//    bool renderStructValue(std::shared_ptr<ShaderLib::ShaderStructInstance>& structInst);
//    bool renderArrayValue(std::shared_ptr<ShaderLib::ShaderArrayInstance>& arrayInst);
//};
//
//// ============================================================================
//// SHADER SELECTOR WIDGET
//// ============================================================================
//
//class ShaderSelectorWidget {
//public:
//    ShaderSelectorWidget(MaterialResourceManager& resourceManager);
//
//    /**
//     * Render shader selection combo
//     * @return Selected shader if changed, nullopt otherwise
//     */
//    std::optional<ShaderResource> render();
//
//    void setSelected(const std::string& shaderName);
//    const std::optional<ShaderResource>& getSelected() const { return m_selected; }
//
//private:
//    MaterialResourceManager& m_resourceManager;
//    std::optional<ShaderResource> m_selected;
//    int m_selectedIndex = -1;
//
//    void updateSelectedIndex();
//};
//
//// ============================================================================
//// MAIN UI CLASS
//// ============================================================================
//
//class MaterialCreatorUI {
//public:
//    MaterialCreatorUI();
//    ~MaterialCreatorUI();
//
//    void render();
//
//    bool m_showWindow = false;
//
//private:
//    // ========================================
//    // RENDERING
//    // ========================================
//
//    void renderHeader();
//    void renderMaterialProperties();
//    void renderShaderSelection();
//    void renderParametersList();
//    void renderActionButtons();
//    void renderStatusBar();
//    void renderValidationResults();
//
//    // ========================================
//    // ACTIONS
//    // ========================================
//
//    void onShaderSelected(const ShaderResource& shader);
//    void onCreateMaterial();
//    void onValidateMaterial();
//    void onResetForm();
//    void onRefreshResources();
//
//    // ========================================
//    // HELPERS
//    // ========================================
//
//    std::string generateOutputPath() const;
//    bool canCreateMaterial() const;
//    void autoGenerateParameters();
//
//    // ========================================
//    // STATE
//    // ========================================
//
//    MaterialCreatorUIState m_state;
//    std::unique_ptr<MaterialCreator> m_creator;
//    std::unique_ptr<MaterialResourceManager> m_resourceManager;
//
//    // Widgets
//    std::unique_ptr<ParameterEditorWidget> m_parameterEditor;
//    std::unique_ptr<ShaderSelectorWidget> m_shaderSelector;
//
//    // UI buffers
//    char m_materialNameBuffer[64] = "";
//};
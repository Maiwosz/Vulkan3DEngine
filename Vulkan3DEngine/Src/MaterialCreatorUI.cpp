//#include "MaterialCreatorUI.h"
//#include "imgui.h"
//#include "AssetLoader.h"
//#include "Paths.h"
//#include <filesystem>
//#include <spdlog/spdlog.h>
//
//// ============================================================================
//// UI STATE
//// ============================================================================
//
//void MaterialCreatorUIState::clearStatus() {
//    statusMessage.clear();
//    showSuccess = false;
//    showError = false;
//    statusTimer = 0.0f;
//}
//
//void MaterialCreatorUIState::setSuccess(const std::string& message, float duration) {
//    statusMessage = message;
//    showSuccess = true;
//    showError = false;
//    statusTimer = duration;
//}
//
//void MaterialCreatorUIState::setError(const std::string& message, float duration) {
//    statusMessage = message;
//    showError = true;
//    showSuccess = false;
//    statusTimer = duration;
//}
//
//void MaterialCreatorUIState::updateTimer(float deltaTime) {
//    if (statusTimer > 0.0f) {
//        statusTimer -= deltaTime;
//        if (statusTimer <= 0.0f) {
//            clearStatus();
//        }
//    }
//}
//
//// ============================================================================
//// RESOURCE MANAGER
//// ============================================================================
//
//MaterialResourceManager::MaterialResourceManager() {
//    refreshShaders();
//    refreshTextures();
//}
//
//void MaterialResourceManager::refreshShaders() {
//    m_shaders.clear();
//    std::string shadersDir = std::string(ASSETS_COMP) +
//        AssetLoader::GetAssetSubdirectory(AssetType::Shader);
//    loadShadersFromDirectory(shadersDir);
//}
//
//void MaterialResourceManager::refreshTextures() {
//    m_textures.clear();
//    std::string texturesDir = std::string(ASSETS_COMP) +
//        AssetLoader::GetAssetSubdirectory(AssetType::Texture);
//    loadTexturesFromDirectory(texturesDir);
//}
//
//std::optional<ShaderResource> MaterialResourceManager::findShader(const std::string& name) const {
//    for (const auto& shader : m_shaders) {
//        if (shader.name == name) {
//            return shader;
//        }
//    }
//    return std::nullopt;
//}
//
//std::optional<TextureResource> MaterialResourceManager::findTexture(const std::string& name) const {
//    for (const auto& texture : m_textures) {
//        if (texture.name == name) {
//            return texture;
//        }
//    }
//    return std::nullopt;
//}
//
//void MaterialResourceManager::loadShadersFromDirectory(const std::string& directory) {
//    try {
//        if (!std::filesystem::exists(directory)) {
//            SPDLOG_WARN("Shader directory does not exist: {}", directory);
//            return;
//        }
//
//        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
//            if (!entry.is_regular_file()) continue;
//
//            std::string extension = entry.path().extension().string();
//            if (extension != ".ashd") continue;
//
//            try {
//                AssetLib::AssetData assetData = AssetLib::ReadAsset(entry.path().string());
//                auto [metadata, stages] = AssetLib::ReadShader(assetData);
//
//                ShaderResource shader;
//                shader.name = entry.path().stem().string();
//                shader.path = entry.path().string();
//                shader.metadata = metadata;
//
//                m_shaders.push_back(shader);
//
//            }
//            catch (const std::exception& e) {
//                SPDLOG_WARN("Failed to load shader {}: {}", entry.path().string(), e.what());
//            }
//        }
//
//        SPDLOG_INFO("Loaded {} shaders", m_shaders.size());
//
//    }
//    catch (const std::exception& e) {
//        SPDLOG_ERROR("Error loading shaders from {}: {}", directory, e.what());
//    }
//}
//
//void MaterialResourceManager::loadTexturesFromDirectory(const std::string& directory) {
//    try {
//        if (!std::filesystem::exists(directory)) {
//            SPDLOG_WARN("Texture directory does not exist: {}", directory);
//            return;
//        }
//
//        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
//            if (!entry.is_regular_file()) continue;
//
//            std::string extension = entry.path().extension().string();
//            if (extension != ".atex") continue;
//
//            TextureResource texture;
//            texture.name = entry.path().stem().string();
//            texture.path = entry.path().string();
//
//            m_textures.push_back(texture);
//        }
//
//        SPDLOG_INFO("Loaded {} textures", m_textures.size());
//
//    }
//    catch (const std::exception& e) {
//        SPDLOG_ERROR("Error loading textures from {}: {}", directory, e.what());
//    }
//}
//
//// ============================================================================
//// PARAMETER EDITOR WIDGET
//// ============================================================================
//
//ParameterEditorWidget::ParameterEditorWidget(MaterialResourceManager& resourceManager)
//    : m_resourceManager(resourceManager) {
//}
//
//bool ParameterEditorWidget::render(MaterialCreator::ParameterDefinition& parameter) {
//    bool modified = false;
//
//    ImGui::PushID(parameter.name.c_str());
//
//    if (ImGui::CollapsingHeader(parameter.name.c_str())) {
//        ImGui::Indent();
//
//        ImGui::Text("Name: %s", parameter.name.c_str());
//
//        if (parameter.isTextureParameter()) {
//            modified = renderTextureParameter(parameter);
//        }
//        else if (parameter.isBufferParameter()) {
//            modified = renderBufferParameter(parameter);
//        }
//
//        ImGui::Unindent();
//    }
//
//    ImGui::PopID();
//
//    return modified;
//}
//
//bool ParameterEditorWidget::renderBufferParameter(MaterialCreator::ParameterDefinition& param) {
//    bool modified = false;
//
//    if (!std::holds_alternative<ShaderLib::BufferValue>(param.value)) {
//        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Invalid buffer parameter");
//        return false;
//    }
//
//    auto& bufVal = std::get<ShaderLib::BufferValue>(param.value);
//
//    // Check if composite type
//    if (auto* structInst = std::get_if<std::shared_ptr<ShaderLib::ShaderStructInstance>>(&bufVal)) {
//        modified = renderStructValue(*structInst);
//    }
//    else if (auto* arrayInst = std::get_if<std::shared_ptr<ShaderLib::ShaderArrayInstance>>(&bufVal)) {
//        modified = renderArrayValue(*arrayInst);
//    }
//    else {
//        // Base type
//        ShaderLib::BaseType baseType = ShaderLib::GetBaseTypeFromVariant(bufVal);
//        modified = renderBaseTypeValue(bufVal, baseType);
//    }
//
//    return modified;
//}
//
//bool ParameterEditorWidget::renderTextureParameter(MaterialCreator::ParameterDefinition& param) {
//    bool modified = false;
//
//    if (!std::holds_alternative<Material::TextureParam>(param.value)) {
//        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Invalid texture parameter");
//        return false;
//    }
//
//    auto& texParam = std::get<Material::TextureParam>(param.value);
//
//    // Texture selection
//    const auto& textures = m_resourceManager.getTextures();
//
//    if (!textures.empty()) {
//        std::vector<const char*> textureNames;
//        textureNames.push_back("(none)");
//
//        for (const auto& tex : textures) {
//            textureNames.push_back(tex.name.c_str());
//        }
//
//        int currentIndex = 0;
//        for (size_t i = 0; i < textures.size(); ++i) {
//            if (textures[i].path == texParam.handle.filename) {
//                currentIndex = static_cast<int>(i + 1);
//                break;
//            }
//        }
//
//        if (ImGui::Combo("Texture", &currentIndex, textureNames.data(),
//            static_cast<int>(textureNames.size()))) {
//            if (currentIndex == 0) {
//                texParam.handle.filename = "";
//            }
//            else {
//                texParam.handle.filename = textures[currentIndex - 1].path;
//            }
//            modified = true;
//        }
//    }
//
//    ImGui::Text("Path: %s", texParam.handle.filename.empty() ? "(none)" : texParam.handle.filename.c_str());
//
//    // Color space selection
//    const char* colorSpaces[] = { "Linear", "sRGB", "HDR" };
//    int colorSpaceIndex = static_cast<int>(texParam.colorSpace);
//    if (ImGui::Combo("Color Space", &colorSpaceIndex, colorSpaces, IM_ARRAYSIZE(colorSpaces))) {
//        texParam.colorSpace = static_cast<AssetLib::ColorSpace>(colorSpaceIndex);
//        modified = true;
//    }
//
//    return modified;
//}
//
//bool ParameterEditorWidget::renderBaseTypeValue(
//    ShaderLib::BufferValue& value,
//    ShaderLib::BaseType type
//) {
//    bool modified = false;
//
//    std::visit([&modified](auto& val) {
//        using T = std::decay_t<decltype(val)>;
//
//        if constexpr (std::is_same_v<T, float>) {
//            modified = ImGui::DragFloat("Value", &val, 0.01f);
//        }
//        else if constexpr (std::is_same_v<T, glm::vec2>) {
//            modified = ImGui::DragFloat2("Value", &val.x, 0.01f);
//        }
//        else if constexpr (std::is_same_v<T, glm::vec3>) {
//            modified = ImGui::DragFloat3("Value", &val.x, 0.01f);
//        }
//        else if constexpr (std::is_same_v<T, glm::vec4>) {
//            modified = ImGui::DragFloat4("Value", &val.x, 0.01f);
//        }
//        else if constexpr (std::is_same_v<T, int32_t>) {
//            modified = ImGui::DragInt("Value", &val);
//        }
//        else if constexpr (std::is_same_v<T, bool>) {
//            modified = ImGui::Checkbox("Value", &val);
//        }
//        else if constexpr (std::is_same_v<T, glm::mat4>) {
//            ImGui::Text("Matrix4x4");
//            if (ImGui::Button("Reset to Identity")) {
//                val = glm::mat4(1.0f);
//                modified = true;
//            }
//        }
//        else if constexpr (std::is_same_v<T, glm::mat3>) {
//            ImGui::Text("Matrix3x3");
//            if (ImGui::Button("Reset to Identity")) {
//                val = glm::mat3(1.0f);
//                modified = true;
//            }
//        }
//        else if constexpr (std::is_same_v<T, uint32_t>) {
//            int temp = static_cast<int>(val);
//            if (ImGui::DragInt("Value", &temp, 1.0f, 0)) {
//                val = static_cast<uint32_t>(std::max(0, temp));
//                modified = true;
//            }
//        }
//        else if constexpr (std::is_same_v<T, glm::ivec2>) {
//            modified = ImGui::DragInt2("Value", &val.x);
//        }
//        else if constexpr (std::is_same_v<T, glm::ivec3>) {
//            modified = ImGui::DragInt3("Value", &val.x);
//        }
//        else if constexpr (std::is_same_v<T, glm::ivec4>) {
//            modified = ImGui::DragInt4("Value", &val.x);
//        }
//        else {
//            ImGui::Text("Unsupported type for editing");
//        }
//        }, value);
//
//    return modified;
//}
//
//bool ParameterEditorWidget::renderStructValue(
//    std::shared_ptr<ShaderLib::ShaderStructInstance>& structInst
//) {
//    bool modified = false;
//
//    if (!structInst) {
//        ImGui::Text("(null struct)");
//        return false;
//    }
//
//    auto def = structInst->GetStructDefinition();
//    ImGui::Text("Struct: %s", def->GetTypeName().c_str());
//
//    ImGui::Indent();
//
//    for (const auto& field : def->GetFields()) {
//        ImGui::PushID(field.name.c_str());
//
//        if (field.IsComposite()) {
//            ImGui::Text("%s: (composite)", field.name.c_str());
//            // Could recursively edit composite fields here
//        }
//        else {
//            auto fieldValue = structInst->GetField(field.name);
//            if (renderBaseTypeValue(fieldValue, field.baseType)) {
//                structInst->SetField(field.name, fieldValue);
//                modified = true;
//            }
//        }
//
//        ImGui::PopID();
//    }
//
//    ImGui::Unindent();
//
//    return modified;
//}
//
//bool ParameterEditorWidget::renderArrayValue(
//    std::shared_ptr<ShaderLib::ShaderArrayInstance>& arrayInst
//) {
//    bool modified = false;
//
//    if (!arrayInst) {
//        ImGui::Text("(null array)");
//        return false;
//    }
//
//    auto def = arrayInst->GetArrayDefinition();
//    ImGui::Text("Array[%u]", def->GetArrayCount());
//
//    ImGui::Indent();
//
//    for (uint32_t i = 0; i < def->GetArrayCount(); ++i) {
//        ImGui::PushID(static_cast<int>(i));
//
//        if (def->IsCompositeElement()) {
//            ImGui::Text("[%u]: (composite)", i);
//            // Could edit composite elements here
//        }
//        else {
//            auto elemValue = arrayInst->GetElement(i);
//            if (renderBaseTypeValue(elemValue, def->GetElementBaseType())) {
//                arrayInst->SetElement(i, elemValue);
//                modified = true;
//            }
//        }
//
//        ImGui::PopID();
//    }
//
//    ImGui::Unindent();
//
//    return modified;
//}
//
//// ============================================================================
//// SHADER SELECTOR WIDGET
//// ============================================================================
//
//ShaderSelectorWidget::ShaderSelectorWidget(MaterialResourceManager& resourceManager)
//    : m_resourceManager(resourceManager) {
//}
//
//std::optional<ShaderResource> ShaderSelectorWidget::render() {
//    std::optional<ShaderResource> result;
//
//    const auto& shaders = m_resourceManager.getShaders();
//
//    if (shaders.empty()) {
//        ImGui::Text("No shaders available");
//        if (ImGui::Button("Refresh")) {
//            m_resourceManager.refreshShaders();
//        }
//        return result;
//    }
//
//    // Prepare combo items
//    std::vector<const char*> shaderNames;
//    shaderNames.reserve(shaders.size());
//    for (const auto& shader : shaders) {
//        shaderNames.push_back(shader.name.c_str());
//    }
//
//    int previousIndex = m_selectedIndex;
//
//    if (ImGui::Combo("Shader", &m_selectedIndex, shaderNames.data(),
//        static_cast<int>(shaderNames.size()))) {
//        if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(shaders.size())) {
//            m_selected = shaders[m_selectedIndex];
//            result = m_selected;
//        }
//    }
//
//    ImGui::SameLine();
//    if (ImGui::Button("Refresh")) {
//        m_resourceManager.refreshShaders();
//        updateSelectedIndex();
//    }
//
//    if (m_selected.has_value()) {
//        ImGui::Text("Selected: %s", m_selected->name.c_str());
//
//        // Show shader info
//        const auto& metadata = m_selected->metadata;
//        const auto* customSet = metadata.GetCustomSet();
//
//        if (customSet) {
//            ImGui::Text("Parameters: %zu buffers, %zu samplers",
//                customSet->buffers.size(),
//                customSet->GetAllSamplers().size());
//        }
//    }
//
//    return result;
//}
//
//void ShaderSelectorWidget::setSelected(const std::string& shaderName) {
//    auto shader = m_resourceManager.findShader(shaderName);
//    if (shader.has_value()) {
//        m_selected = shader;
//        updateSelectedIndex();
//    }
//}
//
//void ShaderSelectorWidget::updateSelectedIndex() {
//    if (!m_selected.has_value()) {
//        m_selectedIndex = -1;
//        return;
//    }
//
//    const auto& shaders = m_resourceManager.getShaders();
//    for (size_t i = 0; i < shaders.size(); ++i) {
//        if (shaders[i].name == m_selected->name) {
//            m_selectedIndex = static_cast<int>(i);
//            return;
//        }
//    }
//
//    m_selectedIndex = -1;
//}
//
//// ============================================================================
//// MAIN UI CLASS
//// ============================================================================
//
//MaterialCreatorUI::MaterialCreatorUI() {
//    m_creator = std::make_unique<MaterialCreator>();
//    m_resourceManager = std::make_unique<MaterialResourceManager>();
//    m_parameterEditor = std::make_unique<ParameterEditorWidget>(*m_resourceManager);
//    m_shaderSelector = std::make_unique<ShaderSelectorWidget>(*m_resourceManager);
//
//    m_state.currentDefinition.sourceInfo = "MaterialCreatorUI";
//}
//
//MaterialCreatorUI::~MaterialCreatorUI() = default;
//
//void MaterialCreatorUI::render() {
//    if (!m_showWindow) return;
//
//    ImGui::SetNextWindowSize(ImVec2(600, 800), ImGuiCond_FirstUseEver);
//
//    if (ImGui::Begin("Material Creator", &m_showWindow)) {
//        renderHeader();
//        ImGui::Separator();
//
//        renderMaterialProperties();
//        ImGui::Separator();
//
//        renderShaderSelection();
//
//        if (m_state.selectedShader.has_value()) {
//            ImGui::Separator();
//            renderParametersList();
//        }
//
//        ImGui::Separator();
//        renderActionButtons();
//
//        ImGui::Separator();
//        renderStatusBar();
//
//        if (m_state.lastValidation.has_value()) {
//            renderValidationResults();
//        }
//
//        // Update timers
//        m_state.updateTimer(ImGui::GetIO().DeltaTime);
//    }
//    ImGui::End();
//}
//
//// ========================================
//// RENDERING
//// ========================================
//
//void MaterialCreatorUI::renderHeader() {
//    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Material Creator Tool");
//    ImGui::TextWrapped("Create new material assets with custom parameters");
//}
//
//void MaterialCreatorUI::renderMaterialProperties() {
//    ImGui::Text("Material Properties");
//
//    if (ImGui::InputText("Material Name", m_materialNameBuffer, sizeof(m_materialNameBuffer))) {
//        m_state.currentDefinition.materialName = m_materialNameBuffer;
//    }
//
//    if (ImGui::IsItemHovered()) {
//        ImGui::SetTooltip("Unique name for this material");
//    }
//}
//
//void MaterialCreatorUI::renderShaderSelection() {
//    ImGui::Text("Shader Selection");
//
//    auto selectedShader = m_shaderSelector->render();
//    if (selectedShader.has_value()) {
//        onShaderSelected(*selectedShader);
//    }
//}
//
//void MaterialCreatorUI::renderParametersList() {
//    if (m_state.currentDefinition.parameters.empty()) {
//        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
//            "No parameters. Click 'Auto-Generate' to create from shader.");
//        return;
//    }
//
//    ImGui::Text("Parameters (%zu)", m_state.currentDefinition.parameters.size());
//
//    if (ImGui::BeginChild("ParametersList", ImVec2(0, -100), true)) {
//        for (auto& param : m_state.currentDefinition.parameters) {
//            m_parameterEditor->render(param);
//        }
//    }
//    ImGui::EndChild();
//}
//
//void MaterialCreatorUI::renderActionButtons() {
//    // Auto-generate button
//    if (m_state.selectedShader.has_value()) {
//        if (ImGui::Button("Auto-Generate Parameters", ImVec2(-1, 0))) {
//            autoGenerateParameters();
//        }
//
//        if (ImGui::IsItemHovered()) {
//            ImGui::SetTooltip("Generate parameters from selected shader metadata");
//        }
//    }
//
//    ImGui::Spacing();
//
//    // Main action buttons
//    float buttonWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2) / 3.0f;
//
//    // Validate button
//    if (ImGui::Button("Validate", ImVec2(buttonWidth, 40))) {
//        onValidateMaterial();
//    }
//
//    ImGui::SameLine();
//
//    // Create button
//    bool canCreate = canCreateMaterial();
//    if (!canCreate) {
//        ImGui::BeginDisabled();
//    }
//
//    if (ImGui::Button("Create Material", ImVec2(buttonWidth, 40))) {
//        onCreateMaterial();
//    }
//
//    if (!canCreate) {
//        ImGui::EndDisabled();
//
//        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
//            std::string tooltip = "Cannot create:\n";
//            if (m_state.currentDefinition.materialName.empty()) {
//                tooltip += "- Material name is empty\n";
//            }
//            if (!m_state.selectedShader.has_value()) {
//                tooltip += "- No shader selected\n";
//            }
//            ImGui::SetTooltip("%s", tooltip.c_str());
//        }
//    }
//
//    ImGui::SameLine();
//
//    // Reset button
//    if (ImGui::Button("Reset", ImVec2(buttonWidth, 40))) {
//        onResetForm();
//    }
//}
//
//void MaterialCreatorUI::renderStatusBar() {
//    if (m_state.showSuccess) {
//        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f),
//            "SUCCESS: %s", m_state.statusMessage.c_str());
//    }
//    else if (m_state.showError) {
//        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f),
//            "ERROR: %s", m_state.statusMessage.c_str());
//    }
//}
//
//void MaterialCreatorUI::renderValidationResults() {
//    if (!m_state.lastValidation.has_value()) return;
//
//    const auto& validation = m_state.lastValidation.value();
//
//    if (ImGui::CollapsingHeader("Validation Results", ImGuiTreeNodeFlags_DefaultOpen)) {
//        if (validation.isValid) {
//            ImGui::TextColored(ImVec4(0, 1, 0, 1), "Valid");
//        }
//        else {
//            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Invalid");
//        }
//
//        if (!validation.errors.empty()) {
//            ImGui::Text("Errors:");
//            ImGui::Indent();
//            for (const auto& error : validation.errors) {
//                ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "- %s", error.c_str());
//            }
//            ImGui::Unindent();
//        }
//
//        if (!validation.warnings.empty()) {
//            ImGui::Text("Warnings:");
//            ImGui::Indent();
//            for (const auto& warning : validation.warnings) {
//                ImGui::TextColored(ImVec4(1, 1, 0, 1), "- %s", warning.c_str());
//            }
//            ImGui::Unindent();
//        }
//    }
//}
//
//// ========================================
//// ACTIONS
//// ========================================
//
//void MaterialCreatorUI::onShaderSelected(const ShaderResource& shader) {
//    m_state.selectedShader = shader;
//    m_state.currentDefinition.shaderName = shader.name;
//
//    // Auto-generate parameters
//    autoGenerateParameters();
//
//    m_state.setSuccess("Shader selected: " + shader.name);
//}
//
//void MaterialCreatorUI::onCreateMaterial() {
//    if (!canCreateMaterial()) {
//        m_state.setError("Cannot create material: validation failed");
//        return;
//    }
//
//    // Validate first
//    const ShaderLib::ShaderMetadata* metadata = nullptr;
//    if (m_state.selectedShader.has_value()) {
//        metadata = &m_state.selectedShader->metadata;
//    }
//
//    auto validation = m_creator->validateDefinition(m_state.currentDefinition, metadata);
//    m_state.lastValidation = validation;
//
//    if (!validation.isValid) {
//        m_state.setError("Validation failed. Check validation results below.", 10.0f);
//        return;
//    }
//
//    // Generate output path
//    std::string outputPath = generateOutputPath();
//
//    // Create material
//    bool success = m_creator->createMaterial(
//        m_state.currentDefinition,
//        outputPath,
//        AssetLib::CompressionType::LZ4,
//        1
//    );
//
//    if (success) {
//        m_state.setSuccess("Material created: " + outputPath, 5.0f);
//        onResetForm();
//    }
//    else {
//        m_state.setError("Failed to create material", 5.0f);
//    }
//}
//
//void MaterialCreatorUI::onValidateMaterial() {
//    const ShaderLib::ShaderMetadata* metadata = nullptr;
//    if (m_state.selectedShader.has_value()) {
//        metadata = &m_state.selectedShader->metadata;
//    }
//
//    auto validation = m_creator->validateDefinition(m_state.currentDefinition, metadata);
//    m_state.lastValidation = validation;
//
//    if (validation.isValid) {
//        m_state.setSuccess(std::string("Validation passed") +
//            (validation.warnings.empty() ? "" : " with warnings"));
//    }
//    else {
//        m_state.setError("Validation failed");
//    }
//}
//
//void MaterialCreatorUI::onResetForm() {
//    m_state.currentDefinition = MaterialCreator::MaterialDefinition();
//    m_state.currentDefinition.sourceInfo = "MaterialCreatorUI";
//    m_state.selectedShader.reset();
//    m_state.lastValidation.reset();
//
//    memset(m_materialNameBuffer, 0, sizeof(m_materialNameBuffer));
//
//    m_state.clearStatus();
//}
//
//void MaterialCreatorUI::onRefreshResources() {
//    m_resourceManager->refreshShaders();
//    m_resourceManager->refreshTextures();
//    m_state.setSuccess("Resources refreshed");
//}
//
//// ========================================
//// HELPERS
//// ========================================
//
//std::string MaterialCreatorUI::generateOutputPath() const {
//    std::string materialsDir = std::string(ASSETS_COMP) +
//        AssetLoader::GetAssetSubdirectory(AssetType::Material);
//
//    std::filesystem::create_directories(materialsDir);
//
//    std::string filename = m_state.currentDefinition.materialName;
//    if (filename.empty()) {
//        filename = "untitled_material";
//    }
//
//    if (filename.find(".amat") == std::string::npos) {
//        filename += ".amat";
//    }
//
//    return (std::filesystem::path(materialsDir) / filename).string();
//}
//
//bool MaterialCreatorUI::canCreateMaterial() const {
//    return !m_state.currentDefinition.materialName.empty() &&
//        m_state.selectedShader.has_value();
//}
//
//void MaterialCreatorUI::autoGenerateParameters() {
//    if (!m_state.selectedShader.has_value()) {
//        m_state.setError("No shader selected");
//        return;
//    }
//
//    // Generate parameters from shader
//    auto parameters = MaterialCreator::generateParametersFromShader(
//        m_state.selectedShader->metadata,
//        false,  // Don't include global UBO
//        false   // Don't include object UBO
//    );
//
//    m_state.currentDefinition.parameters = parameters;
//
//    m_state.setSuccess("Generated " + std::to_string(parameters.size()) + " parameters");
//}
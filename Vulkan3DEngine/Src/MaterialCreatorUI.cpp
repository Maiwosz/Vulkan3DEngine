#include "MaterialCreatorUI.h"
#include "imgui.h"
#include "AssetLoader.h"
#include "Paths.h"
#include <filesystem>
#include <spdlog/spdlog.h>
#include <glm/glm.hpp>

MaterialCreatorUI::MaterialCreatorUI() {
    m_materialCreator = std::make_unique<MaterialCreator>();
    resetDefinition();
    loadAvailableShaders();
    loadAvailableTextures();
}

MaterialCreatorUI::~MaterialCreatorUI() = default;

void MaterialCreatorUI::render() {
    if (!m_showWindow) return;

    if (ImGui::Begin("Material Creator", &m_showWindow)) {
        renderMaterialDefinition();
        ImGui::Separator();
        renderShaderSelection();

        // Only show parameters if shader is selected
        if (m_selectedShaderIndex >= 0) {
            ImGui::Separator();
            renderParametersList();
        }

        ImGui::Separator();
        renderCreateButton();

        // Update message timer
        if (m_showSuccessMessage || m_showErrorMessage) {
            m_messageTimer -= ImGui::GetIO().DeltaTime;
            if (m_messageTimer <= 0.0f) {
                m_showSuccessMessage = false;
                m_showErrorMessage = false;
            }
        }

        // Show status messages
        if (m_showSuccessMessage) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%s", m_statusMessage.c_str());
        }
        if (m_showErrorMessage) {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%s", m_statusMessage.c_str());
        }
    }
    ImGui::End();
}

void MaterialCreatorUI::renderMaterialDefinition() {
    ImGui::Text("Material Definition");
    ImGui::InputText("Material Name", m_materialNameBuffer, sizeof(m_materialNameBuffer));
    m_currentDefinition.materialName = m_materialNameBuffer;
}

void MaterialCreatorUI::renderShaderSelection() {
    ImGui::Text("Shader Selection");

    if (ImGui::Button("Refresh Shaders")) {
        loadAvailableShaders();
    }

    if (m_availableShaders.empty()) {
        ImGui::Text("No shaders found");
        return;
    }

    // Create combo items
    std::vector<const char*> shaderNames;
    shaderNames.reserve(m_availableShaders.size());
    for (const auto& shader : m_availableShaders) {
        shaderNames.push_back(shader.name.c_str());
    }

    int previousSelection = m_selectedShaderIndex;
    if (ImGui::Combo("Available Shaders", &m_selectedShaderIndex, shaderNames.data(), static_cast<int>(shaderNames.size()))) {
        if (m_selectedShaderIndex != previousSelection && m_selectedShaderIndex >= 0) {
            onShaderSelected(m_selectedShaderIndex);
        }
    }

    if (m_selectedShaderIndex >= 0 && m_selectedShaderIndex < static_cast<int>(m_availableShaders.size())) {
        const auto& selectedShader = m_availableShaders[m_selectedShaderIndex];
        ImGui::Text("Selected: %s", selectedShader.name.c_str());
        ImGui::Text("Parameters: %zu", m_currentDefinition.parameters.size());
    }
}

void MaterialCreatorUI::renderParametersList() {
    if (m_currentDefinition.parameters.empty()) {
        ImGui::Text("No parameters found in selected shader");
        return;
    }

    ImGui::Text("Shader Parameters (%zu)", m_currentDefinition.parameters.size());
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Parameters are automatically generated from the selected shader");

    // Render existing parameters
    for (int i = 0; i < static_cast<int>(m_currentDefinition.parameters.size()); ++i) {
        ImGui::PushID(i);

        if (ImGui::CollapsingHeader(m_currentDefinition.parameters[i].name.c_str())) {
            renderParameterEditor(m_currentDefinition.parameters[i], i);
        }

        ImGui::PopID();
    }
}

void MaterialCreatorUI::renderParameterEditor(MaterialCreator::ParameterDefinition& param, int index) {
    ImGui::Text("Name: %s", param.name.c_str());

    if (param.descriptorType == ShaderLib::DescriptorType::UniformBuffer) {
        ImGui::Text("Type: Uniform");

        // Must be BufferValue
        if (!std::holds_alternative<ShaderLib::BufferValue>(param.defaultValue)) {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Invalid parameter type");
            return;
        }

        auto& bufVal = std::get<ShaderLib::BufferValue>(param.defaultValue);

        std::visit([&](auto& value) {
            using T = std::decay_t<decltype(value)>;

            if constexpr (std::is_same_v<T, float>) {
                ImGui::DragFloat("Value", &value, 0.01f);
            }
            else if constexpr (std::is_same_v<T, glm::vec2>) {
                ImGui::DragFloat2("Value", &value.x, 0.01f);
            }
            else if constexpr (std::is_same_v<T, glm::vec3>) {
                ImGui::DragFloat3("Value", &value.x, 0.01f);
            }
            else if constexpr (std::is_same_v<T, glm::vec4>) {
                ImGui::DragFloat4("Value", &value.x, 0.01f);
            }
            else if constexpr (std::is_same_v<T, int32_t>) {
                ImGui::DragInt("Value", &value);
            }
            else if constexpr (std::is_same_v<T, bool>) {
                ImGui::Checkbox("Value", &value);
            }
            else if constexpr (std::is_same_v<T, glm::mat4>) {
                ImGui::Text("Matrix4x4 (identity default)");
                if (ImGui::Button("Reset to Identity")) {
                    value = glm::mat4(1.0f);
                }
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ShaderLib::ShaderStruct>> ||
                std::is_same_v<T, std::shared_ptr<ShaderLib::ShaderArray>>) {
                ImGui::Text("Composite type (not editable in UI)");
            }
            else {
                ImGui::Text("Unsupported type");
            }
            }, bufVal);
    }
    else if (param.descriptorType == ShaderLib::DescriptorType::Sampler2D) {
        ImGui::Text("Type: Texture");

        if (std::holds_alternative<Material::TextureParam>(param.defaultValue)) {
            auto& textureParam = std::get<Material::TextureParam>(param.defaultValue);

            // Texture selection dropdown
            if (!m_availableTextures.empty()) {
                std::vector<const char*> textureNames;
                textureNames.push_back("(none)");
                for (const auto& texture : m_availableTextures) {
                    textureNames.push_back(texture.name.c_str());
                }

                int currentTextureIndex = 0;
                for (int j = 0; j < static_cast<int>(m_availableTextures.size()); ++j) {
                    if (m_availableTextures[j].path == textureParam.handle.filename) {
                        currentTextureIndex = j + 1;
                        break;
                    }
                }

                if (ImGui::Combo("Texture", &currentTextureIndex, textureNames.data(), static_cast<int>(textureNames.size()))) {
                    if (currentTextureIndex == 0) {
                        textureParam.handle.filename = "";
                    }
                    else {
                        textureParam.handle.filename = m_availableTextures[currentTextureIndex - 1].path;
                    }
                }
            }

            ImGui::Text("Path: %s", textureParam.handle.filename.c_str());

            // Color space selection
            const char* colorSpaces[] = { "Linear", "sRGB", "HDR" };
            int colorSpaceIndex = static_cast<int>(textureParam.colorSpace);
            if (ImGui::Combo("Color Space", &colorSpaceIndex, colorSpaces, IM_ARRAYSIZE(colorSpaces))) {
                textureParam.colorSpace = static_cast<AssetLib::ColorSpace>(colorSpaceIndex);
            }
        }
    }
}

void MaterialCreatorUI::renderCreateButton() {
    // Disable create button if no shader is selected
    bool canCreate = m_selectedShaderIndex >= 0 && !m_currentDefinition.materialName.empty();

    if (!canCreate) {
        ImGui::BeginDisabled();
    }

    if (ImGui::Button("Create Material", ImVec2(-1, 40))) {
        std::string errorMessage;
        if (m_materialCreator->validateDefinition(m_currentDefinition, errorMessage)) {
            std::string outputPath = getOutputPath();

            if (m_materialCreator->createMaterial(m_currentDefinition, outputPath)) {
                m_statusMessage = "Material created successfully: " + outputPath;
                m_showSuccessMessage = true;
                m_showErrorMessage = false;
                m_messageTimer = 3.0f;

                // Reset definition after successful creation
                resetDefinition();
            }
            else {
                m_statusMessage = "Failed to create material";
                m_showErrorMessage = true;
                m_showSuccessMessage = false;
                m_messageTimer = 3.0f;
            }
        }
        else {
            m_statusMessage = "Validation error: " + errorMessage;
            m_showErrorMessage = true;
            m_showSuccessMessage = false;
            m_messageTimer = 3.0f;
        }
    }

    if (!canCreate) {
        ImGui::EndDisabled();
        if (!m_currentDefinition.materialName.empty() && m_selectedShaderIndex < 0) {
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f), "Please select a shader first");
        }
        else if (m_currentDefinition.materialName.empty() && m_selectedShaderIndex >= 0) {
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f), "Please enter a material name");
        }
        else if (m_currentDefinition.materialName.empty() && m_selectedShaderIndex < 0) {
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f), "Please enter a material name and select a shader");
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("Reset")) {
        resetDefinition();
    }
}

void MaterialCreatorUI::resetDefinition() {
    m_currentDefinition = MaterialCreator::MaterialDefinition();
    m_currentDefinition.sourceInfo = "MaterialCreatorUI";

    memset(m_materialNameBuffer, 0, sizeof(m_materialNameBuffer));

    m_selectedShaderIndex = -1;

    m_statusMessage.clear();
    m_showSuccessMessage = false;
    m_showErrorMessage = false;
}

void MaterialCreatorUI::loadAvailableShaders() {
    m_availableShaders.clear();

    std::string shadersDir = std::string(ASSETS_COMP) + AssetLoader::GetAssetSubdirectory(AssetType::Shader);

    try {
        if (std::filesystem::exists(shadersDir)) {
            for (const auto& entry : std::filesystem::directory_iterator(shadersDir)) {
                if (entry.is_regular_file()) {
                    std::string extension = entry.path().extension().string();
                    if (extension == ".ashd") { // Shader asset extension
                        try {
                            // Load shader metadata
                            AssetLib::AssetData assetData = AssetLib::ReadAsset(entry.path().string());
                            auto [metadata, stages] = AssetLib::ReadShader(assetData);

                            ShaderOption option;
                            option.name = entry.path().stem().string();
                            option.path = entry.path().string();
                            option.metadata = metadata;

                            m_availableShaders.push_back(option);
                        }
                        catch (const std::exception& e) {
                            SPDLOG_WARN("Failed to load shader {}: {}", entry.path().string(), e.what());
                        }
                    }
                }
            }
        }
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Error loading shaders: {}", e.what());
    }

    m_shadersLoaded = true;
}

void MaterialCreatorUI::loadAvailableTextures() {
    m_availableTextures.clear();

    std::string texturesDir = std::string(ASSETS_COMP) + AssetLoader::GetAssetSubdirectory(AssetType::Texture);

    try {
        if (std::filesystem::exists(texturesDir)) {
            for (const auto& entry : std::filesystem::directory_iterator(texturesDir)) {
                if (entry.is_regular_file()) {
                    std::string extension = entry.path().extension().string();
                    if (extension == ".atex") { // Texture asset extension
                        TextureOption option;
                        option.name = entry.path().stem().string();
                        option.path = entry.path().string();
                        m_availableTextures.push_back(option);
                    }
                }
            }
        }
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Error loading textures: {}", e.what());
    }

    m_texturesLoaded = true;
}

void MaterialCreatorUI::onShaderSelected(int shaderIndex) {
    if (shaderIndex >= 0 && shaderIndex < static_cast<int>(m_availableShaders.size())) {
        const auto& shader = m_availableShaders[shaderIndex];

        // Set shader name in definition
        m_currentDefinition.shaderName = shader.name;

        // Automatically generate parameters from shader
        generateParametersFromShader(shader.metadata);
    }
}

void MaterialCreatorUI::generateParametersFromShader(const ShaderLib::ShaderMetadata& metadata) {
    // Clear existing parameters
    m_currentDefinition.parameters.clear();

    // Process custom buffers (set 2 - CUSTOM_DESCRIPTOR_SET)
    for (const auto& buffer : metadata.customBuffers) {
        if (buffer.set == ShaderLib::CUSTOM_DESCRIPTOR_SET) {
            if (buffer.IsUniformBuffer()) {
                // Add parameters for each variable in the buffer
                for (const auto& variable : buffer.variables) {
                    // Only handle base types, skip composite types
                    if (!variable.IsBase()) {
                        continue;
                    }

                    MaterialCreator::ParameterDefinition param;
                    param.name = variable.name;
                    param.descriptorType = ShaderLib::DescriptorType::UniformBuffer;
                    param.baseType = variable.baseType;
                    param.arraySize = 0;

                    // Set default values based on type
                    ShaderLib::BufferValue bufVal;
                    switch (variable.baseType) {
                    case ShaderLib::BaseType::Float:
                        bufVal = 0.0f;
                        break;
                    case ShaderLib::BaseType::Vec2:
                        bufVal = glm::vec2(0.0f);
                        break;
                    case ShaderLib::BaseType::Vec3:
                        bufVal = glm::vec3(0.0f);
                        break;
                    case ShaderLib::BaseType::Vec4:
                        bufVal = glm::vec4(0.0f);
                        break;
                    case ShaderLib::BaseType::Int:
                        bufVal = int32_t(0);
                        break;
                    case ShaderLib::BaseType::Bool:
                        bufVal = false;
                        break;
                    case ShaderLib::BaseType::Mat4:
                        bufVal = glm::mat4(1.0f);
                        break;
                    case ShaderLib::BaseType::Mat3:
                        bufVal = glm::mat3(1.0f);
                        break;
                    case ShaderLib::BaseType::Mat2:
                        bufVal = glm::mat2(1.0f);
                        break;
                    case ShaderLib::BaseType::UInt:
                        bufVal = uint32_t(0);
                        break;
                    case ShaderLib::BaseType::IVec2:
                        bufVal = glm::ivec2(0);
                        break;
                    case ShaderLib::BaseType::IVec3:
                        bufVal = glm::ivec3(0);
                        break;
                    case ShaderLib::BaseType::IVec4:
                        bufVal = glm::ivec4(0);
                        break;
                    case ShaderLib::BaseType::UVec2:
                        bufVal = glm::uvec2(0u);
                        break;
                    case ShaderLib::BaseType::UVec3:
                        bufVal = glm::uvec3(0u);
                        break;
                    case ShaderLib::BaseType::UVec4:
                        bufVal = glm::uvec4(0u);
                        break;
                    case ShaderLib::BaseType::Double:
                        bufVal = 0.0;
                        break;
                    case ShaderLib::BaseType::DVec2:
                        bufVal = glm::dvec2(0.0);
                        break;
                    case ShaderLib::BaseType::DVec3:
                        bufVal = glm::dvec3(0.0);
                        break;
                    case ShaderLib::BaseType::DVec4:
                        bufVal = glm::dvec4(0.0);
                        break;
                    default:
                        continue; // Skip unsupported types
                    }

                    param.defaultValue = Material::ParamValue{ bufVal };
                    m_currentDefinition.parameters.push_back(param);
                }
            }
        }
    }

    // Process texture descriptors
    for (const auto& descriptor : metadata.descriptors) {
        if (descriptor.set == ShaderLib::CUSTOM_DESCRIPTOR_SET) {
            if (descriptor.descriptorType == ShaderLib::DescriptorType::Sampler2D) {
                // Add texture parameter
                MaterialCreator::ParameterDefinition param = MaterialCreator::createTextureParam(
                    descriptor.name, "", AssetLib::ColorSpace::SRGB);
                m_currentDefinition.parameters.push_back(param);
            }
        }
    }

    m_statusMessage = "Generated " + std::to_string(m_currentDefinition.parameters.size()) + " parameters from shader";
    m_showSuccessMessage = true;
    m_showErrorMessage = false;
    m_messageTimer = 2.0f;
}

std::vector<std::string> MaterialCreatorUI::getTextureList() const {
    std::vector<std::string> textureList;
    for (const auto& texture : m_availableTextures) {
        textureList.push_back(texture.name);
    }
    return textureList;
}

std::string MaterialCreatorUI::getOutputPath() const {
    std::string materialsDir = std::string(ASSETS_COMP) + AssetLoader::GetAssetSubdirectory(AssetType::Material);

    // Ensure directory exists
    std::filesystem::create_directories(materialsDir);

    std::string filename = m_currentDefinition.materialName;
    if (filename.empty()) {
        filename = "untitled_material";
    }

    // Add extension if not present
    if (filename.find(".amat") == std::string::npos) {
        filename += ".amat";
    }

    return (std::filesystem::path(materialsDir) / filename).string();
}

void MaterialCreatorUI::safeStringCopy(char* dest, const std::string& src, size_t destSize) {
    if (destSize == 0) return;

    size_t copySize = std::min(src.size(), destSize - 1);
    if (copySize > 0) {
        std::copy_n(src.c_str(), copySize, dest);
    }
    dest[copySize] = '\0';
}
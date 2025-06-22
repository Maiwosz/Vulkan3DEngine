#pragma once

#include <memory>
#include <string>
#include <vector>
#include "MaterialCreator.h"
#include "ShaderLib.h"

struct ShaderOption {
    std::string name;
    std::string path;
    ShaderLib::ShaderMetadata metadata;
};

struct TextureOption {
    std::string name;
    std::string path;
};

class MaterialCreatorUI {
public:
    MaterialCreatorUI();
    ~MaterialCreatorUI();

    void render();

    bool m_showWindow = false;

private:
    void renderMaterialDefinition();
    void renderShaderSelection();
    void renderParametersList();
    void renderParameterEditor(MaterialCreator::ParameterDefinition& param, int index);
    void renderCreateButton();

    void resetDefinition();

    // Shader management
    void loadAvailableShaders();
    void onShaderSelected(int shaderIndex);
    void generateParametersFromShader(const ShaderLib::ShaderMetadata& metadata);

    // Texture management
    void loadAvailableTextures();
    std::vector<std::string> getTextureList() const;

    std::string getOutputPath() const;

    // Safe string copy helper
    void safeStringCopy(char* dest, const std::string& src, size_t destSize);

private:
    std::unique_ptr<MaterialCreator> m_materialCreator;
    MaterialCreator::MaterialDefinition m_currentDefinition;

    // Shader selection
    std::vector<ShaderOption> m_availableShaders;
    int m_selectedShaderIndex = -1;
    bool m_shadersLoaded = false;

    // Texture selection
    std::vector<TextureOption> m_availableTextures;
    bool m_texturesLoaded = false;

    // UI state
    char m_materialNameBuffer[64] = "";

    // Status messages
    std::string m_statusMessage;
    bool m_showSuccessMessage = false;
    bool m_showErrorMessage = false;
    float m_messageTimer = 0.0f;
};
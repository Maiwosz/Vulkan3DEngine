#pragma once
#include "AssetLib.h"
#include "Material.h"
#include "ShaderLib.h"
#include <string>
#include <vector>
#include <variant>
#include <unordered_map>
#include <memory>
#include <glm/glm.hpp>

/**
 * MaterialCreator - klasa do tworzenia nowych materiałów i zapisywania ich do plików
 */
class MaterialCreator {
public:
    // Struktura opisująca parametr materiału do utworzenia
    struct ParameterDefinition {
        std::string name;
        ShaderLib::DescriptorType descriptorType;
        ShaderLib::UniformType uniformType;
        Material::ParamValue defaultValue;
        uint32_t arraySize = 0; // 0 oznacza brak tablicy

        // Dla parametrów tekstury
        AssetLib::SamplerDescription samplerDesc{};

        ParameterDefinition() = default;

        // Konstruktor dla parametrów uniformowych
        ParameterDefinition(
            const std::string& paramName,
            ShaderLib::UniformType uType,
            const Material::ParamValue& defValue,
            uint32_t arrSize = 0
        ) : name(paramName),
            descriptorType(ShaderLib::DescriptorType::UniformBuffer),
            uniformType(uType),
            defaultValue(defValue),
            arraySize(arrSize) {
        }

        // Konstruktor dla parametrów tekstury
        ParameterDefinition(
            const std::string& paramName,
            const std::string& texturePath,
            const AssetLib::SamplerDescription& sampler,
            AssetLib::ColorSpace colorSpace = AssetLib::ColorSpace::SRGB
        ) : name(paramName),
            descriptorType(ShaderLib::DescriptorType::CombinedImageSampler),
            uniformType(ShaderLib::UniformType::Unknown),
            arraySize(0),
            samplerDesc(sampler) {

            Material::TextureParam textureParam;
            textureParam.handle = AssetHandle(AssetType::Texture, texturePath);
            textureParam.colorSpace = colorSpace;
            defaultValue = textureParam;
        }
    };

    // Struktura opisująca materiał do utworzenia
    struct MaterialDefinition {
        std::string materialName;
        std::string shaderName;
        std::vector<ParameterDefinition> parameters;
        std::string sourceInfo; // Opcjonalna informacja o źródle materiału
    };

    MaterialCreator();
    ~MaterialCreator();

    // Główna metoda do tworzenia i zapisywania materiału
    bool createMaterial(
        const MaterialDefinition& definition,
        const std::string& outputPath,
        AssetLib::CompressionType compression = AssetLib::CompressionType::LZ4,
        int compressionLevel = 1
    );

    // Pomocnicze metody do tworzenia domyślnych samplerów
    static AssetLib::SamplerDescription createDefaultSampler();
    static AssetLib::SamplerDescription createLinearSampler();
    static AssetLib::SamplerDescription createNearestSampler();
    static AssetLib::SamplerDescription createClampedSampler();

    // Pomocnicze metody do tworzenia definicji parametrów
    static ParameterDefinition createFloatParam(const std::string& name, float defaultValue);
    static ParameterDefinition createVec2Param(const std::string& name, const glm::vec2& defaultValue);
    static ParameterDefinition createVec3Param(const std::string& name, const glm::vec3& defaultValue);
    static ParameterDefinition createVec4Param(const std::string& name, const glm::vec4& defaultValue);
    static ParameterDefinition createIntParam(const std::string& name, int32_t defaultValue);
    static ParameterDefinition createBoolParam(const std::string& name, bool defaultValue);
    static ParameterDefinition createMat4Param(const std::string& name, const glm::mat4& defaultValue);

    static ParameterDefinition createTextureParam(
        const std::string& name,
        const std::string& texturePath,
        AssetLib::ColorSpace colorSpace = AssetLib::ColorSpace::SRGB,
        const AssetLib::SamplerDescription& sampler = createDefaultSampler()
    );

    // Walidacja definicji materiału
    bool validateDefinition(const MaterialDefinition& definition, std::string& errorMessage) const;

    // Sprawdzenie czy plik materiału już istnieje
    static bool materialExists(const std::string& path);

private:
    // Konwersja Material::ParamValue do danych binarnych
    std::vector<uint8_t> serializeParameterValue(const Material::ParamValue& value) const;

    // Obliczenie rozmiaru parametru w bajtach
    size_t getParameterSize(const Material::ParamValue& value) const;

    // Konwersja ParameterDefinition do AssetLib::MaterialParameter
    AssetLib::MaterialParameter convertToAssetParameter(
        const ParameterDefinition& paramDef,
        uint32_t dataOffset,
        uint32_t dataSize
    ) const;

    // Generowanie automatycznej ścieżki dla materiału
    std::string generateMaterialPath(const std::string& materialName, const std::string& baseDir) const;

    // Walidacja parametrów
    bool validateParameter(const ParameterDefinition& param, std::string& errorMessage) const;

    // Sprawdzenie zgodności typu parametru z wartością domyślną
    bool isValueTypeCompatible(ShaderLib::UniformType uniformType, const Material::ParamValue& value) const;
};

// Pomocnicze funkcje dla łatwego tworzenia materiałów
namespace MaterialCreatorUtils {
    // Szybkie tworzenie prostego materiału z podstawowymi parametrami
    MaterialCreator::MaterialDefinition createBasicMaterial(
        const std::string& name,
        const std::string& shader,
        const std::vector<std::pair<std::string, std::string>>& textures = {}
    );

    // Tworzenie materiału PBR z typowymi parametrami
    MaterialCreator::MaterialDefinition createPBRMaterial(
        const std::string& name,
        const std::string& shader,
        const std::string& albedoTexture = "",
        const std::string& normalTexture = "",
        const std::string& metallicRoughnessTexture = "",
        const glm::vec3& albedoColor = glm::vec3(1.0f),
        float metallic = 0.0f,
        float roughness = 0.5f
    );
}
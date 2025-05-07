#pragma once
#include <unordered_map>
#include <vector>
#include <memory>
#include "ShaderModule.h"
#include "ShaderModuleHandle.h"
#include <string>
#include <AssetLib.h>
#include "UniformBufferManager.h"
#include "DescriptorLayoutManager.h"
#include "PipelineLayoutManager.h"

// Handle dla całego shadera (zawierającego wszystkie etapy)
struct ShaderHandle {
    uint32_t id;
    constexpr explicit ShaderHandle(uint32_t id = 0) : id(id) {}
    bool operator==(const ShaderHandle&) const = default;
    explicit operator bool() const { return id != 0; }
};

// Hash function dla ShaderHandle
namespace std {
    template <>
    struct hash<ShaderHandle> {
        size_t operator()(const ShaderHandle& handle) const {
            return hash<uint32_t>()(handle.id);
        }
    };
}

struct CombinedShader {
    std::unordered_map<ShaderLib::Stage, ShaderModuleHandle> stages;
};

// Struktura przechowująca zasoby dla programu shaderowego
struct ShaderResources {
    std::unordered_map<uint32_t, DescriptorLayoutHandle> descriptorLayouts; // Numer seta -> uchwyt layoutu
    PipelineLayoutHandle pipelineLayout;
    std::unordered_map<std::string, UniformBufferHandle> uniformBuffers; // Nazwa bufora -> uchwyt bufora
};

class ShaderModuleManager {
public:
    ShaderModuleManager(
        const LogicalDevice& device,
        UniformBufferManager& uniformBufferManager,
        DescriptorLayoutManager& descriptorLayoutManager,
        PipelineLayoutManager& pipelineLayoutManager
    );
    ~ShaderModuleManager();

    // Tworzenie modułu shadera z kodu SPIRV
    ShaderModuleHandle createModuleFromSPIRV(const std::vector<uint32_t>& spirvCode);

    ShaderModuleHandle createModuleFromSPIRVFile(const std::string& filePath);

    // Tworzenie pełnego shadera z metadanych i skompilowanych etapów
    ShaderHandle createShader(
        const ShaderLib::ShaderMetadata& metadata,
        const std::vector<ShaderLib::CompiledStage>& stages
    );

    // Niszczenie modułu shadera
    void destroyModule(ShaderModuleHandle handle);

    // Niszczenie całego shadera
    void destroyShader(ShaderHandle handle);

    // Pobieranie modułu shadera za pomocą uchwytu
    ShaderModule& getModule(ShaderModuleHandle handle);

    // Pobieranie modułu shadera dla konkretnego etapu z kompletnego shadera
    ShaderModule* getModuleForStage(ShaderHandle shader, ShaderLib::Stage stage);

    // Pobieranie combined shadera
    const CombinedShader& getCombinedShader(ShaderHandle handle) const;

    // Pobieranie zasobów shadera
    const ShaderResources& getShaderResources(ShaderHandle handle) const;

    // Pobieranie metadanych shadera
    const ShaderLib::ShaderMetadata& getShaderMetadata(ShaderHandle handle) const;

    // Sprawdzanie czy uchwyt modułu shadera jest poprawny
    bool isModuleValid(ShaderModuleHandle handle) const;

    // Sprawdzanie czy uchwyt shadera jest poprawny
    bool isShaderValid(ShaderHandle handle) const;

    // Nowe metody do obsługi buforów uniform

    // Tworzenie globalnego bufora uniform
    UniformBufferHandle createGlobalUniformBuffer(ShaderHandle shaderHandle);

    // Tworzenie obiektowego bufora uniform
    UniformBufferHandle createObjectUniformBuffer(ShaderHandle shaderHandle);

    // Tworzenie niestandardowego bufora uniform
    UniformBufferHandle createCustomUniformBuffer(ShaderHandle shaderHandle, const std::string& name);

    // Pobieranie bufora uniform z puli lub tworzenie nowego
    UniformBufferHandle acquireUniformBuffer(ShaderHandle shaderHandle, const std::string& name);

    // Zwracanie bufora uniform do puli
    void releaseUniformBuffer(UniformBufferHandle handle);

    // Aktualizacja danych w buforze uniform
    void updateUniformBuffer(UniformBufferHandle handle, const void* data, uint32_t size, uint32_t offset = 0);

    // Aktualizacja konkretnej zmiennej w buforze uniform
    template<typename T>
    void updateUniformVariable(
        UniformBufferHandle handle,
        const std::string& variableName,
        const T& value
    ) {
        m_uniformBufferManager.updateVariable(handle, variableName, value);
    }

private:
    // Tworzenie layoutów deskryptorów na podstawie metadanych shadera
    std::unordered_map<uint32_t, DescriptorLayoutHandle> createDescriptorLayouts(
        const ShaderLib::ShaderMetadata& metadata
    );

    // Tworzenie layoutu potoku na podstawie metadanych shadera i layoutów deskryptorów
    PipelineLayoutHandle createPipelineLayout(
        const ShaderLib::ShaderMetadata& metadata,
        const std::unordered_map<uint32_t, DescriptorLayoutHandle>& descriptorLayouts
    );

    // Tworzenie buforów uniform na podstawie metadanych shadera
    std::unordered_map<std::string, UniformBufferHandle> createUniformBuffers(
        const ShaderLib::ShaderMetadata& metadata
    );

    const LogicalDevice& m_device;
    UniformBufferManager& m_uniformBufferManager;
    DescriptorLayoutManager& m_descriptorLayoutManager;
    PipelineLayoutManager& m_pipelineLayoutManager;

    // Przechowywanie modułów shaderów
    std::unordered_map<ShaderModuleHandle, std::unique_ptr<ShaderModule>> m_modules;

    // Przechowywanie kompletnych shaderów
    std::unordered_map<ShaderHandle, CombinedShader> m_combinedShaders;
    std::unordered_map<ShaderHandle, ShaderResources> m_shaderResources;
    std::unordered_map<ShaderHandle, ShaderLib::ShaderMetadata> m_shaderMetadata;

    // Liczniki dla generowania unikatowych uchwytów
    uint32_t m_nextModuleHandle = 1;
    uint32_t m_nextShaderHandle = 1;

    
};
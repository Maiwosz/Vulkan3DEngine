#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include "ShaderManager.h"
#include "BufferManager.h"
#include "AssetHandle.h"
#include "ImageSamplerManager.h"
#include "TextureManager.h"
#include "DescriptorAllocator.h"
#include "DescriptorLayoutManager.h"
#include "BufferObjectInstance.h"
#include "BufferBundle.h"
#include "ShaderLib.h"
#include "ThreadPool.h"
#include "Handle.h"

// Forward declarations
class MaterialManager;

/**
 * Material - High-level wrapper for Custom Descriptor Set (set 2)
 *
 * Simplified design using BufferBundle:
 * - Delegates all buffer operations to BufferBundle
 * - Manages textures and descriptor sets
 * - Provides convenient field access API
 * - Thread-safe async operations via BufferBundle
 *
 * Design principles:
 * - BufferBundle handles all buffer sync AND metadata complexity
 * - Material focuses on descriptor set management and textures
 * - Clean separation of concerns
 */
class Material {
public:
    // Texture parameter
    struct TextureParam {
        AssetHandle assetHandle;
        TextureHandle textureHandle;
        SamplerHandle samplerHandle;
        AssetLib::ColorSpace colorSpace = AssetLib::ColorSpace::SRGB;
    };

    // Constructor
    Material(
        const std::string& name,
        SmartAssetHandle<ShaderHandle, ShaderAsset> shader,
        std::shared_ptr<ShaderLib::BufferObjectInstance> inputBuffer,
        std::shared_ptr<ShaderLib::BufferObjectInstance> outputBuffer,
        std::shared_ptr<ShaderLib::BufferObjectInstance> inputOutputBuffer,
        const LogicalDevice& device,
        BufferManager& bufferManager,
        ImageSamplerManager& samplerManager,
        TextureManager& textureManager,
        DescriptorAllocator& descriptorAllocator,
        DescriptorLayoutManager& descriptorLayoutManager,
        ThreadPool& threadPool
    );

    ~Material();

    // Basic accessors
    const std::string& GetName() const { return m_name; }
    const SmartAssetHandle<ShaderHandle, ShaderAsset>& GetShader() const { return m_shader; }

    // ==========================================================================
    // FIELD ACCESS - Full FieldProxy support with chaining
    // ==========================================================================

    ShaderLib::FieldProxy operator[](const std::string& name);
    ShaderLib::FieldProxy operator[](const char* name);
    ShaderLib::FieldProxy GetField(const std::string& path);

    template<typename T>
    T Get(const std::string& path) const;

    template<typename T>
    void Set(const std::string& path, const T& value);

    // ==========================================================================
    // FIELD METADATA - Delegated to BufferBundle
    // ==========================================================================

    bool HasField(const std::string& nameOrPath) const {
        return m_bufferBundle.HasField(nameOrPath);
    }

    std::vector<std::string> GetFieldNames() const {
        return m_bufferBundle.GetTopLevelFieldNames();
    }

    std::vector<std::string> GetAllFieldPaths() const {
        return m_bufferBundle.GetAllFieldPaths();
    }

    const BufferBundle::FieldInfo* GetFieldInfo(const std::string& nameOrPath) const {
        return m_bufferBundle.GetFieldInfo(nameOrPath);
    }

    bool IsArrayField(const std::string& name) const {
        return m_bufferBundle.IsArrayField(name);
    }

    size_t GetArraySize(const std::string& name) const {
        return m_bufferBundle.GetArraySize(name);
    }

    bool IsStructureField(const std::string& name) const {
        return m_bufferBundle.IsStructureField(name);
    }

    std::vector<std::string> GetStructureChildren(const std::string& name) const {
        return m_bufferBundle.GetStructureChildren(name);
    }

    // ==========================================================================
    // ARRAY FIELD OPERATIONS
    // ==========================================================================

    template<typename T>
    void SetArray(const std::string& name, const std::vector<T>& values);

    template<typename T>
    std::vector<T> GetArray(const std::string& name) const;

    // ==========================================================================
    // TEXTURE MANAGEMENT
    // ==========================================================================

    bool SetTexture(const std::string& name, const TextureParam& texture);
    bool GetTexture(const std::string& name, TextureParam& outTexture) const;
    std::vector<std::string> GetTextureNames() const;
    bool HasTexture(const std::string& name) const;

    // ==========================================================================
    // GPU SYNCHRONIZATION - SYNCHRONOUS (blocking)
    // ==========================================================================

    void SyncToGPU();
    void SyncFromGPU();

    void SyncBufferToGPU(const std::string& bufferIdentifier);
    void SyncBufferFromGPU(const std::string& bufferIdentifier);

    void SyncFieldsToGPU(const std::vector<std::string>& paths);
    void SyncFieldsFromGPU(const std::vector<std::string>& paths);

    // ==========================================================================
    // GPU SYNCHRONIZATION - ASYNCHRONOUS (non-blocking)
    // ==========================================================================

    std::vector<BufferSyncTaskHandle> SyncToGPUAsync();
    std::vector<BufferSyncTaskHandle> SyncFromGPUAsync();

    BufferSyncTaskHandle SyncBufferToGPUAsync(const std::string& bufferIdentifier);
    BufferSyncTaskHandle SyncBufferFromGPUAsync(const std::string& bufferIdentifier);

    std::vector<BufferSyncTaskHandle> SyncFieldsToGPUAsync(const std::vector<std::string>& paths);
    std::vector<BufferSyncTaskHandle> SyncFieldsFromGPUAsync(const std::vector<std::string>& paths);

    // ==========================================================================
    // ASYNC TASK MANAGEMENT
    // ==========================================================================

    bool AreTasksComplete(const std::vector<BufferSyncTaskHandle>& tasks) const;
    bool IsTaskComplete(BufferSyncTaskHandle task) const;
    void WaitForTasks(const std::vector<BufferSyncTaskHandle>& tasks);
    void WaitForTask(BufferSyncTaskHandle task);
    void WaitForAllTasks();
    void PollCompletedTasks();
    size_t GetActiveTaskCount() const;

    // ==========================================================================
    // DESCRIPTOR SET MANAGEMENT
    // ==========================================================================

    SmartHandle<DescriptorSetHandle, VkDescriptorSet> GetDescriptorSet();
    void InvalidateDescriptorSet();
    bool IsDescriptorSetValid() const { return m_descriptorSetValid; }

    // ==========================================================================
    // BUFFER ACCESS
    // ==========================================================================

    BufferBundle& GetBufferBundle() { return m_bufferBundle; }
    const BufferBundle& GetBufferBundle() const { return m_bufferBundle; }

    std::shared_ptr<ShaderLib::BufferObjectInstance> GetInputBuffer() const;
    std::shared_ptr<ShaderLib::BufferObjectInstance> GetOutputBuffer() const;
    std::shared_ptr<ShaderLib::BufferObjectInstance> GetInputOutputBuffer() const;

    bool HasInputBuffer() const;
    bool HasOutputBuffer() const;
    bool HasInputOutputBuffer() const;

private:
    // Texture binding
    struct TextureBinding {
        std::string name;
        uint32_t binding;
        ShaderLib::DescriptorType type;
        TextureParam texture;
    };

    // Initialization
    void BuildTextureBindings();
    void CollectSamplerHandles();

    // Descriptor set
    SmartHandle<DescriptorSetHandle, VkDescriptorSet> CreateDescriptorSet();
    bool NeedsDescriptorSetRecreation() const;
    void AcquireBuffersForDescriptorSet();
    void ReleaseDescriptorSetBuffers();

    // Helper methods
    const ShaderLib::DescriptorSet* GetCustomDescriptorSet() const;

    // Core data
    std::string m_name;
    SmartAssetHandle<ShaderHandle, ShaderAsset> m_shader;

    // Buffer management - delegated to BufferBundle
    BufferBundle m_bufferBundle;

    // Texture bindings (Material's primary responsibility)
    std::unordered_map<std::string, TextureBinding> m_textureBindings;

    // Dependencies
    const LogicalDevice& m_device;
    BufferManager& m_bufferManager;
    ImageSamplerManager& m_samplerManager;
    TextureManager& m_textureManager;
    DescriptorAllocator& m_descriptorAllocator;
    DescriptorLayoutManager& m_descriptorLayoutManager;

    // Descriptor set cache
    SmartHandle<DescriptorSetHandle, VkDescriptorSet> m_descriptorSet;
    bool m_descriptorSetValid;

    // Buffer handles for descriptor set
    SmartHandle<BufferHandle, Buffer> m_inputBufferHandle;
    SmartHandle<BufferHandle, Buffer> m_outputBufferHandle;
    SmartHandle<BufferHandle, Buffer> m_inputOutputBufferHandle;

    // Sampler handles for dirty checking
    std::vector<SamplerHandle> m_samplerHandles;

    friend class MaterialManager;
};

// =============================================================================
// TEMPLATE IMPLEMENTATIONS
// =============================================================================

template<typename T>
inline T Material::Get(const std::string& path) const {
    return m_bufferBundle.Get<T>(path);
}

template<typename T>
inline void Material::Set(const std::string& path, const T& value) {
    m_bufferBundle.Set(path, value);
}

template<typename T>
inline void Material::SetArray(const std::string& name, const std::vector<T>& values) {
    if (!IsArrayField(name)) {
        throw std::runtime_error("Material " + m_name + ": Field is not an array: " + name);
    }

    size_t arraySize = GetArraySize(name);
    if (values.size() > arraySize) {
        throw std::runtime_error("Material " + m_name + ": Array size mismatch for " + name);
    }

    for (size_t i = 0; i < values.size(); ++i) {
        operator[](name)[i] = values[i];
    }
}

template<typename T>
inline std::vector<T> Material::GetArray(const std::string& name) const {
    if (!IsArrayField(name)) {
        throw std::runtime_error("Material " + m_name + ": Field is not an array: " + name);
    }

    size_t arraySize = GetArraySize(name);
    std::vector<T> result;
    result.reserve(arraySize);

    for (size_t i = 0; i < arraySize; ++i) {
        std::string elementPath = name + "[" + std::to_string(i) + "]";
        result.push_back(m_bufferBundle.Get<T>(elementPath));
    }

    return result;
}

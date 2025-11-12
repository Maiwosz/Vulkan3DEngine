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
#include "Handle.h"

// Forward declarations
class MaterialManager;

/**
 * Material - High-level wrapper for Custom Descriptor Set (set 2)
 *
 * Design principles:
 * - GPU buffers are created immediately in constructor (always available)
 * - Descriptor sets are created lazily on first GetDescriptorSet() call
 * - BufferBundle manages all buffer instances and field routing
 * - Material is fully functional immediately after construction
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

    // Constructor - Creates GPU buffers immediately
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
        DescriptorLayoutManager& descriptorLayoutManager
    );

    ~Material();

    // Basic accessors
    const std::string& GetName() const { return m_name; }
    const SmartAssetHandle<ShaderHandle, ShaderAsset>& GetShader() const { return m_shader; }

    // ==========================================================================
    // BUFFER BUNDLE ACCESS
    // ==========================================================================

    BufferBundle& GetBufferBundle() { return m_bufferBundle; }
    const BufferBundle& GetBufferBundle() const { return m_bufferBundle; }

    // Direct buffer access (convenience wrappers)
    BufferBundle::BufferProxy GetInputBuffer() {
        return m_bufferBundle.GetBuffer("input");
    }
    BufferBundle::BufferProxy GetOutputBuffer() {
        return m_bufferBundle.GetBuffer("output");
    }
    BufferBundle::BufferProxy GetInputOutputBuffer() {
        return m_bufferBundle.GetBuffer("input_output");
    }

    bool HasInputBuffer() const { return m_bufferBundle.HasBuffer("input"); }
    bool HasOutputBuffer() const { return m_bufferBundle.HasBuffer("output"); }
    bool HasInputOutputBuffer() const { return m_bufferBundle.HasBuffer("input_output"); }

    // ==========================================================================
    // FIELD ACCESS - Delegates to BufferBundle
    // ==========================================================================

    ShaderLib::FieldProxy operator[](const std::string& name) {
        return m_bufferBundle.GetField(name);
    }

    ShaderLib::FieldProxy operator[](const char* name) {
        return m_bufferBundle.GetField(std::string(name));
    }

    template<typename T>
    T Get(const std::string& path) const {
        return m_bufferBundle.Get<T>(path);
    }

    template<typename T>
    void Set(const std::string& path, const T& value) {
        m_bufferBundle.Set(path, value);
    }

    // ==========================================================================
    // FIELD QUERIES - Delegates to BufferBundle
    // ==========================================================================

    std::vector<std::string> GetFieldNames() const {
        return m_bufferBundle.GetTopLevelFieldNames();
    }

    std::vector<std::string> GetAllFieldPaths() const {
        return m_bufferBundle.GetAllFieldPaths();
    }

    std::vector<std::string> GetFieldNames(const std::string& bufferIdentifier) const {
        return m_bufferBundle.GetFieldNames(bufferIdentifier);
    }

    bool HasField(const std::string& path) const {
        return m_bufferBundle.HasField(path);
    }

    bool IsArrayField(const std::string& path) const {
        return m_bufferBundle.IsArrayField(path);
    }

    bool IsStructureField(const std::string& path) const {
        return m_bufferBundle.IsStructureField(path);
    }

    std::vector<std::string> GetStructureChildren(const std::string& path) const {
        return m_bufferBundle.GetStructureChildren(path);
    }

    std::string GetBufferIdentifierForField(const std::string& path) const {
        return m_bufferBundle.GetBufferIdentifierForField(path);
    }

    // ==========================================================================
    // BULK BUFFER OPERATIONS
    // GPU buffers are always available, no need for initialization checks
    // ==========================================================================

    void SyncAllToGPU();
    void SyncAllFromGPU();

    std::vector<ShaderLib::AsyncOperationHandle> SyncAllToGPUAsync();
    std::vector<ShaderLib::AsyncOperationHandle> SyncAllFromGPUAsync();

    void WaitForAllBuffers();

    // ==========================================================================
    // TEXTURE MANAGEMENT
    // ==========================================================================

    bool SetTexture(const std::string& name, const TextureParam& texture);
    bool GetTexture(const std::string& name, TextureParam& outTexture) const;
    std::vector<std::string> GetTextureNames() const;
    bool HasTexture(const std::string& name) const;

    // ==========================================================================
    // DESCRIPTOR SET MANAGEMENT
    // Descriptor sets are created lazily on first access
    // ==========================================================================

    SmartHandle<DescriptorSetHandle, VkDescriptorSet> GetDescriptorSet();
    void InvalidateDescriptorSet();
    bool IsDescriptorSetValid() const { return m_descriptorSetValid; }

private:
    // Texture binding
    struct TextureBinding {
        std::string name;
        uint32_t binding;
        ShaderLib::DescriptorType type;
        TextureParam texture;
    };

    // Initialization - called from constructor
    void InitializeBufferBundle(
        std::shared_ptr<ShaderLib::BufferObjectInstance> inputBuffer,
        std::shared_ptr<ShaderLib::BufferObjectInstance> outputBuffer,
        std::shared_ptr<ShaderLib::BufferObjectInstance> inputOutputBuffer
    );
    void BuildTextureBindings();
    void CollectSamplerHandles();

    // NEW: Acquire GPU buffers immediately in constructor
    void AcquireGPUBuffers();

    // Descriptor set (lazy creation)
    SmartHandle<DescriptorSetHandle, VkDescriptorSet> CreateDescriptorSet();
    bool NeedsDescriptorSetRecreation() const;

    // Helper methods
    const ShaderLib::DescriptorSet* GetCustomDescriptorSet() const;

    // Core data
    std::string m_name;
    SmartAssetHandle<ShaderHandle, ShaderAsset> m_shader;

    // Buffer management through BufferBundle
    BufferBundle m_bufferBundle;

    // Texture bindings
    std::unordered_map<std::string, TextureBinding> m_textureBindings;

    // Dependencies
    const LogicalDevice& m_device;
    BufferManager& m_bufferManager;
    ImageSamplerManager& m_samplerManager;
    TextureManager& m_textureManager;
    DescriptorAllocator& m_descriptorAllocator;
    DescriptorLayoutManager& m_descriptorLayoutManager;

    // Descriptor set cache (lazy)
    SmartHandle<DescriptorSetHandle, VkDescriptorSet> m_descriptorSet;
    bool m_descriptorSetValid;

    // GPU buffer handles - created immediately in constructor
    SmartHandle<BufferHandle, Buffer> m_inputBufferHandle;
    SmartHandle<BufferHandle, Buffer> m_outputBufferHandle;
    SmartHandle<BufferHandle, Buffer> m_inputOutputBufferHandle;

    // Sampler handles for dirty checking
    std::vector<SamplerHandle> m_samplerHandles;

    friend class MaterialManager;
};

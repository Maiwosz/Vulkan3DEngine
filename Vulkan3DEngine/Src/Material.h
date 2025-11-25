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
 * - Direct buffer access by name (no routing layer)
 * - All buffer metadata comes from shader descriptor set definition
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
        const std::unordered_map<std::string, std::shared_ptr<ShaderLib::BufferObjectInstance>>& buffers,
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
    // BUFFER ACCESS BY NAME
    // ==========================================================================

    // Check if buffer exists
    bool HasBuffer(const std::string& name) const;

    // Get buffer instance (returns nullptr if not found)
    std::shared_ptr<ShaderLib::BufferObjectInstance> GetBuffer(const std::string& name);
    std::shared_ptr<const ShaderLib::BufferObjectInstance> GetBuffer(const std::string& name) const;

    // Get all buffer names
    std::vector<std::string> GetBufferNames() const;

    // Get buffer count
    size_t GetBufferCount() const { return m_buffers.size(); }

    // ==========================================================================
    // BULK BUFFER OPERATIONS
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
    // ==========================================================================

    SmartHandle<DescriptorSetHandle, VkDescriptorSet> GetDescriptorSet();
    void InvalidateDescriptorSet();
    bool IsDescriptorSetValid() const { return m_descriptorSetValid; }

private:
    // Buffer entry with GPU handle
    struct BufferEntry {
        std::string name;
        uint32_t binding;
        std::shared_ptr<ShaderLib::BufferObjectInstance> instance;
        SmartHandle<BufferHandle, Buffer> gpuHandle;
    };

    // Texture binding
    struct TextureBinding {
        std::string name;
        uint32_t binding;
        ShaderLib::DescriptorType type;
        TextureParam texture;
    };

    // Initialization
    void BuildBufferEntries(
        const std::unordered_map<std::string, std::shared_ptr<ShaderLib::BufferObjectInstance>>& buffers
    );
    void BuildTextureBindings();
    void CollectSamplerHandles();
    void AcquireGPUBuffers();

    // Descriptor set (lazy creation)
    SmartHandle<DescriptorSetHandle, VkDescriptorSet> CreateDescriptorSet();
    bool NeedsDescriptorSetRecreation() const;

    // Helper methods
    const ShaderLib::DescriptorSet* GetCustomDescriptorSet() const;

    // Core data
    std::string m_name;
    SmartAssetHandle<ShaderHandle, ShaderAsset> m_shader;

    // Buffer storage (name -> buffer entry)
    std::unordered_map<std::string, BufferEntry> m_buffers;

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

    // Sampler handles for dirty checking
    std::vector<SamplerHandle> m_samplerHandles;

    friend class MaterialManager;
};

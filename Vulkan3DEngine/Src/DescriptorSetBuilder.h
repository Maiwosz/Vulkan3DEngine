#pragma once
#include <ShaderLib.h>
#include "Handle.h"
#include "ISmartHandleManager.h"
#include "BufferManager.h"
#include "ImageSamplerManager.h"
#include "TextureManager.h"
#include "DescriptorAllocator.h"
#include "DescriptorLayoutManager.h"
#include "DescriptorWriter.h"
#include <unordered_map>
#include <variant>
#include <optional>

// High-level descriptor set builder that works with ShaderLib metadata
class DescriptorSetBuilder {
public:
    // Binding value types
    struct BufferBinding {
        SmartHandle<BufferHandle, Buffer> buffer;
    };

    struct TextureBinding {
        TextureHandle texture;
        SamplerHandle sampler;
    };

    struct ImageBinding {
        VkImageView imageView;
        SamplerHandle sampler;
    };

    using BindingValue = std::variant<BufferBinding, TextureBinding, ImageBinding>;

    DescriptorSetBuilder(
        const LogicalDevice& device,
        BufferManager& bufferManager,
        ImageSamplerManager& samplerManager,
        TextureManager& textureManager,
        DescriptorAllocator& descriptorAllocator,
        DescriptorLayoutManager& descriptorLayoutManager
    );

    // Set the descriptor set metadata to build from
    DescriptorSetBuilder& forDescriptorSet(
        const ShaderLib::DescriptorSet& descriptorSet,
        DescriptorLayoutHandle layoutHandle
    );

    // High-level binding methods using names
    DescriptorSetBuilder& bindBuffer(const std::string& name, SmartHandle<BufferHandle, Buffer> buffer);
    DescriptorSetBuilder& bindTexture(const std::string& name, TextureHandle texture, SamplerHandle sampler);
    DescriptorSetBuilder& bindImageView(const std::string& name, VkImageView imageView, SamplerHandle sampler);

    // Low-level binding methods using binding numbers
    DescriptorSetBuilder& bindBufferToSlot(uint32_t binding, SmartHandle<BufferHandle, Buffer> buffer);
    DescriptorSetBuilder& bindTextureToSlot(uint32_t binding, TextureHandle texture, SamplerHandle sampler);
    DescriptorSetBuilder& bindImageViewToSlot(uint32_t binding, VkImageView imageView, SamplerHandle sampler);

    // Build the descriptor set
    SmartHandle<DescriptorSetHandle, VkDescriptorSet> build();

    // Clear all bindings and reset builder
    void clear();

    // Validation
    bool validateBindings() const;
    std::vector<std::string> getMissingBindings() const;

private:
    struct PendingBinding {
        uint32_t binding;
        BindingValue value;
        const ShaderLib::DescriptorSlot* slot = nullptr;
    };

    // Helper methods
    const ShaderLib::DescriptorSlot* findSlotByName(const std::string& name) const;
    std::shared_ptr<const ShaderLib::BufferObjectDefinition> findBufferDefinitionByName(const std::string& name) const;

    void writeBindingsToWriter(DescriptorWriter& writer);
    void writeBufferBinding(DescriptorWriter& writer, uint32_t binding, const BufferBinding& bufferBinding);
    void writeTextureBinding(DescriptorWriter& writer, uint32_t binding, const TextureBinding& textureBinding);
    void writeImageBinding(DescriptorWriter& writer, uint32_t binding, const ImageBinding& imageBinding);

    VkDescriptorSetLayout getLayout() const;

    // Dependencies
    const LogicalDevice& m_device;
    BufferManager& m_bufferManager;
    ImageSamplerManager& m_samplerManager;
    TextureManager& m_textureManager;
    DescriptorAllocator& m_descriptorAllocator;
    DescriptorLayoutManager& m_descriptorLayoutManager;

    // Builder state
    const ShaderLib::DescriptorSet* m_descriptorSetMetadata = nullptr;
    DescriptorLayoutHandle m_layoutHandle;
    std::vector<PendingBinding> m_pendingBindings;
};

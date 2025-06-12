#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include "AttachmentManager.h"
#include "Handle.h"
#include "IResourceManager.h"

// Forward declarations
class LogicalDevice;

// Structure to uniquely identify render pass configurations
struct RenderPassConfig {
    struct AttachmentDesc {
        VkFormat format;
        VkSampleCountFlagBits samples;
        VkAttachmentLoadOp loadOp;
        VkAttachmentStoreOp storeOp;
        VkImageLayout initialLayout;
        VkImageLayout finalLayout;
        AttachmentType type;

        bool operator==(const AttachmentDesc& other) const {
            return format == other.format &&
                samples == other.samples &&
                loadOp == other.loadOp &&
                storeOp == other.storeOp &&
                initialLayout == other.initialLayout &&
                finalLayout == other.finalLayout &&
                type == other.type;
        }
    };

    std::vector<AttachmentDesc> attachments;
    std::vector<uint32_t> colorAttachmentIndices;
    uint32_t depthAttachmentIndex = UINT32_MAX;  // Invalid by default
    uint32_t resolveAttachmentIndex = UINT32_MAX;  // Invalid by default

    // Hash function for the config
    size_t hash() const;

    // Equality operator for unordered_map
    bool operator==(const RenderPassConfig& other) const;
};

// Custom hash implementation for RenderPassConfig
namespace std {
    template<> struct hash<RenderPassConfig> {
        size_t operator()(const RenderPassConfig& config) const {
            return config.hash();
        }
    };
}

class RenderPassManager : public IResourceManager<RenderPassHandle, VkRenderPass> {
public:
    RenderPassManager(const LogicalDevice& logicalDevice);
    ~RenderPassManager();

    // Delete copy constructors
    RenderPassManager(const RenderPassManager&) = delete;
    RenderPassManager& operator=(const RenderPassManager&) = delete;

    // Get or create a render pass with the specified configuration
    RenderPassHandle acquireRenderPass(const RenderPassConfig& config);
    
    // Recreate render pass with new configuration while keeping the same handle
    void recreateRenderPass(RenderPassHandle handle, const RenderPassConfig& newConfig);

    // IResourceManager interface implementation
    VkRenderPass* getResource(RenderPassHandle handle) override;
    bool isValid(RenderPassHandle handle) const override;
    void releaseResource(RenderPassHandle handle) override;
    void addReference(RenderPassHandle handle) override;
    void removeReference(RenderPassHandle handle) override;

private:
    // Create a new render pass from a configuration
    RenderPassHandle createRenderPass(const RenderPassConfig& config);
    // Helper function to create VkRenderPass from configuration
    VkRenderPass createVkRenderPass(const RenderPassConfig& config);

    const LogicalDevice& m_device;

    // Map from configuration hash to render pass handle
    std::unordered_map<RenderPassConfig, RenderPassHandle> m_configToHandle;

    // Map from handle to VkRenderPass and its configuration
    struct RenderPassEntry {
        VkRenderPass renderPass;
        RenderPassConfig config;
    };
    std::unordered_map<RenderPassHandle, RenderPassEntry> m_renderPasses;

    uint32_t m_nextHandleId = 1;
};
#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include "AttachmentManager.h"

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

// Handle for RenderPasses
struct RenderPassHandle {
    uint32_t id;

    constexpr explicit RenderPassHandle(uint32_t id = 0) : id(id) {}

    bool operator==(const RenderPassHandle&) const = default;
    bool operator<(const RenderPassHandle& other) const { return id < other.id; }
    explicit operator bool() const { return id != 0; }
};

namespace std {
    template<> struct hash<RenderPassHandle> {
        size_t operator()(const RenderPassHandle& handle) const {
            return hash<uint32_t>()(handle.id);
        }
    };
}

class RenderPassManager {
public:
    RenderPassManager(const LogicalDevice& logicalDevice);
    ~RenderPassManager();

    // Delete copy constructors
    RenderPassManager(const RenderPassManager&) = delete;
    RenderPassManager& operator=(const RenderPassManager&) = delete;

    // Get or create a render pass with the specified configuration
    RenderPassHandle acquireRenderPass(const RenderPassConfig& config);

    // Get an existing render pass by handle
    VkRenderPass getRenderPass(RenderPassHandle handle) const;

    // Check if handle is valid
    bool isValid(RenderPassHandle handle) const;

    // Destroy a specific render pass
    void destroy(RenderPassHandle handle);

    // Destroy all render passes
    void cleanup();

private:
    // Create a new render pass from a configuration
    RenderPassHandle createRenderPass(const RenderPassConfig& config);

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
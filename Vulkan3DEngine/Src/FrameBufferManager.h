#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>
#include <memory>
#include "RenderPassManager.h"
#include "AttachmentManager.h"

// Forward declarations
class LogicalDevice;

// Structure to uniquely identify framebuffer configurations
struct FrameBufferConfig {
    RenderPassHandle renderPassHandle;
    std::vector<AttachmentHandle> attachmentHandles;
    VkExtent2D extent;

    // Hash function for the config
    size_t hash() const;

    // Equality operator for unordered_map
    bool operator==(const FrameBufferConfig& other) const;
};

// Custom hash implementation for FrameBufferConfig
namespace std {
    template<> struct hash<FrameBufferConfig> {
        size_t operator()(const FrameBufferConfig& config) const {
            return config.hash();
        }
    };
}

// Handle for FrameBuffers
struct FrameBufferHandle {
    uint32_t id;

    constexpr explicit FrameBufferHandle(uint32_t id = 0) : id(id) {}

    bool operator==(const FrameBufferHandle&) const = default;
    bool operator<(const FrameBufferHandle& other) const { return id < other.id; }
    explicit operator bool() const { return id != 0; }
};

namespace std {
    template<> struct hash<FrameBufferHandle> {
        size_t operator()(const FrameBufferHandle& handle) const {
            return hash<uint32_t>()(handle.id);
        }
    };
}

class FrameBufferManager {
public:
    FrameBufferManager(const LogicalDevice& logicalDevice,
        const RenderPassManager& renderPassManager,
        const AttachmentManager& attachmentManager);
    ~FrameBufferManager();

    // Delete copy constructors
    FrameBufferManager(const FrameBufferManager&) = delete;
    FrameBufferManager& operator=(const FrameBufferManager&) = delete;

    // Get or create a framebuffer with the specified configuration
    FrameBufferHandle getOrCreate(const FrameBufferConfig& config);

    // Get or create a framebuffer using the render pass and attachments
    FrameBufferHandle getOrCreate(RenderPassHandle renderPassHandle,
        const std::vector<AttachmentHandle>& attachmentHandles,
        VkExtent2D extent);

    // Get an existing framebuffer by handle
    VkFramebuffer get(FrameBufferHandle handle) const;

    // Check if handle is valid
    bool isValid(FrameBufferHandle handle) const;

    // Destroy a specific framebuffer
    void destroy(FrameBufferHandle handle);

    // Handle window resize events - invalidate size-dependent framebuffers
    void onResize(VkExtent2D newExtent);

    // Destroy all framebuffers
    void cleanup();

private:
    // Create a new framebuffer from a configuration
    FrameBufferHandle createFrameBuffer(const FrameBufferConfig& config);

    const LogicalDevice& m_device;
    const RenderPassManager& m_renderPassManager;
    const AttachmentManager& m_attachmentManager;

    // Map from configuration hash to framebuffer handle
    std::unordered_map<FrameBufferConfig, FrameBufferHandle> m_configToHandle;

    // Map from handle to VkFramebuffer and its configuration
    struct FrameBufferEntry {
        VkFramebuffer frameBuffer;
        FrameBufferConfig config;
    };
    std::unordered_map<FrameBufferHandle, FrameBufferEntry> m_frameBuffers;

    // Keep track of size-dependent framebuffers
    std::vector<FrameBufferHandle> m_sizeDependent;

    uint32_t m_nextHandleId = 1;
};
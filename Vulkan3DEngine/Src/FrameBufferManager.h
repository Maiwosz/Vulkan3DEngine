#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>
#include <memory>
#include "RenderPassManager.h"
#include "AttachmentManager.h"
#include "IResourceManager.h"
#include "Handle.h"

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

// Wrapper for VkFramebuffer to be used as ResourceType
struct FrameBufferResource {
    VkFramebuffer frameBuffer;
    FrameBufferConfig config;
    uint32_t refCount = 0;

    FrameBufferResource(VkFramebuffer fb, const FrameBufferConfig& cfg)
        : frameBuffer(fb), config(cfg) {
    }
};

class FrameBufferManager : public IResourceManager<FrameBufferHandle, FrameBufferResource> {
public:
    FrameBufferManager(const LogicalDevice& logicalDevice,
        RenderPassManager& renderPassManager,
        AttachmentManager& attachmentManager);
    ~FrameBufferManager();

    // Delete copy constructors
    FrameBufferManager(const FrameBufferManager&) = delete;
    FrameBufferManager& operator=(const FrameBufferManager&) = delete;

    // IResourceManager interface implementation
    FrameBufferResource* getResource(FrameBufferHandle handle) override;
    bool isValid(FrameBufferHandle handle) const override;
    void releaseResource(FrameBufferHandle handle) override;
    void addReference(FrameBufferHandle handle) override;
    void removeReference(FrameBufferHandle handle) override;

    FrameBufferHandle acquireFrameBuffer(const FrameBufferConfig& config);
    FrameBufferHandle acquireFrameBuffer(RenderPassHandle renderPassHandle,
        const std::vector<AttachmentHandle>& attachmentHandles,
        VkExtent2D extent);

    // Handle window resize events - invalidate size-dependent framebuffers
    void onResize(VkExtent2D newExtent);
private:
    // Create a new framebuffer from a configuration
    FrameBufferHandle createFrameBuffer(const FrameBufferConfig& config);

    // Internal destroy method
    void destroyFrameBuffer(FrameBufferHandle handle);

    const LogicalDevice& m_device;
    RenderPassManager& m_renderPassManager;
    AttachmentManager& m_attachmentManager;

    // Map from configuration hash to framebuffer handle
    std::unordered_map<FrameBufferConfig, FrameBufferHandle> m_configToHandle;

    // Map from handle to resource
    std::unordered_map<FrameBufferHandle, std::unique_ptr<FrameBufferResource>> m_frameBuffers;

    // Keep track of size-dependent framebuffers
    std::vector<FrameBufferHandle> m_sizeDependent;

    // Pula dostępnych framebufferów (gotowych do ponownego użycia)
    std::unordered_map<FrameBufferConfig, std::vector<FrameBufferHandle>> m_availablePool;

    uint32_t m_nextHandleId = 1;
};
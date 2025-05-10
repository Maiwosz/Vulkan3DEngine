#pragma once
#include "VulkanContext.h"
#include "CommandBuffer.h"
#include "Buffer.h"
#include "Image.h"
#include "StagingBufferManager.h"
#include <vector>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <memory>
#include <spdlog/spdlog.h>
#include <AssetLib.h>

// Forward declarations
class FrameManager;
class SynchronizationResourceManager;

// Structure to hold the data for a pending buffer transfer
struct BufferTransferRequest {
    Buffer* destination;
    VkDeviceSize size;
    std::vector<uint8_t> data;  // Copy of the data
    VkBuffer destinationBuffer; // Store the actual VkBuffer handle
};

// Structure to hold the data for a pending image transfer
struct ImageTransferRequest {
    Image* destination;
    VkImage destinationImage;
    VkImageCreateInfo imageInfo;
    std::vector<uint8_t> data;
    std::vector<AssetLib::MipLevel> mipLevels;
};

class TransferManager {
public:
    explicit TransferManager(
        VulkanContext& context,
        FrameManager& frameManager,
        SynchronizationResourceManager& syncResourceManager,
        StagingBufferManager& stagingManager
    );

    ~TransferManager() = default;

    // Queue a buffer transfer request to be executed at the beginning of the next frame
    void queueBufferTransfer(Buffer* destination, const void* data, VkDeviceSize size);

    // Queue an image transfer request to be executed at the beginning of the next frame
    void queueImageTransfer(
        Image* destination,
        const void* data,
        const VkImageCreateInfo& imageInfo,
        const std::vector<AssetLib::MipLevel>& mipLevels = {}
    );

    // Execute all queued transfers
    // Should be called at the beginning of each frame before any rendering
    void executeTransfers(CommandBuffer& transferCmd, CommandBuffer& graphicsCmd);

    // Check if there are any pending transfers
    bool hasPendingTransfers() const;

private:
    VulkanContext& m_context;
    FrameManager& m_frameManager;
    SynchronizationResourceManager& m_syncResourceManager;
    StagingBufferManager& m_stagingManager;

    mutable std::mutex m_mutex;
    std::vector<BufferTransferRequest> m_pendingBufferTransfers;
    std::vector<ImageTransferRequest> m_pendingImageTransfers;
};
#include "TransferManager.h"
#include "FrameManager.h"

TransferManager::TransferManager(
    VulkanContext& context,
    FrameManager& frameManager,
    SynchronizationResourceManager& syncResourceManager,
    StagingBufferManager& stagingManager
) : m_context(context),
m_frameManager(frameManager),
m_syncResourceManager(syncResourceManager),
m_stagingManager(stagingManager) {
}

void TransferManager::queueBufferTransfer(Buffer* destination, const void* data, VkDeviceSize size) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (size == 0 || !destination || !data) {
        SPDLOG_WARN("Invalid buffer transfer request: size={}, destination={}", size, (void*)destination);
        return; // Skip invalid requests
    }

    // Validate that destination buffer is valid
    VkBuffer bufferHandle = destination->get();
    if (bufferHandle == VK_NULL_HANDLE) {
        SPDLOG_WARN("Attempted to queue transfer to a null buffer destination");
        return;
    }

    BufferTransferRequest request;
    request.destination = destination;
    request.destinationBuffer = bufferHandle; // Store the actual Vulkan handle
    request.size = size;

    // Make a copy of the source data
    request.data.resize(size);
    memcpy(request.data.data(), data, size);

    SPDLOG_DEBUG("Queued buffer transfer: size={}, buffer={}", size, (void*)bufferHandle);
    m_pendingBufferTransfers.push_back(std::move(request));
}

void TransferManager::queueImageTransfer(
    Image* destination,
    const void* data,
    const VkImageCreateInfo& imageInfo,
    const std::vector<AssetLib::MipLevel>& mipLevels) {

    std::lock_guard<std::mutex> lock(m_mutex);

    if (!destination || !data || imageInfo.extent.width == 0 || imageInfo.extent.height == 0) {
        SPDLOG_WARN("Invalid image transfer request: destination={}, width={}, height={}",
            (void*)destination,
            destination ? imageInfo.extent.width : 0,
            destination ? imageInfo.extent.height : 0);
        return; // Skip invalid requests
    }

    // Validate that destination image is valid
    VkImage imageHandle = destination->get();
    if (imageHandle == VK_NULL_HANDLE) {
        SPDLOG_WARN("Attempted to queue transfer to a null image destination");
        return;
    }

    ImageTransferRequest request;
    request.destination = destination;
    request.destinationImage = imageHandle;
    request.imageInfo = imageInfo;

    // Zapisujemy dostarczone informacje o mipmapach
    if (!mipLevels.empty()) {
        request.mipLevels = mipLevels;
    }
    // Jeśli nie dostarczono informacji o mipmapach, używamy domyślnego obliczania
    else {
        // Obliczamy rozmiar każdego poziomu mipmapy
        uint32_t bytesPerPixel = 4; // Domyślnie RGBA8

        // Określ format na podstawie podanego formatu obrazu
        switch (imageInfo.format) {
        case VK_FORMAT_R8G8B8A8_UNORM:
        case VK_FORMAT_R8G8B8A8_SRGB:
            bytesPerPixel = 4;
            break;
        case VK_FORMAT_R8G8B8_UNORM:
        case VK_FORMAT_R8G8B8_SRGB:
            bytesPerPixel = 3;
            break;
        case VK_FORMAT_R8G8_UNORM:
            bytesPerPixel = 2;
            break;
        case VK_FORMAT_R8_UNORM:
            bytesPerPixel = 1;
            break;
            // BC7 wymaga specjalnej obsługi
        case VK_FORMAT_BC7_UNORM_BLOCK:
        case VK_FORMAT_BC7_SRGB_BLOCK:
            // Specjalna obsługa dla BC7
            // W tym przypadku powinniśmy mieć dostarczone mipLevels, ale w razie czego
            // utworzymy podstawową wersję
            for (uint32_t i = 0; i < imageInfo.mipLevels; i++) {
                uint32_t mipWidth = std::max(1u, imageInfo.extent.width >> i);
                uint32_t mipHeight = std::max(1u, imageInfo.extent.height >> i);

                // Zaokrąglij do wielokrotności 4 dla kompresji blokowej
                uint32_t blocksX = (mipWidth + 3) / 4;
                uint32_t blocksY = (mipHeight + 3) / 4;

                // Każdy blok zajmuje 16 bajtów w BC7
                uint32_t levelSize = blocksX * blocksY * 16;

                AssetLib::MipLevel mipLevel;
                mipLevel.width = mipWidth;
                mipLevel.height = mipHeight;
                mipLevel.dataOffset = (i > 0) ?
                    request.mipLevels.back().dataOffset + request.mipLevels.back().dataSize : 0;
                mipLevel.dataSize = levelSize;

                request.mipLevels.push_back(mipLevel);
            }
            break;
        default:
            // Dla innych formatów, domyślnie 4 bajty na piksel
            bytesPerPixel = 4;
            break;
        }

        // Jeśli nie jest to BC7, generujemy standardowe informacje o mipmapach
        if (imageInfo.format != VK_FORMAT_BC7_UNORM_BLOCK &&
            imageInfo.format != VK_FORMAT_BC7_SRGB_BLOCK) {

            for (uint32_t i = 0; i < imageInfo.mipLevels; i++) {
                uint32_t mipWidth = std::max(1u, imageInfo.extent.width >> i);
                uint32_t mipHeight = std::max(1u, imageInfo.extent.height >> i);

                uint32_t levelSize = mipWidth * mipHeight * bytesPerPixel;

                AssetLib::MipLevel mipLevel;
                mipLevel.width = mipWidth;
                mipLevel.height = mipHeight;
                mipLevel.dataOffset = (i > 0) ?
                    request.mipLevels.back().dataOffset + request.mipLevels.back().dataSize : 0;
                mipLevel.dataSize = levelSize;

                request.mipLevels.push_back(mipLevel);
            }
        }
    }

    // Oblicz całkowitą wielkość
    VkDeviceSize totalSize = 0;
    if (!request.mipLevels.empty()) {
        const auto& lastMip = request.mipLevels.back();
        totalSize = lastMip.dataOffset + lastMip.dataSize;
    }

    if (totalSize == 0) {
        SPDLOG_WARN("Calculated zero size for image transfer");
        return; // Skip if we calculated zero size
    }

    // Make a copy of the source data
    request.data.resize(totalSize);
    memcpy(request.data.data(), data, totalSize);

    SPDLOG_DEBUG("Queued image transfer: width={}, height={}, format={}, mips={}, size={}, image={}",
        imageInfo.extent.width,
        imageInfo.extent.height,
        (uint32_t)imageInfo.format,
        imageInfo.mipLevels,
        totalSize,
        (void*)imageHandle);

    m_pendingImageTransfers.push_back(std::move(request));
}

bool TransferManager::hasPendingTransfers() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return !m_pendingBufferTransfers.empty() || !m_pendingImageTransfers.empty();
}

void TransferManager::executeTransfers(CommandBuffer& transferCmd, CommandBuffer& graphicsCmd) {
    std::vector<BufferTransferRequest> bufferTransfers;
    std::vector<ImageTransferRequest> imageTransfers;

    // Move all pending transfers to local vectors to avoid locking during execution
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_pendingBufferTransfers.empty() && m_pendingImageTransfers.empty()) {
            return; // Nothing to do
        }
        bufferTransfers.swap(m_pendingBufferTransfers);
        imageTransfers.swap(m_pendingImageTransfers);
    }

    SPDLOG_DEBUG("Executing transfers: {} buffers, {} images",
        bufferTransfers.size(), imageTransfers.size());

    // Make sure the command buffers are in recording state
    if (!transferCmd.isRecording()) {
        transferCmd.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    }

    if (!graphicsCmd.isRecording()) {
        graphicsCmd.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    }

    // Process buffer transfers
    if (!bufferTransfers.empty()) {
        // Calculate total buffer size needed
        VkDeviceSize totalBufferSize = 0;
        for (const auto& request : bufferTransfers) {
            totalBufferSize += request.size;
        }

        // Request a staging buffer for all buffer transfers
        Buffer* bufferStagingBuffer = m_stagingManager.requestBuffer(
            totalBufferSize,
            m_frameManager.getInFlightFence()
        );

        if (!bufferStagingBuffer) {
            SPDLOG_ERROR("Failed to allocate staging buffer for buffer transfers");
            return;
        }

        // Ensure staging buffer is valid
        if (bufferStagingBuffer->get() == VK_NULL_HANDLE) {
            SPDLOG_ERROR("Staging buffer returned by StagingBufferManager is null");
            return;
        }

        // Copy all buffer data to the staging buffer
        void* mappedBuffer = bufferStagingBuffer->map();
        if (!mappedBuffer) {
            SPDLOG_ERROR("Failed to map staging buffer for buffer transfers");
            return;
        }

        VkDeviceSize bufferOffset = 0;
        for (const auto& request : bufferTransfers) {
            // Use the stored VkBuffer handle instead of querying the destination pointer
            VkBuffer destinationBuffer = request.destinationBuffer;
            if (destinationBuffer == VK_NULL_HANDLE) {
                SPDLOG_WARN("Skipping buffer transfer - destination buffer handle is null");
                bufferOffset += request.size;
                continue;
            }

            // Copy the data to the staging buffer
            uint8_t* dstPtr = static_cast<uint8_t*>(mappedBuffer) + bufferOffset;
            memcpy(dstPtr, request.data.data(), request.size);

            // Record the copy command
            VkBufferCopy copyRegion{};
            copyRegion.srcOffset = bufferOffset;
            copyRegion.dstOffset = 0;
            copyRegion.size = request.size;

            vkCmdCopyBuffer(
                transferCmd.handle(),
                bufferStagingBuffer->get(),
                destinationBuffer,
                1,
                &copyRegion
            );

            SPDLOG_DEBUG("Copying buffer: size={}, src_offset={}, dst={}",
                request.size, bufferOffset, (void*)destinationBuffer);
            bufferOffset += request.size;
        }

        bufferStagingBuffer->unmap();
    }

    // Process image transfers
    if (!imageTransfers.empty()) {
        // Calculate total image size needed
        VkDeviceSize totalImageSize = 0;
        for (const auto& request : imageTransfers) {
            if (!request.data.empty()) {
                totalImageSize += request.data.size();
            }
        }

        if (totalImageSize > 0) {
            // Request a staging buffer for all image transfers
            Buffer* imageStagingBuffer = m_stagingManager.requestBuffer(
                totalImageSize,
                m_frameManager.getInFlightFence()
            );

            if (!imageStagingBuffer) {
                SPDLOG_ERROR("Failed to allocate staging buffer for image transfers");
                return;
            }

            // Ensure staging buffer is valid
            if (imageStagingBuffer->get() == VK_NULL_HANDLE) {
                SPDLOG_ERROR("Staging buffer returned by StagingBufferManager is null");
                return;
            }

            // Copy all image data to the staging buffer
            void* mappedImage = imageStagingBuffer->map();
            if (!mappedImage) {
                SPDLOG_ERROR("Failed to map staging buffer for image transfers");
                return;
            }

            VkDeviceSize imageOffset = 0;
            for (const auto& request : imageTransfers) {
                // Use the stored VkImage handle instead of querying the destination pointer
                VkImage destinationImage = request.destinationImage;
                if (destinationImage == VK_NULL_HANDLE) {
                    SPDLOG_WARN("Skipping image transfer - destination image handle is null");
                    imageOffset += request.data.size();
                    continue;
                }

                // Copy the data to the staging buffer
                uint8_t* dstPtr = static_cast<uint8_t*>(mappedImage) + imageOffset;
                memcpy(dstPtr, request.data.data(), request.data.size());

                // Get the image aspect from the destination image if available, otherwise calculate it
                VkImageAspectFlags aspectMask;
                if (request.destination && request.destination->get() != VK_NULL_HANDLE) {
                    aspectMask = Image::getImageAspect(request.destination->getFormat());
                }
                else {
                    aspectMask = Image::getImageAspect(request.imageInfo.format);
                }

                // Transition the image layout from UNDEFINED to TRANSFER_DST_OPTIMAL
                VkImageMemoryBarrier preTransferBarrier{};
                preTransferBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                preTransferBarrier.srcAccessMask = 0;
                preTransferBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                preTransferBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                preTransferBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                preTransferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                preTransferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                preTransferBarrier.image = destinationImage;
                preTransferBarrier.subresourceRange.aspectMask = aspectMask;
                preTransferBarrier.subresourceRange.baseMipLevel = 0;
                preTransferBarrier.subresourceRange.levelCount = request.imageInfo.mipLevels;
                preTransferBarrier.subresourceRange.baseArrayLayer = 0;
                preTransferBarrier.subresourceRange.layerCount = request.imageInfo.arrayLayers;

                vkCmdPipelineBarrier(
                    transferCmd.handle(),
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    0,
                    0, nullptr,
                    0, nullptr,
                    1, &preTransferBarrier
                );

                // Prepare buffer-to-image copy regions for each mip level
                std::vector<VkBufferImageCopy> copyRegions;
                copyRegions.reserve(request.imageInfo.mipLevels);

                // Use the mipLevels directly from the request
                for (uint32_t i = 0; i < request.mipLevels.size(); i++) {
                    const auto& mipLevel = request.mipLevels[i];

                    VkBufferImageCopy region{};
                    region.bufferOffset = imageOffset + mipLevel.dataOffset;
                    region.bufferRowLength = 0;   // Tightly packed
                    region.bufferImageHeight = 0; // Tightly packed
                    region.imageSubresource.aspectMask = aspectMask;
                    region.imageSubresource.mipLevel = i;
                    region.imageSubresource.baseArrayLayer = 0;
                    region.imageSubresource.layerCount = request.imageInfo.arrayLayers;
                    region.imageOffset = { 0, 0, 0 };
                    region.imageExtent = {
                        mipLevel.width,
                        mipLevel.height,
                        1
                    };

                    copyRegions.push_back(region);
                }

                // Record the copy command for this image
                vkCmdCopyBufferToImage(
                    transferCmd.handle(),
                    imageStagingBuffer->get(),
                    destinationImage,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    static_cast<uint32_t>(copyRegions.size()),
                    copyRegions.data()
                );

                SPDLOG_DEBUG("Copying image: width={}, height={}, mips={}, format={}, dst={}",
                    request.imageInfo.extent.width,
                    request.imageInfo.extent.height,
                    request.imageInfo.mipLevels,
                    (uint32_t)request.imageInfo.format,
                    (void*)destinationImage);

                // Transition the image layout from TRANSFER_DST_OPTIMAL to SHADER_READ_ONLY_OPTIMAL
                // This is done on the graphics command buffer to ensure proper execution order
                VkImageMemoryBarrier postTransferBarrier{};
                postTransferBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                postTransferBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                postTransferBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                postTransferBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                postTransferBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                postTransferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                postTransferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                postTransferBarrier.image = destinationImage;
                postTransferBarrier.subresourceRange.aspectMask = aspectMask;
                postTransferBarrier.subresourceRange.baseMipLevel = 0;
                postTransferBarrier.subresourceRange.levelCount = request.imageInfo.mipLevels;
                postTransferBarrier.subresourceRange.baseArrayLayer = 0;
                postTransferBarrier.subresourceRange.layerCount = request.imageInfo.arrayLayers;

                vkCmdPipelineBarrier(
                    graphicsCmd.handle(),
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    0,
                    0, nullptr,
                    0, nullptr,
                    1, &postTransferBarrier
                );

                // Update the image's layout tracking if the destination object is still valid
                if (request.destination && request.destination->get() == destinationImage) {
                    request.destination->setLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                }

                imageOffset += request.data.size();
            }

            imageStagingBuffer->unmap();
        }
    }
}
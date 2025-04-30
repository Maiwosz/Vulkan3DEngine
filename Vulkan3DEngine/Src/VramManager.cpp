#include "VramManager.h"
#include <stdexcept>
#include "GraphicsTypes.h"

VramManager::VramManager(
    VulkanContext& context,
    FrameManager& frameManager,
    CommandBufferManager& cmdBufferManager,
    SynchronizationResourceManager& syncResourceManager
)
    : m_context(context),
    m_frameManager(frameManager),
    m_cmdBufferManager(cmdBufferManager),
	m_syncResourceManager(syncResourceManager),
    m_allocator(createVmaAllocator(context)),
    m_stagingManager(m_allocator, context.logical())
{

}

VramManager::~VramManager() {
    m_resources.clear();

    if (m_allocator) {
        vmaDestroyAllocator(m_allocator);
    }
}

VmaAllocator VramManager::createVmaAllocator(VulkanContext& context) {
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = context.physical().get();
    allocatorInfo.device = context.logical().get();
    allocatorInfo.instance = context.instance().get();
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_2;

    VmaAllocator allocator;
    if (vmaCreateAllocator(&allocatorInfo, &allocator) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create VMA allocator");
    }
    return allocator;
}

VramHandle VramManager::createBuffer(const Graphics::BufferCreateInfo& info, const void* initialData) {
    const auto vkUsage = convertBufferUsage(info.usage);
    Buffer buffer = Buffer::create(m_allocator, info.size, vkUsage, VMA_MEMORY_USAGE_AUTO);

    if (initialData) {
        auto& frame = m_frameManager.getCurrentFrame();
        recordBufferUpload(*frame.transferCommandBuffer.get(), buffer, initialData, info.size);
    }

    VramHandle handle{ m_nextId++ };
    m_resources.emplace(handle.id, std::move(buffer));
    return handle;
}

VramHandle VramManager::createImage(const Graphics::ImageCreateInfo& info, const void* initialData) {
    VkImageCreateInfo vkImageInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    vkImageInfo.imageType = VK_IMAGE_TYPE_2D;
    vkImageInfo.format = convertImageFormat(info.format);
    vkImageInfo.extent = { info.width, info.height, 1 };
    vkImageInfo.mipLevels = info.mipLevels;
    vkImageInfo.usage = convertImageUsage(info.usage);
    vkImageInfo.samples = Graphics::convertSampleCount(info.samples);

    Image image = Image::create(m_allocator, vkImageInfo, VMA_MEMORY_USAGE_AUTO);

    if (initialData) {
        auto& frame = m_frameManager.getCurrentFrame();
        recordImageUpload(*frame.transferCommandBuffer.get(), image, initialData, vkImageInfo);
    }

    VramHandle handle{ m_nextId++ };
    m_resources.emplace(handle.id, std::move(image));
    return handle;
}

VramHandle VramManager::createBuffer(
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags memoryProperties,
    const void* initialData)
{
    // Tworzenie głównego bufora
    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.requiredFlags = memoryProperties;

    Buffer buffer = Buffer::create(m_allocator, size, usage, allocInfo.usage);

    if (initialData) {
        // 1. Pobierz zasoby synchronizacji
        VkFence fence = m_syncResourceManager.acquireFence(false);
        auto staging = m_stagingManager.requestBuffer(size, fence);

        // 2. Skopiuj dane do staging buffer
        staging.copyData(initialData, size);

        // 3. Pobierz bufor poleceń
        CommandBufferManager::Configuration cmdConfig{
            .queueType = LogicalDevice::QueueType::Transfer,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .usageFlags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
        };
        auto cmdBuffer = m_cmdBufferManager.acquireBuffer(cmdConfig);

        // 4. Nagrywanie komend
        cmdBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        VkBufferCopy copyRegion{ 0, 0, size };
        vkCmdCopyBuffer(
            cmdBuffer->get(),
            staging.get(),
            buffer.get(),
            1,
            &copyRegion
        );

        cmdBuffer->end();

        // 5. Submisja
        VkCommandBuffer commandBufferHandle = cmdBuffer->get();
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBufferHandle;

        vkQueueSubmit(m_context.logical().getQueue(LogicalDevice::QueueType::Transfer), 1, &submitInfo, fence);
        vkWaitForFences(m_context.logical().get(), 1, &fence, VK_TRUE, UINT64_MAX);

        // 6. Zwrot zasobów
        m_cmdBufferManager.releaseBuffer(std::move(cmdBuffer));
        m_syncResourceManager.releaseFence(fence);
        m_stagingManager.returnBuffer(std::move(staging));
    }

    VramHandle handle{ m_nextId++ };
    m_resources.emplace(handle.id, std::move(buffer));
    return handle;
}

VramHandle VramManager::createImage(
    const VkImageCreateInfo& imageInfo,
    VkMemoryPropertyFlags memoryProperties,
    const void* initialData)
{
    // Tworzenie głównego obrazu
    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.requiredFlags = memoryProperties;

    Image image = Image::create(m_allocator, imageInfo, allocInfo.usage);
    const VkImageAspectFlags aspectMask = Image::getImageAspect(imageInfo.format);
    const VkDeviceSize imageSize = Graphics::calculateImageSize(
        imageInfo.format,
        imageInfo.extent.width,
        imageInfo.extent.height
    );

    if (initialData) {
        // 1. Pobierz zasoby synchronizacji
        VkFence fence = m_syncResourceManager.acquireFence(false);
        auto staging = m_stagingManager.requestBuffer(imageSize, fence);
        staging.copyData(initialData, imageSize);

        // 2. Pobierz bufor poleceń
        CommandBufferManager::Configuration cmdConfig{
            .queueType = LogicalDevice::QueueType::Transfer,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .usageFlags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
        };
        auto cmdBuffer = m_cmdBufferManager.acquireBuffer(cmdConfig);

        // 3. Nagrywanie komend
        cmdBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        // Przejście layoutu
        image.recordLayoutTransition(
            *cmdBuffer,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            aspectMask
        );

        // Kopiowanie danych
        VkBufferImageCopy region{};
        region.imageSubresource = { aspectMask, 0, 0, 1 };
        region.imageExtent = imageInfo.extent;

        vkCmdCopyBufferToImage(
            cmdBuffer->get(),
            staging.get(),
            image.get(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &region
        );

        // Finalny layout
        image.recordLayoutTransition(
            *cmdBuffer,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            aspectMask
        );

        cmdBuffer->end();

        // 4. Submisja
        VkCommandBuffer commandBufferHandle = cmdBuffer->get();
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBufferHandle;

        vkQueueSubmit(m_context.logical().getQueue(LogicalDevice::QueueType::Transfer), 1, &submitInfo, fence);
        vkWaitForFences(m_context.logical().get(), 1, &fence, VK_TRUE, UINT64_MAX);

        // 5. Zwrot zasobów
        m_cmdBufferManager.releaseBuffer(std::move(cmdBuffer));
        m_syncResourceManager.releaseFence(fence);
        m_stagingManager.returnBuffer(std::move(staging));
    }

    VramHandle handle{ m_nextId++ };
    m_resources.emplace(handle.id, std::move(image));
    return handle;
}

void VramManager::freeResource(VramHandle handle) {
    if (!handle.isValid()) {
        return; // Ignoruj nieprawidłowe uchwyty
    }

    auto it = m_resources.find(handle.id);
    if (it == m_resources.end()) {
        return; // Zasób już został zwolniony
    }

    // Obsłuż specjalny przypadek obrazów zewnętrznych
    if (std::holds_alternative<Image>(it->second)) {
        Image& img = std::get<Image>(it->second);

        if (img.isExternalResource()) {
            // 1. Wykonaj barierę końcową jeśli potrzebna
            // 2. Usuń tylko referencję, nie niszcz VkImage
            // 3. Wyczyść stan w managera
            m_resources.erase(it);
            return;
        }
    }

    // Standardowe zwalnianie zasobów
    if (std::holds_alternative<Image>(it->second)) {
        Image& img = std::get<Image>(it->second);
        // Ewentualne dodatkowe przygotowanie obrazu
    }
    else if (std::holds_alternative<Buffer>(it->second)) {
        Buffer& buf = std::get<Buffer>(it->second);
        // Ewentualne dodatkowe przygotowanie bufora
    }

    m_resources.erase(it); // Wyzwala destruktor zasobu
}

uint64_t VramManager::getResourceSize(VramHandle handle) {
    if (!handle.isValid()) {
        return 0;
    }

    auto it = m_resources.find(handle.id);
    if (it == m_resources.end()) {
        return 0;
    }

    if (const Buffer* buffer = std::get_if<Buffer>(&it->second)) {
        return static_cast<uint64_t>(buffer->getAllocatedSize());
    }
    else if (const Image* image = std::get_if<Image>(&it->second)) {
        return static_cast<uint64_t>(image->getAllocatedSize());
    }

    return 0;
}

uint64_t VramManager::getVramUsed() const
{
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(m_context.physical().get(), &memProps);

    VmaBudget budgets[VK_MAX_MEMORY_HEAPS];
    vmaGetHeapBudgets(m_allocator, budgets);

    VkDeviceSize usedBytes = 0;
    for (uint32_t i = 0; i < memProps.memoryHeapCount; ++i) {
        if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            usedBytes += budgets[i].usage;
        }
    }
    return static_cast<uint64_t>(usedBytes);
}

uint64_t VramManager::getVramBudget() const
{
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(m_context.physical().get(), &memProps);

    VmaBudget budgets[VK_MAX_MEMORY_HEAPS];
    vmaGetHeapBudgets(m_allocator, budgets);

    VkDeviceSize budgetBytes = 0;
    for (uint32_t i = 0; i < memProps.memoryHeapCount; ++i) {
        if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            budgetBytes += budgets[i].budget;
        }
    }
    return static_cast<uint64_t>(budgetBytes);
}

float VramManager::getVramUsagePercentage() const
{
    const VkDeviceSize used = getVramUsed();
    const VkDeviceSize budget = getVramBudget();
    return (budget > 0) ? static_cast<float>(used) / static_cast<float>(budget) * 100.0f : 0.0f;
}

VramHandle VramManager::registerExternalImage(
    VkImage image,
    VkFormat format,
    VkExtent2D extent,
    VkImageLayout initialLayout,
    VkSampleCountFlagBits samples)
{
    // Generate a new handle ID
    VramHandle handle{ m_nextId++ };

    // Create the image directly in the map (avoiding temporary)
    auto [iter, success] = m_resources.try_emplace(
        handle.id,
        std::in_place_type<Image>
    );

    // If insertion succeeded, now initialize the Image
    if (success) {
        Image* imgPtr = std::get_if<Image>(&iter->second);
        if (imgPtr) {
            // Initialize the image in-place
            *imgPtr = Image::createExternal(
                m_allocator,
                image,
                format,
                extent,
                initialLayout,
                samples
            );
        }
    }

    return handle;
}

void VramManager::recordBufferUpload(CommandBuffer& cmdBuffer, Buffer& dst, const void* data, VkDeviceSize size) {
    Buffer staging = m_stagingManager.requestBuffer(size, m_frameManager.getCurrentFrame().inFlightFence); 
    staging.copyData(data, size);

    VkBufferCopy copyRegion{ 0, 0, size };
    vkCmdCopyBuffer(cmdBuffer.get(), staging.get(), dst.get(), 1, &copyRegion);
}


void VramManager::recordImageUpload(CommandBuffer& commandBuffer, Image& dst, const void* data, const VkImageCreateInfo& imageInfo) {
    const VkImageAspectFlags aspectMask = Image::getImageAspect(imageInfo.format);
    const VkDeviceSize imageSize = Graphics::calculateImageSize(imageInfo.format, imageInfo.extent.width, imageInfo.extent.height);

    // 1. Staging buffer
    Buffer staging = m_stagingManager.requestBuffer(imageSize, m_frameManager.getCurrentFrame().inFlightFence);
    staging.copyData(data, imageSize);

    // 2. Przejście: UNDEFINED → TRANSFER_DST_OPTIMAL
    dst.recordLayoutTransition(
        commandBuffer,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        aspectMask
    );

    // 3. Kopiuj dane z bufora do obrazu
    VkBufferImageCopy region{};
    region.imageSubresource = { aspectMask, 0, 0, 1 };
    region.imageExtent = imageInfo.extent;

    vkCmdCopyBufferToImage(
        commandBuffer.get(),
        staging.get(),
        dst.get(),
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &region
    );

    // 4. Przejście: TRANSFER_DST_OPTIMAL → SHADER_READ_ONLY_OPTIMAL
    dst.recordLayoutTransition(
        commandBuffer,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        aspectMask
    );

    // 5. Przechowaj staging buffer
    m_frameManager.getCurrentFrame().stagingBuffers.push_back(std::move(staging));
}

#include "VramManager.h"
#include <stdexcept>
#include "GraphicsTypes.h"
#include <cassert>
#include <spdlog/spdlog.h>

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
    m_stagingManager(m_allocator, context.logical()),
    m_transferManager(context, frameManager, syncResourceManager, m_stagingManager)
{
    // Verify allocator was created successfully
    assert(m_allocator != VK_NULL_HANDLE && "VMA allocator creation failed");

    // Add logging or debug check here
    if (m_allocator == VK_NULL_HANDLE) {
        SPDLOG_ERROR("VMA allocator initialization failed");
        throw std::runtime_error("VMA allocator initialization failed");
    }

    // Log total VRAM available
    uint64_t vramBudget = getVramBudget();
    SPDLOG_INFO("VramManager initialized with {:.2f} MB total VRAM", vramBudget / (1024.0f * 1024.0f));
}

VramManager::~VramManager() {
    uint64_t vramUsed = getVramUsed();
    if (vramUsed > 0) {
        SPDLOG_WARN("VramManager destroyed with {:.2f} MB VRAM still in use ({} resources remaining)",
            vramUsed / (1024.0f * 1024.0f), m_resources.size());
    }

    m_resources.clear();

    if (m_allocator) {
        vmaDestroyAllocator(m_allocator);
        m_allocator = VK_NULL_HANDLE;
    }

    SPDLOG_DEBUG("VramManager destroyed");
}

VmaAllocator VramManager::createVmaAllocator(VulkanContext& context) {
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = context.physical().get();
    allocatorInfo.device = context.logical().get();
    allocatorInfo.instance = context.instance().get();
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_2;

    // Add additional validation
    if (allocatorInfo.physicalDevice == VK_NULL_HANDLE) {
        SPDLOG_ERROR("Physical device is null in VMA allocator creation");
        throw std::runtime_error("Physical device is null in VMA allocator creation");
    }

    if (allocatorInfo.device == VK_NULL_HANDLE) {
        SPDLOG_ERROR("Logical device is null in VMA allocator creation");
        throw std::runtime_error("Logical device is null in VMA allocator creation");
    }

    if (allocatorInfo.instance == VK_NULL_HANDLE) {
        SPDLOG_ERROR("Vulkan instance is null in VMA allocator creation");
        throw std::runtime_error("Vulkan instance is null in VMA allocator creation");
    }

    VmaAllocator allocator = VK_NULL_HANDLE;
    VkResult result = vmaCreateAllocator(&allocatorInfo, &allocator);

    if (result != VK_SUCCESS || allocator == VK_NULL_HANDLE) {
        SPDLOG_ERROR("Failed to create VMA allocator");
        throw std::runtime_error("Failed to create VMA allocator");
    }

    SPDLOG_DEBUG("VMA allocator created successfully");
    return allocator;
}

VramHandle VramManager::createBuffer(const Graphics::BufferCreateInfo& info, const void* initialData) {
    // Validate inputs
    if (info.size == 0) {
        SPDLOG_ERROR("Attempted to create buffer with zero size");
        throw std::runtime_error("Buffer size cannot be zero");
    }

    if (m_allocator == VK_NULL_HANDLE) {
        SPDLOG_ERROR("Cannot create buffer with null VMA allocator");
        throw std::runtime_error("Cannot create buffer with null VMA allocator");
    }

    const auto vkUsage = convertBufferUsage(info.usage);
    Buffer buffer = Buffer::create(m_allocator, info.size, vkUsage, VMA_MEMORY_USAGE_AUTO);

    if (initialData) {
        m_transferManager.queueBufferTransfer(&buffer, initialData, info.size);
    }

    VramHandle handle{ m_nextId++ };
    m_resources.emplace(handle.id, std::move(buffer));

    // Log resource creation and VRAM usage
    uint64_t resourceSize = getResourceSize(handle);
    float usagePercent = getVramUsagePercentage();
    SPDLOG_INFO("Buffer created: handle={}, size={:.2f} KB, VRAM usage={:.1f}%",
        handle.id, resourceSize / 1024.0f, usagePercent);

    return handle;
}

VramHandle VramManager::createImage(
    const Graphics::ImageCreateInfo& info,
    const void* initialData,
    const std::vector<AssetLib::MipLevel>& mipLevels) {

    // Validate inputs
    if (info.width == 0 || info.height == 0) {
        SPDLOG_ERROR("Attempted to create image with invalid dimensions: {}x{}", info.width, info.height);
        throw std::runtime_error("Image dimensions cannot be zero");
    }

    if (m_allocator == VK_NULL_HANDLE) {
        SPDLOG_ERROR("Cannot create image with null VMA allocator");
        throw std::runtime_error("Cannot create image with null VMA allocator");
    }

    VkImageCreateInfo vkImageInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    vkImageInfo.imageType = VK_IMAGE_TYPE_2D;
    vkImageInfo.format = convertImageFormat(info.format);
    vkImageInfo.extent = { info.width, info.height, 1 };
    vkImageInfo.mipLevels = info.mipLevels;
    vkImageInfo.arrayLayers = 1;
    vkImageInfo.usage = convertImageUsage(info.usage);
    vkImageInfo.samples = Graphics::convertSampleCount(info.samples);
    vkImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    vkImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    vkImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    // Sprawdź wymiary obrazu dla formatów skompresowanych
    if (info.format == Graphics::ImageFormat::BC7_UNORM ||
        info.format == Graphics::ImageFormat::BC7_SRGB) {
        if (info.width % 4 != 0 || info.height % 4 != 0) {
            SPDLOG_ERROR("Compressed texture dimensions must be multiple of 4: {}x{}", info.width, info.height);
            throw std::runtime_error("Compressed texture dimensions must be multiple of 4: " +
                std::to_string(info.width) + "x" + std::to_string(info.height));
        }
    }

    try {
        Image image = Image::create(m_allocator, vkImageInfo, VMA_MEMORY_USAGE_AUTO);

        if (initialData) {
            m_transferManager.queueImageTransfer(&image, initialData, vkImageInfo, mipLevels);
        }

        VramHandle handle{ m_nextId++ };
        m_resources.emplace(handle.id, std::move(image));

        // Log resource creation and VRAM usage
        uint64_t resourceSize = getResourceSize(handle);
        float usagePercent = getVramUsagePercentage();
        SPDLOG_INFO("Image created: handle={}, size={:.2f} KB, dimensions={}x{}, mips={}, format={}, VRAM usage={:.1f}%",
            handle.id, resourceSize / 1024.0f, info.width, info.height, info.mipLevels,
            static_cast<int>(info.format), usagePercent);

        return handle;
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to create image: {}", e.what());
        throw std::runtime_error(std::string("Failed to create image: ") + e.what());
    }
}

VramHandle VramManager::createBuffer(
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags memoryProperties,
    const void* initialData)
{
    if (size == 0) {
        SPDLOG_ERROR("Attempted to create buffer with zero size");
        throw std::runtime_error("Buffer size cannot be zero");
    }

    // Tworzenie głównego bufora
    VmaAllocationCreateInfo allocInfo = {};

    // For host-visible memory, use CPU_ONLY or CPU_TO_GPU
    if (memoryProperties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    }
    else {
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    }

    // Still set required flags to ensure we get exactly what we need
    allocInfo.requiredFlags = memoryProperties;

    // Pass the required memory properties to Buffer::create
    Buffer buffer = Buffer::create(m_allocator, size, usage, allocInfo.usage, allocInfo.requiredFlags);

    if (initialData) {
        if (!(memoryProperties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
            // 1. Pobierz zasoby synchronizacji
            VkFence fence = m_syncResourceManager.acquireFence(false);

            // 2. Pobierz wskaźnik do bufora staging - zmiana zgodnie z nowym API
            Buffer* staging = m_stagingManager.requestBuffer(size, fence);

            // 3. Skopiuj dane do staging buffer
            staging->copyData(initialData, size);

            // 4. Pobierz bufor poleceń
            CommandBufferManager::Configuration cmdConfig{
                .queueType = LogicalDevice::QueueType::Transfer,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .usageFlags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
            };
            auto cmdBuffer = m_cmdBufferManager.acquireBuffer(cmdConfig);

            // 5. Nagrywanie komend
            cmdBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

            VkBufferCopy copyRegion{ 0, 0, size };
            vkCmdCopyBuffer(
                cmdBuffer->get(),
                staging->get(),  // Używamy operatora -> zamiast . ponieważ mamy wskaźnik
                buffer.get(),
                1,
                &copyRegion
            );

            cmdBuffer->end();

            // 6. Submisja
            VkCommandBuffer commandBufferHandle = cmdBuffer->get();
            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBufferHandle;

            vkQueueSubmit(m_context.logical().getQueue(LogicalDevice::QueueType::Transfer), 1, &submitInfo, fence);

            // 7. Czekanie na zakończenie operacji
            vkWaitForFences(m_context.logical().get(), 1, &fence, VK_TRUE, UINT64_MAX);

            // 8. Zwrot zasobów
            m_cmdBufferManager.releaseBuffer(std::move(cmdBuffer));
            m_syncResourceManager.releaseFence(fence);

            // 9. Zwolnij bufory dla zakończonych płotków
            m_stagingManager.reclaimBuffers();

            SPDLOG_DEBUG("Immediate buffer transfer completed for buffer of size {:.2f} KB", size / 1024.0f);
        }
        else {
            // For host-visible memory, we can directly map and copy
            void* mappedData = buffer.map();
            if (mappedData) {
                memcpy(mappedData, initialData, size);
                buffer.unmap();
                SPDLOG_DEBUG("Direct mapped memory transfer completed for buffer of size {:.2f} KB", size / 1024.0f);
            }
        }
    }

    VramHandle handle{ m_nextId++ };
    m_resources.emplace(handle.id, std::move(buffer));

    // Log resource creation and VRAM usage
    uint64_t resourceSize = getResourceSize(handle);
    float usagePercent = getVramUsagePercentage();
    SPDLOG_INFO("Buffer created (immediate): handle={}, size={:.2f} KB, usage=0x{:x}, memory=0x{:x}, VRAM usage={:.1f}%",
        handle.id, resourceSize / 1024.0f, usage, memoryProperties, usagePercent);

    return handle;
}

VramHandle VramManager::createImage(
    const VkImageCreateInfo& imageInfo,
    VkMemoryPropertyFlags memoryProperties,
    const void* initialData)
{
    if (imageInfo.extent.width == 0 || imageInfo.extent.height == 0) {
        SPDLOG_ERROR("Attempted to create image with invalid dimensions: {}x{}",
            imageInfo.extent.width, imageInfo.extent.height);
        throw std::runtime_error("Image dimensions cannot be zero");
    }

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

        // 2. Pobierz wskaźnik do bufora staging - zmiana zgodnie z nowym API
        Buffer* staging = m_stagingManager.requestBuffer(imageSize, fence);

        // 3. Skopiuj dane do bufora staging
        staging->copyData(initialData, imageSize);

        // 4. Pobierz bufor poleceń
        CommandBufferManager::Configuration cmdConfig{
            .queueType = LogicalDevice::QueueType::Transfer,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .usageFlags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
        };
        auto cmdBuffer = m_cmdBufferManager.acquireBuffer(cmdConfig);

        // 5. Nagrywanie komend
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
            staging->get(),
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

        // 6. Submisja
        VkCommandBuffer commandBufferHandle = cmdBuffer->get();
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBufferHandle;

        vkQueueSubmit(m_context.logical().getQueue(LogicalDevice::QueueType::Transfer), 1, &submitInfo, fence);

        // 7. Czekanie na zakończenie operacji
        vkWaitForFences(m_context.logical().get(), 1, &fence, VK_TRUE, UINT64_MAX);

        // 8. Zwrot zasobów
        m_cmdBufferManager.releaseBuffer(std::move(cmdBuffer));
        m_syncResourceManager.releaseFence(fence);

        // 9. Zwolnij bufory dla zakończonych płotków
        m_stagingManager.reclaimBuffers();

        SPDLOG_DEBUG("Immediate image transfer completed for image of size {:.2f} KB", imageSize / 1024.0f);
    }

    VramHandle handle{ m_nextId++ };
    m_resources.emplace(handle.id, std::move(image));

    // Log resource creation and VRAM usage
    uint64_t resourceSize = getResourceSize(handle);
    float usagePercent = getVramUsagePercentage();
    SPDLOG_INFO("Image created (immediate): handle={}, size={:.2f} KB, dimensions={}x{}, mips={}, format={}, VRAM usage={:.1f}%",
        handle.id, resourceSize / 1024.0f, imageInfo.extent.width, imageInfo.extent.height,
        imageInfo.mipLevels, static_cast<int>(imageInfo.format), usagePercent);

    return handle;
}

void VramManager::freeResource(VramHandle handle) {
    if (!handle.isValid()) {
        SPDLOG_WARN("Attempted to free invalid resource handle");
        return; // Ignoruj nieprawidłowe uchwyty
    }

    auto it = m_resources.find(handle.id);
    if (it == m_resources.end()) {
        SPDLOG_WARN("Attempted to free non-existent resource: handle={}", handle.id);
        return; // Zasób już został zwolniony
    }

    uint64_t resourceSize = getResourceSize(handle);

    // Obsłuż specjalny przypadek obrazów zewnętrznych
    if (std::holds_alternative<Image>(it->second)) {
        Image& img = std::get<Image>(it->second);

        if (img.isExternalResource()) {
            SPDLOG_DEBUG("Freeing external image resource: handle={}", handle.id);
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
        SPDLOG_DEBUG("Freeing image resource: handle={}, size={:.2f} KB",
            handle.id, resourceSize / 1024.0f);
    }
    else if (std::holds_alternative<Buffer>(it->second)) {
        Buffer& buf = std::get<Buffer>(it->second);
        // Ewentualne dodatkowe przygotowanie bufora
        SPDLOG_DEBUG("Freeing buffer resource: handle={}, size={:.2f} KB",
            handle.id, resourceSize / 1024.0f);
    }

    m_resources.erase(it); // Wyzwala destruktor zasobu

    // Log VRAM status after freeing
    float usagePercent = getVramUsagePercentage();
    SPDLOG_INFO("Resource freed: handle={}, size={:.2f} KB, VRAM usage={:.1f}%",
        handle.id, resourceSize / 1024.0f, usagePercent);
}

VramHandle VramManager::registerExternalImage(
    VkImage image,
    VkFormat format,
    VkExtent2D extent,
    VkImageLayout initialLayout,
    VkSampleCountFlagBits samples)
{
    if (image == VK_NULL_HANDLE) {
        SPDLOG_ERROR("Attempted to register null external image");
        throw std::runtime_error("Cannot register null external image");
    }

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

            SPDLOG_INFO("External image registered: handle={}, dimensions={}x{}, format={}",
                handle.id, extent.width, extent.height, static_cast<int>(format));
        }
    }

    return handle;
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
    VmaBudget budgets[VK_MAX_MEMORY_HEAPS];
    vmaGetHeapBudgets(m_allocator, budgets);

    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(m_context.physical().get(), &memProps);

    uint64_t usedBytes = 0;
    for (uint32_t i = 0; i < memProps.memoryHeapCount; ++i) {
        // Only count device-local heaps (GPU VRAM)
        if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            usedBytes += budgets[i].usage;
        }
    }
    return usedBytes;
}

uint64_t VramManager::getVramBudget() const
{
    VmaBudget budgets[VK_MAX_MEMORY_HEAPS];
    vmaGetHeapBudgets(m_allocator, budgets);

    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(m_context.physical().get(), &memProps);

    uint64_t budgetBytes = 0;
    for (uint32_t i = 0; i < memProps.memoryHeapCount; ++i) {
        // Only count device-local heaps (GPU VRAM)
        if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            // Use the budget reported by VMA, which should be more accurate
            budgetBytes += budgets[i].budget;
        }
    }
    return budgetBytes;
}

float VramManager::getVramUsagePercentage() const
{
    const uint64_t used = getVramUsed();
    const uint64_t budget = getVramBudget();

    // Protect against division by zero and ensure values make sense
    if (budget == 0) return 0.0f;

    // Clamp the result to a maximum of 100% to avoid unrealistic values
    // This ensures we don't report using more VRAM than exists
    float percentage = static_cast<float>(used) / static_cast<float>(budget) * 100.0f;
    return std::min(percentage, 100.0f);
}
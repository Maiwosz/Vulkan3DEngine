#include "AsyncBufferSync.h"
#include <spdlog/spdlog.h>
#include <algorithm>

AsyncBufferSync::AsyncBufferSync(
    std::shared_ptr<ShaderLib::BufferObjectInstance> buffer,
    ThreadPool& threadPool,
    const Config& config
)
    : m_buffer(buffer)
    , m_threadPool(threadPool)
    , m_config(config)
    , m_nextTaskId(1)
{
}

AsyncBufferSync::~AsyncBufferSync() {
    WaitForAllTasks();
}

// =============================================================================
// SYNCHRONOUS OPERATIONS
// =============================================================================

void AsyncBufferSync::SyncToGPU() {
    if (!m_buffer || !m_buffer->HasMappedBuffer()) {
        return;
    }
    m_buffer->SyncToBuffer();
}

void AsyncBufferSync::SyncFromGPU() {
    if (!m_buffer || !m_buffer->HasMappedBuffer()) {
        return;
    }
    m_buffer->SyncFromBuffer();
}

void AsyncBufferSync::SyncFieldToGPU(const std::string& path) {
    if (!m_buffer) {
        return;
    }
    m_buffer->SyncFieldToBuffer(path);
}

void AsyncBufferSync::SyncFieldFromGPU(const std::string& path) {
    if (!m_buffer) {
        return;
    }
    m_buffer->SyncFieldFromBuffer(path);
}

void AsyncBufferSync::SyncFieldsToGPU(const std::vector<std::string>& paths) {
    if (!m_buffer) {
        return;
    }
    for (const auto& path : paths) {
        m_buffer->SyncFieldToBuffer(path);
    }
}

void AsyncBufferSync::SyncFieldsFromGPU(const std::vector<std::string>& paths) {
    if (!m_buffer) {
        return;
    }
    for (const auto& path : paths) {
        m_buffer->SyncFieldFromBuffer(path);
    }
}

// =============================================================================
// ASYNCHRONOUS OPERATIONS
// =============================================================================

BufferSyncTaskHandle AsyncBufferSync::SyncToGPUAsync() {
    if (!m_buffer || !m_buffer->HasMappedBuffer()) {
        return BufferSyncTaskHandle();
    }

    const size_t bufferSize = m_buffer->GetBufferSize();
    if (bufferSize == 0) {
        return BufferSyncTaskHandle();
    }

    auto chunks = DivideBufferIntoChunks();

    BufferSyncTaskHandle taskHandle = CreateTask("SyncToGPU");
    if (!taskHandle.isValid()) {
        return BufferSyncTaskHandle();
    }

    SyncTask& task = m_activeTasks[taskHandle];

    if (chunks.size() <= 1) {
        // Single-threaded
        task.futures.push_back(m_threadPool.enqueue([buf = m_buffer]() {
            buf->SyncToBuffer();
            }));
    }
    else {
        // Multi-threaded
        for (const auto& chunk : chunks) {
            task.futures.push_back(m_threadPool.enqueue([buf = m_buffer, chunk]() {
                buf->SyncRangeToBuffer(chunk.offset, chunk.size);
                }));
        }
    }

    SPDLOG_TRACE("AsyncBufferSync: Started async SyncToGPU with {} tasks", task.futures.size());
    return taskHandle;
}

BufferSyncTaskHandle AsyncBufferSync::SyncFromGPUAsync() {
    if (!m_buffer || !m_buffer->HasMappedBuffer()) {
        return BufferSyncTaskHandle();
    }

    const size_t bufferSize = m_buffer->GetBufferSize();
    if (bufferSize == 0) {
        return BufferSyncTaskHandle();
    }

    auto chunks = DivideBufferIntoChunks();

    BufferSyncTaskHandle taskHandle = CreateTask("SyncFromGPU");
    if (!taskHandle.isValid()) {
        return BufferSyncTaskHandle();
    }

    SyncTask& task = m_activeTasks[taskHandle];

    if (chunks.size() <= 1) {
        task.futures.push_back(m_threadPool.enqueue([buf = m_buffer]() {
            buf->SyncFromBuffer();
            }));
    }
    else {
        for (const auto& chunk : chunks) {
            task.futures.push_back(m_threadPool.enqueue([buf = m_buffer, chunk]() {
                buf->SyncRangeFromBuffer(chunk.offset, chunk.size);
                }));
        }
    }

    SPDLOG_TRACE("AsyncBufferSync: Started async SyncFromGPU with {} tasks", task.futures.size());
    return taskHandle;
}

BufferSyncTaskHandle AsyncBufferSync::SyncFieldsToGPUAsync(const std::vector<std::string>& paths) {
    if (!m_buffer || paths.empty()) {
        return BufferSyncTaskHandle();
    }

    BufferSyncTaskHandle taskHandle = CreateTask("SyncFieldsToGPU (" + std::to_string(paths.size()) + " fields)");
    if (!taskHandle.isValid()) {
        return BufferSyncTaskHandle();
    }

    SyncTask& task = m_activeTasks[taskHandle];
    auto batches = DivideFieldsIntoBatches(paths);

    if (batches.size() <= 1) {
        // Single-threaded
        task.futures.push_back(m_threadPool.enqueue([buf = m_buffer, paths]() {
            for (const auto& path : paths) {
                buf->SyncFieldToBuffer(path);
            }
            }));
    }
    else {
        // Multi-threaded
        for (const auto& batch : batches) {
            task.futures.push_back(m_threadPool.enqueue([buf = m_buffer, batch]() {
                for (const auto& path : batch) {
                    buf->SyncFieldToBuffer(path);
                }
                }));
        }
    }

    SPDLOG_TRACE("AsyncBufferSync: Started async field sync to GPU ({} fields, {} tasks)",
        paths.size(), task.futures.size());
    return taskHandle;
}

BufferSyncTaskHandle AsyncBufferSync::SyncFieldsFromGPUAsync(const std::vector<std::string>& paths) {
    if (!m_buffer || paths.empty()) {
        return BufferSyncTaskHandle();
    }

    BufferSyncTaskHandle taskHandle = CreateTask("SyncFieldsFromGPU (" + std::to_string(paths.size()) + " fields)");
    if (!taskHandle.isValid()) {
        return BufferSyncTaskHandle();
    }

    SyncTask& task = m_activeTasks[taskHandle];
    auto batches = DivideFieldsIntoBatches(paths);

    if (batches.size() <= 1) {
        task.futures.push_back(m_threadPool.enqueue([buf = m_buffer, paths]() {
            for (const auto& path : paths) {
                buf->SyncFieldFromBuffer(path);
            }
            }));
    }
    else {
        for (const auto& batch : batches) {
            task.futures.push_back(m_threadPool.enqueue([buf = m_buffer, batch]() {
                for (const auto& path : batch) {
                    buf->SyncFieldFromBuffer(path);
                }
                }));
        }
    }

    SPDLOG_TRACE("AsyncBufferSync: Started async field sync from GPU ({} fields, {} tasks)",
        paths.size(), task.futures.size());
    return taskHandle;
}

// =============================================================================
// TASK MANAGEMENT
// =============================================================================

bool AsyncBufferSync::IsTaskComplete(BufferSyncTaskHandle task) {
    std::lock_guard<std::mutex> lock(m_taskMutex);

    auto it = m_activeTasks.find(task);
    if (it == m_activeTasks.end()) {
        return true;
    }

    SyncTask& syncTask = it->second;

    if (syncTask.completed.load()) {
        return true;
    }

    for (const auto& future : syncTask.futures) {
        if (future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            return false;
        }
    }

    syncTask.completed.store(true);
    return true;
}

bool AsyncBufferSync::WaitForTask(BufferSyncTaskHandle task) {
    std::unique_lock<std::mutex> lock(m_taskMutex);

    auto it = m_activeTasks.find(task);
    if (it == m_activeTasks.end()) {
        return false;
    }

    SyncTask& syncTask = it->second;
    std::vector<std::future<void>> futures = std::move(syncTask.futures);
    std::string debugInfo = syncTask.debugInfo;

    lock.unlock();

    // Wait for all futures
    for (auto& future : futures) {
        try {
            future.get();
        }
        catch (const std::exception& e) {
            SPDLOG_ERROR("AsyncBufferSync: Task '{}' failed: {}", debugInfo, e.what());
        }
    }

    lock.lock();
    CleanupTask(task);

    return true;
}

void AsyncBufferSync::WaitForAllTasks() {
    std::vector<BufferSyncTaskHandle> taskHandles;

    {
        std::lock_guard<std::mutex> lock(m_taskMutex);
        taskHandles.reserve(m_activeTasks.size());
        for (const auto& [handle, _] : m_activeTasks) {
            taskHandles.push_back(handle);
        }
    }

    for (const auto& handle : taskHandles) {
        WaitForTask(handle);
    }
}

void AsyncBufferSync::PollCompletedTasks() {
    std::vector<BufferSyncTaskHandle> completedTasks;

    {
        std::lock_guard<std::mutex> lock(m_taskMutex);

        for (auto& [handle, task] : m_activeTasks) {
            bool allComplete = true;
            for (const auto& future : task.futures) {
                if (future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
                    allComplete = false;
                    break;
                }
            }

            if (allComplete) {
                completedTasks.push_back(handle);
            }
        }
    }

    for (const auto& handle : completedTasks) {
        std::lock_guard<std::mutex> lock(m_taskMutex);
        CleanupTask(handle);
    }
}

size_t AsyncBufferSync::GetActiveTaskCount() const {
    std::lock_guard<std::mutex> lock(m_taskMutex);
    return m_activeTasks.size();
}

// =============================================================================
// INTERNAL HELPERS
// =============================================================================

std::vector<AsyncBufferSync::BufferChunk> AsyncBufferSync::DivideBufferIntoChunks() const {
    std::vector<BufferChunk> chunks;

    if (!m_buffer) {
        return chunks;
    }

    const size_t bufferSize = m_buffer->GetBufferSize();
    const size_t numThreads = m_threadPool.getThreadCount();

    if (bufferSize < m_config.minChunkSize || numThreads <= 1) {
        chunks.push_back({ 0, static_cast<uint32_t>(bufferSize) });
        return chunks;
    }

    const size_t idealChunkSize = std::max(m_config.minChunkSize, bufferSize / numThreads);
    const size_t alignedChunkSize = (idealChunkSize + m_config.cacheLineSize - 1) & ~(m_config.cacheLineSize - 1);

    uint32_t offset = 0;
    while (offset < bufferSize) {
        uint32_t chunkSize = static_cast<uint32_t>(std::min(alignedChunkSize, bufferSize - offset));
        chunks.push_back({ offset, chunkSize });
        offset += chunkSize;
    }

    return chunks;
}

std::vector<std::vector<std::string>> AsyncBufferSync::DivideFieldsIntoBatches(
    const std::vector<std::string>& paths
) const {
    std::vector<std::vector<std::string>> batches;

    const size_t numThreads = m_threadPool.getThreadCount();

    if (paths.size() <= numThreads) {
        for (const auto& path : paths) {
            batches.push_back({ path });
        }
        return batches;
    }

    const size_t batchSize = (paths.size() + numThreads - 1) / numThreads;

    for (size_t i = 0; i < paths.size(); i += batchSize) {
        size_t end = std::min(i + batchSize, paths.size());
        batches.emplace_back(paths.begin() + i, paths.begin() + end);
    }

    return batches;
}

BufferSyncTaskHandle AsyncBufferSync::CreateTask(const std::string& debugInfo) {
    std::lock_guard<std::mutex> lock(m_taskMutex);

    BufferSyncTaskHandle handle(m_nextTaskId++);

    SyncTask& task = m_activeTasks[handle];
    task.debugInfo = debugInfo;
    task.completed.store(false);

    return handle;
}

void AsyncBufferSync::CleanupTask(BufferSyncTaskHandle task) {
    // Caller must hold m_taskMutex
    auto it = m_activeTasks.find(task);
    if (it != m_activeTasks.end()) {
        for (auto& future : it->second.futures) {
            if (future.valid()) {
                try {
                    future.get();
                }
                catch (const std::exception& e) {
                    SPDLOG_ERROR("AsyncBufferSync: Exception during cleanup: {}", e.what());
                }
            }
        }
        m_activeTasks.erase(it);
    }
}

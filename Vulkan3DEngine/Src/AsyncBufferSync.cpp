#include "AsyncBufferSync.h"
#include <spdlog/spdlog.h>

AsyncBufferSync::AsyncBufferSync(
    std::shared_ptr<ShaderLib::BufferObjectInstance> buffer,
    ThreadPool& threadPool,
    const Config& config
)
    : m_buffer(buffer)
    , m_memOps(threadPool, ConvertToMemOpsConfig(config))
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

    BufferSyncTaskHandle taskHandle = CreateTask("SyncToGPU");
    if (!taskHandle.isValid()) {
        return BufferSyncTaskHandle();
    }

    SyncTask& task = m_activeTasks[taskHandle];

    // Use AsyncMemoryOps for chunked buffer sync
    task.futures = m_memOps.ChunkedOperation(
        bufferSize,
        [buf = m_buffer](size_t offset, size_t size) {
            buf->SyncRangeToBuffer(offset, size);
        }
    );

    SPDLOG_TRACE("AsyncBufferSync: Started async SyncToGPU with {} tasks",
        task.futures.size());
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

    BufferSyncTaskHandle taskHandle = CreateTask("SyncFromGPU");
    if (!taskHandle.isValid()) {
        return BufferSyncTaskHandle();
    }

    SyncTask& task = m_activeTasks[taskHandle];

    task.futures = m_memOps.ChunkedOperation(
        bufferSize,
        [buf = m_buffer](size_t offset, size_t size) {
            buf->SyncRangeFromBuffer(offset, size);
        }
    );

    SPDLOG_TRACE("AsyncBufferSync: Started async SyncFromGPU with {} tasks",
        task.futures.size());
    return taskHandle;
}

BufferSyncTaskHandle AsyncBufferSync::SyncFieldsToGPUAsync(
    const std::vector<std::string>& paths
) {
    if (!m_buffer || paths.empty()) {
        return BufferSyncTaskHandle();
    }

    BufferSyncTaskHandle taskHandle = CreateTask(
        "SyncFieldsToGPU (" + std::to_string(paths.size()) + " fields)"
    );
    if (!taskHandle.isValid()) {
        return BufferSyncTaskHandle();
    }

    SyncTask& task = m_activeTasks[taskHandle];

    // Use BatchOperation for field-based operations
    task.futures = m_memOps.BatchOperation(
        paths.size(),
        [buf = m_buffer, &paths](size_t index) {
            buf->SyncFieldToBuffer(paths[index]);
        }
    );

    SPDLOG_TRACE("AsyncBufferSync: Started async field sync to GPU ({} fields, {} tasks)",
        paths.size(), task.futures.size());
    return taskHandle;
}

BufferSyncTaskHandle AsyncBufferSync::SyncFieldsFromGPUAsync(
    const std::vector<std::string>& paths
) {
    if (!m_buffer || paths.empty()) {
        return BufferSyncTaskHandle();
    }

    BufferSyncTaskHandle taskHandle = CreateTask(
        "SyncFieldsFromGPU (" + std::to_string(paths.size()) + " fields)"
    );
    if (!taskHandle.isValid()) {
        return BufferSyncTaskHandle();
    }

    SyncTask& task = m_activeTasks[taskHandle];

    task.futures = m_memOps.BatchOperation(
        paths.size(),
        [buf = m_buffer, &paths](size_t index) {
            buf->SyncFieldFromBuffer(paths[index]);
        }
    );

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

    if (AsyncMemoryOps::AreAllReady(syncTask.futures)) {
        syncTask.completed.store(true);
        return true;
    }

    return false;
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

    try {
        AsyncMemoryOps::WaitForAll(futures);
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("AsyncBufferSync: Task '{}' failed: {}", debugInfo, e.what());
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
            if (AsyncMemoryOps::AreAllReady(task.futures)) {
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

AsyncMemoryOps::Config AsyncBufferSync::ConvertToMemOpsConfig(const Config& config) {
    AsyncMemoryOps::Config memOpsConfig;
    memOpsConfig.minChunkSize = config.minChunkSize;
    memOpsConfig.cacheLineSize = config.cacheLineSize;
    memOpsConfig.useMultiThreading = true;
    return memOpsConfig;
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

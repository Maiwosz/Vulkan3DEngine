#include "AsyncMemoryOps.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <chrono>

AsyncMemoryOps::AsyncMemoryOps(
    ThreadPool& threadPool,
    const Config& config
)
    : m_threadPool(threadPool)
    , m_config(config)
    , m_nextOperationId(1)
{
}

AsyncMemoryOps::~AsyncMemoryOps() {
    WaitForAll();
}

// =============================================================================
// CHUNKED EXECUTION
// =============================================================================

ShaderLib::AsyncOperationHandle AsyncMemoryOps::ExecuteChunked(
    size_t totalSize,
    std::function<void(size_t offset, size_t size)> operation
) {
    if (totalSize == 0) {
        return ShaderLib::AsyncOperationHandle();
    }

    auto handle = CreateOperation("ChunkedOperation");
    if (!handle.isValid()) {
        return ShaderLib::AsyncOperationHandle();
    }

    Operation& op = m_operations[handle];

    // Single-threaded fallback for small sizes
    if (totalSize < m_config.minChunkSize || m_threadPool.getThreadCount() <= 1) {
        op.futures.push_back(m_threadPool.enqueue(
            [operation, totalSize]() {
                operation(0, totalSize);
            }
        ));
        return handle;
    }

    // Calculate chunks for parallel execution
    auto chunks = CalculateChunks(totalSize);
    op.futures.reserve(chunks.size());

    for (const auto& chunk : chunks) {
        op.futures.push_back(m_threadPool.enqueue(
            [operation, offset = chunk.offset, size = chunk.size]() {
                operation(offset, size);
            }
        ));
    }

    SPDLOG_TRACE("AsyncMemoryOperations: Started chunked operation with {} tasks",
        op.futures.size());

    return handle;
}

// =============================================================================
// BATCH EXECUTION
// =============================================================================

ShaderLib::AsyncOperationHandle AsyncMemoryOps::ExecuteBatch(
    size_t itemCount,
    std::function<void(size_t index)> operation
) {
    if (itemCount == 0) {
        return ShaderLib::AsyncOperationHandle();
    }

    auto handle = CreateOperation("BatchOperation");
    if (!handle.isValid()) {
        return ShaderLib::AsyncOperationHandle();
    }

    Operation& op = m_operations[handle];

    const size_t threadCount = m_threadPool.getThreadCount();

    // Single-threaded fallback
    if (itemCount <= threadCount || threadCount <= 1) {
        op.futures.push_back(m_threadPool.enqueue(
            [operation, itemCount]() {
                for (size_t i = 0; i < itemCount; ++i) {
                    operation(i);
                }
            }
        ));
        return handle;
    }

    // Parallel batch execution
    const size_t batchSize = (itemCount + threadCount - 1) / threadCount;

    for (size_t start = 0; start < itemCount; start += batchSize) {
        size_t end = std::min(start + batchSize, itemCount);

        op.futures.push_back(m_threadPool.enqueue(
            [operation, start, end]() {
                for (size_t i = start; i < end; ++i) {
                    operation(i);
                }
            }
        ));
    }

    SPDLOG_TRACE("AsyncMemoryOperations: Started batch operation with {} batches",
        op.futures.size());

    return handle;
}

// =============================================================================
// OPERATION MANAGEMENT
// =============================================================================

bool AsyncMemoryOps::IsOperationComplete(
    ShaderLib::AsyncOperationHandle handle
) {
    std::lock_guard<std::mutex> lock(m_operationMutex);

    auto it = m_operations.find(handle);
    if (it == m_operations.end()) {
        return true;
    }

    Operation& op = it->second;

    if (op.completed.load()) {
        return true;
    }

    // Check if all futures are ready
    bool allReady = true;
    for (const auto& future : op.futures) {
        if (future.valid() &&
            future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            allReady = false;
            break;
        }
    }

    if (allReady) {
        op.completed.store(true);
    }

    return allReady;
}

void AsyncMemoryOps::WaitForOperation(
    ShaderLib::AsyncOperationHandle handle
) {
    std::unique_lock<std::mutex> lock(m_operationMutex);

    auto it = m_operations.find(handle);
    if (it == m_operations.end()) {
        return;
    }

    Operation& op = it->second;
    std::vector<std::future<void>> futures = std::move(op.futures);
    std::string debugInfo = op.debugInfo;

    lock.unlock();

    // Wait for all futures
    try {
        for (auto& future : futures) {
            if (future.valid()) {
                future.get();
            }
        }
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("AsyncMemoryOperations: Operation '{}' failed: {}",
            debugInfo, e.what());
    }

    lock.lock();
    CleanupOperation(handle);
}

void AsyncMemoryOps::WaitForAll() {
    std::vector<ShaderLib::AsyncOperationHandle> handles;

    {
        std::lock_guard<std::mutex> lock(m_operationMutex);
        handles.reserve(m_operations.size());
        for (const auto& [handle, _] : m_operations) {
            handles.push_back(handle);
        }
    }

    for (const auto& handle : handles) {
        WaitForOperation(handle);
    }
}

void AsyncMemoryOps::PollCompleted() {
    std::vector<ShaderLib::AsyncOperationHandle> completedOps;

    {
        std::lock_guard<std::mutex> lock(m_operationMutex);

        for (auto& [handle, op] : m_operations) {
            bool allReady = true;
            for (const auto& future : op.futures) {
                if (future.valid() &&
                    future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
                    allReady = false;
                    break;
                }
            }

            if (allReady) {
                completedOps.push_back(handle);
            }
        }
    }

    for (const auto& handle : completedOps) {
        std::lock_guard<std::mutex> lock(m_operationMutex);
        CleanupOperation(handle);
    }
}

size_t AsyncMemoryOps::GetActiveOperationCount() const {
    std::lock_guard<std::mutex> lock(m_operationMutex);
    return m_operations.size();
}

// =============================================================================
// INTERNAL HELPERS
// =============================================================================

std::vector<AsyncMemoryOps::Chunk>
AsyncMemoryOps::CalculateChunks(size_t totalSize) const {
    std::vector<Chunk> chunks;

    if (totalSize == 0) {
        return chunks;
    }

    const size_t threadCount = m_threadPool.getThreadCount();

    // Single chunk if size is too small or single-threaded
    if (totalSize < m_config.minChunkSize || threadCount <= 1) {
        chunks.push_back({ 0, totalSize });
        return chunks;
    }

    // Calculate number of chunks
    size_t numChunks = threadCount;
    if (m_config.maxChunks > 0) {
        numChunks = std::min(numChunks, m_config.maxChunks);
    }

    // Calculate ideal chunk size
    const size_t idealChunkSize = std::max(
        m_config.minChunkSize,
        totalSize / numChunks
    );

    // Align to cache line for better performance
    const size_t alignedChunkSize = (idealChunkSize + m_config.cacheLineSize - 1)
        & ~(m_config.cacheLineSize - 1);

    // Divide buffer into chunks
    size_t offset = 0;
    while (offset < totalSize) {
        size_t chunkSize = std::min(alignedChunkSize, totalSize - offset);
        chunks.push_back({ offset, chunkSize });
        offset += chunkSize;
    }

    return chunks;
}

ShaderLib::AsyncOperationHandle AsyncMemoryOps::CreateOperation(
    const std::string& debugInfo
) {
    std::lock_guard<std::mutex> lock(m_operationMutex);

    ShaderLib::AsyncOperationHandle handle(m_nextOperationId++);

    Operation& op = m_operations[handle];
    op.debugInfo = debugInfo;
    op.completed.store(false);

    return handle;
}

void AsyncMemoryOps::CleanupOperation(
    ShaderLib::AsyncOperationHandle handle
) {
    auto it = m_operations.find(handle);
    if (it != m_operations.end()) {
        for (auto& future : it->second.futures) {
            if (future.valid()) {
                try {
                    future.get();
                }
                catch (const std::exception& e) {
                    SPDLOG_ERROR("AsyncMemoryOperations: Exception during cleanup: {}",
                        e.what());
                }
            }
        }
        m_operations.erase(it);
    }
}

#include "AsyncMemoryOps.h"
#include <algorithm>
#include <cstring>
#include <chrono>

AsyncMemoryOps::AsyncMemoryOps(ThreadPool& threadPool, const Config& config)
    : m_threadPool(threadPool)
    , m_config(config)
{
}

std::vector<std::future<void>> AsyncMemoryOps::Memcpy(
    void* dest,
    const void* src,
    size_t size
) {
    return ChunkedOperation(
        size,
        [dest, src](size_t offset, size_t chunkSize) {
            std::memcpy(
                static_cast<char*>(dest) + offset,
                static_cast<const char*>(src) + offset,
                chunkSize
            );
        }
    );
}

std::vector<AsyncMemoryOps::Chunk> AsyncMemoryOps::CalculateChunks(size_t totalSize) const {
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

void AsyncMemoryOps::WaitForAll(std::vector<std::future<void>>& futures) {
    for (auto& future : futures) {
        if (future.valid()) {
            future.get();
        }
    }
}

bool AsyncMemoryOps::AreAllReady(const std::vector<std::future<void>>& futures) {
    for (const auto& future : futures) {
        if (future.valid() &&
            future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            return false;
        }
    }
    return true;
}

size_t AsyncMemoryOps::CountReady(const std::vector<std::future<void>>& futures) {
    size_t count = 0;
    for (const auto& future : futures) {
        if (!future.valid() ||
            future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            ++count;
        }
    }
    return count;
}

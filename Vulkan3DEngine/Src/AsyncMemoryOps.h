#pragma once
#include <vector>
#include <future>
#include <functional>
#include "ThreadPool.h"

class AsyncMemoryOps {
public:
    struct Config {
        size_t minChunkSize = 4096;
        size_t cacheLineSize = 64;
        bool useMultiThreading = true;
        size_t maxChunks = 0;
    };

    explicit AsyncMemoryOps(ThreadPool& threadPool, const Config& config = Config{});

    std::vector<std::future<void>> Memcpy(
        void* dest,
        const void* src,
        size_t size
    );

    // Używamy std::function zamiast perfect forwarding
    // Dzięki temu lambda z przechwyconym shared_ptr jest poprawnie kopiowana
    template<typename Operation>
    std::vector<std::future<void>> ChunkedOperation(
        size_t totalSize,
        Operation&& operation
    ) {
        std::vector<std::future<void>> futures;

        if (totalSize == 0) {
            return futures;
        }

        // Konwertujemy lambdę na std::function
        // To zapewnia poprawne kopiowanie przechwyconych shared_ptr
        std::function<void(size_t, size_t)> op = std::forward<Operation>(operation);

        if (!m_config.useMultiThreading ||
            totalSize < m_config.minChunkSize ||
            m_threadPool.getThreadCount() <= 1) {

            futures.push_back(m_threadPool.enqueue(
                [op, totalSize]() {
                    op(0, totalSize);
                }
            ));
            return futures;
        }

        auto chunks = CalculateChunks(totalSize);
        futures.reserve(chunks.size());

        for (const auto& chunk : chunks) {
            // Każda lambda dostaje własną kopię std::function
            futures.push_back(m_threadPool.enqueue(
                [op, offset = chunk.offset, size = chunk.size]() {
                    op(offset, size);
                }
            ));
        }

        return futures;
    }

    template<typename Operation>
    std::vector<std::future<void>> BatchOperation(
        size_t itemCount,
        Operation&& operation
    ) {
        std::vector<std::future<void>> futures;

        if (itemCount == 0) {
            return futures;
        }

        std::function<void(size_t)> op = std::forward<Operation>(operation);

        const size_t threadCount = m_threadPool.getThreadCount();

        if (!m_config.useMultiThreading || itemCount <= threadCount || threadCount <= 1) {
            futures.push_back(m_threadPool.enqueue(
                [op, itemCount]() {
                    for (size_t i = 0; i < itemCount; ++i) {
                        op(i);
                    }
                }
            ));
            return futures;
        }

        const size_t batchSize = (itemCount + threadCount - 1) / threadCount;

        for (size_t start = 0; start < itemCount; start += batchSize) {
            size_t end = std::min(start + batchSize, itemCount);

            futures.push_back(m_threadPool.enqueue(
                [op, start, end]() {
                    for (size_t i = start; i < end; ++i) {
                        op(i);
                    }
                }
            ));
        }

        return futures;
    }

    void SetConfig(const Config& config) { m_config = config; }
    const Config& GetConfig() const { return m_config; }

    static void WaitForAll(std::vector<std::future<void>>& futures);
    static bool AreAllReady(const std::vector<std::future<void>>& futures);
    static size_t CountReady(const std::vector<std::future<void>>& futures);

private:
    struct Chunk {
        size_t offset;
        size_t size;
    };

    std::vector<Chunk> CalculateChunks(size_t totalSize) const;

    ThreadPool& m_threadPool;
    Config m_config;
};

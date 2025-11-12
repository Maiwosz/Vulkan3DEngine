#pragma once
#include "IAsyncMemoryOperations.h"
#include "ThreadPool.h"
#include <unordered_map>
#include <mutex>
#include <atomic>

/**
 * Implementation of IAsyncMemoryOperations using ThreadPool.
 *
 * This is the bridge between ShaderLib (which doesn't know about ThreadPool)
 * and the engine's threading system.
 */
class AsyncMemoryOps : public ShaderLib::IAsyncMemoryOperations {
public:
    struct Config {
        size_t minChunkSize = 4096;      // Minimum chunk size for multithreading
        size_t cacheLineSize = 64;       // Cache line alignment
        size_t maxChunks = 0;            // Max chunks (0 = unlimited)
    };

    explicit AsyncMemoryOps(
        ThreadPool& threadPool,
        const Config& config = Config{}
    );

    ~AsyncMemoryOps() override;

    // IAsyncMemoryOperations interface
    ShaderLib::AsyncOperationHandle ExecuteChunked(
        size_t totalSize,
        std::function<void(size_t offset, size_t size)> operation
    ) override;

    ShaderLib::AsyncOperationHandle ExecuteBatch(
        size_t itemCount,
        std::function<void(size_t index)> operation
    ) override;

    bool IsOperationComplete(ShaderLib::AsyncOperationHandle handle) override;
    void WaitForOperation(ShaderLib::AsyncOperationHandle handle) override;
    void WaitForAll() override;
    void PollCompleted() override;
    size_t GetActiveOperationCount() const override;

    // Configuration
    void SetConfig(const Config& config) { m_config = config; }
    const Config& GetConfig() const { return m_config; }

private:
    struct Chunk {
        size_t offset;
        size_t size;
    };

    struct Operation {
        std::vector<std::future<void>> futures;
        std::string debugInfo;
        std::atomic<bool> completed;

        Operation() : completed(false) {}
    };

    std::vector<Chunk> CalculateChunks(size_t totalSize) const;
    ShaderLib::AsyncOperationHandle CreateOperation(const std::string& debugInfo);
    void CleanupOperation(ShaderLib::AsyncOperationHandle handle);

    ThreadPool& m_threadPool;
    Config m_config;

    // Operation tracking
    std::unordered_map<ShaderLib::AsyncOperationHandle, Operation> m_operations;
    uint32_t m_nextOperationId;
    mutable std::mutex m_operationMutex;
};

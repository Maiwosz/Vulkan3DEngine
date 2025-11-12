#pragma once
#include <memory>
#include <vector>
#include <future>
#include <atomic>
#include <mutex>
#include "BufferObjectInstance.h"
#include "ThreadPool.h"
#include "AsyncMemoryOps.h"
#include "Handle.h"

DEFINE_HANDLE_TYPE(BufferSyncTaskHandle, uint32_t)

/**
 * AsyncBufferSync - Async/multithreaded wrapper for single BufferObjectInstance
 *
 * Provides:
 * - Async CPU↔GPU synchronization with automatic chunking
 * - Task tracking and management
 * - Thread-safe operations
 * - Partial sync for specific fields
 *
 * Uses AsyncMemoryOps internally for efficient parallel processing.
 */
    class AsyncBufferSync {
    public:
        struct Config {
            size_t minChunkSize = 4096;      // Minimum chunk size for multithreading
            size_t cacheLineSize = 64;       // Alignment for cache-friendly chunks (typical CPU cache line)
        };

        AsyncBufferSync(
            std::shared_ptr<ShaderLib::BufferObjectInstance> buffer,
            ThreadPool& threadPool,
            const Config& config = Config{}
        );

        ~AsyncBufferSync();

        // Synchronous operations (blocking)
        void SyncToGPU();
        void SyncFromGPU();
        void SyncFieldToGPU(const std::string& path);
        void SyncFieldFromGPU(const std::string& path);
        void SyncFieldsToGPU(const std::vector<std::string>& paths);
        void SyncFieldsFromGPU(const std::vector<std::string>& paths);

        // Asynchronous operations (non-blocking)
        BufferSyncTaskHandle SyncToGPUAsync();
        BufferSyncTaskHandle SyncFromGPUAsync();
        BufferSyncTaskHandle SyncFieldsToGPUAsync(const std::vector<std::string>& paths);
        BufferSyncTaskHandle SyncFieldsFromGPUAsync(const std::vector<std::string>& paths);

        // Task management
        bool IsTaskComplete(BufferSyncTaskHandle task);
        bool WaitForTask(BufferSyncTaskHandle task);
        void WaitForAllTasks();
        void PollCompletedTasks();
        size_t GetActiveTaskCount() const;

        // Buffer access
        std::shared_ptr<ShaderLib::BufferObjectInstance> GetBuffer() const { return m_buffer; }
        bool HasBuffer() const { return m_buffer != nullptr; }

        // Configuration
        void SetConfig(const Config& config) {
            m_config = config;
            m_memOps.SetConfig(ConvertToMemOpsConfig(config));
        }
        const Config& GetConfig() const { return m_config; }

    private:
        struct SyncTask {
            std::vector<std::future<void>> futures;
            std::string debugInfo;
            std::atomic<bool> completed;

            SyncTask() : completed(false) {}
        };

        // Task management
        BufferSyncTaskHandle CreateTask(const std::string& debugInfo);
        void CleanupTask(BufferSyncTaskHandle task);

        // Config conversion
        static AsyncMemoryOps::Config ConvertToMemOpsConfig(const Config& config);

        std::shared_ptr<ShaderLib::BufferObjectInstance> m_buffer;
        AsyncMemoryOps m_memOps;
        Config m_config;

        // Task tracking
        std::unordered_map<BufferSyncTaskHandle, SyncTask> m_activeTasks;
        uint32_t m_nextTaskId;
        mutable std::mutex m_taskMutex;
};

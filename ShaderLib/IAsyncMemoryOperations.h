#pragma once
#include <functional>
#include <vector>
#include <future>
#include <cstddef>

namespace ShaderLib {

    struct AsyncOperationHandle {
        uint32_t id;

        // Konstruktory
        constexpr AsyncOperationHandle() : id(0) {}
        constexpr explicit AsyncOperationHandle(uint32_t id) : id(id) {}

        // Operatory porównania
        bool operator==(const AsyncOperationHandle&) const = default;
        bool operator!=(const AsyncOperationHandle&) const = default;

        // Sprawdzanie ważności uchwytu
        constexpr bool isValid() const { return id != 0; }
        explicit operator bool() const { return id != 0; }
    };

    /**
    * Interface for asynchronous memory operations.
    *
    * BufferObjectInstance can optionally use this interface to perform
    * multi-threaded memory operations and asynchronous GPU synchronization.
    *
    * If not set, all operations fall back to synchronous single-threaded execution.
    */
    class IAsyncMemoryOperations {
    public:
        virtual ~IAsyncMemoryOperations() = default;

        /**
            * Execute operation in chunks across multiple threads.
            *
            * @param totalSize Total size to process
            * @param operation Function(offset, size) to execute for each chunk
            * @return Handle to track operation completion
            */
        virtual AsyncOperationHandle ExecuteChunked(
            size_t totalSize,
            std::function<void(size_t offset, size_t size)> operation
        ) = 0;

        /**
            * Execute batch operation across multiple threads.
            *
            * @param itemCount Number of items to process
            * @param operation Function(index) to execute for each item
            * @return Handle to track operation completion
            */
        virtual AsyncOperationHandle ExecuteBatch(
            size_t itemCount,
            std::function<void(size_t index)> operation
        ) = 0;

        /**
            * Check if async operation is complete.
            * Non-blocking.
            */
        virtual bool IsOperationComplete(AsyncOperationHandle handle) = 0;

        /**
            * Wait for async operation to complete.
            * Blocking.
            */
        virtual void WaitForOperation(AsyncOperationHandle handle) = 0;

        /**
            * Wait for all pending operations to complete.
            * Blocking.
            */
        virtual void WaitForAll() = 0;

        /**
            * Poll and cleanup completed operations.
            * Non-blocking.
            */
        virtual void PollCompleted() = 0;

        /**
            * Get number of active (pending) operations.
            */
        virtual size_t GetActiveOperationCount() const = 0;
    };

} // namespace ShaderLib

// Hash specialization for AsyncOperationHandle
namespace std {
    template<>
    struct hash<ShaderLib::AsyncOperationHandle> {
        size_t operator()(const ShaderLib::AsyncOperationHandle& handle) const noexcept {
            return std::hash<uint32_t>{}(handle.id);
        }
    };
}

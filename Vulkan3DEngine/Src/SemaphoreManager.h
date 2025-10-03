#pragma once
#include "IResourceManager.h"
#include "Handle.h"
#include <atomic>
#include <unordered_map>
#include <string>
#include <mutex>
#include <memory>

// Semaphore resource structure
struct Semaphore {
    std::atomic<int> value;
    std::string name; // Optional for debugging
    int referenceCount = 0;

    explicit Semaphore(int initialValue = 0, const std::string& debugName = "")
        : value(initialValue), name(debugName) {
    }
};

class SemaphoreManager : public IResourceManager<SemaphoreHandle, Semaphore> {
public:
    SemaphoreManager() = default;
    ~SemaphoreManager() = default;

    // Non-copyable, non-movable for thread safety
    SemaphoreManager(const SemaphoreManager&) = delete;
    SemaphoreManager& operator=(const SemaphoreManager&) = delete;
    SemaphoreManager(SemaphoreManager&&) = delete;
    SemaphoreManager& operator=(SemaphoreManager&&) = delete;

    // IResourceManager interface implementation
    Semaphore* getResource(SemaphoreHandle handle) override;
    const Semaphore* getResource(SemaphoreHandle handle) const; // Add const overload
    bool isValid(SemaphoreHandle handle) const override;
    void releaseResource(SemaphoreHandle handle) override;
    void addReference(SemaphoreHandle handle) override;
    void removeReference(SemaphoreHandle handle) override;

    // Semaphore-specific operations
    SemaphoreHandle createSemaphore(const std::string& name, int initialValue = 0);
    SemaphoreHandle findSemaphore(const std::string& name) const;

    // Semaphore operations
    void notify(SemaphoreHandle handle, int count = 1);
    bool tryWait(SemaphoreHandle handle, int count = 1);
    void wait(SemaphoreHandle handle, int count = 1);
    int getValue(SemaphoreHandle handle) const;
    void setValue(SemaphoreHandle handle, int value);

    // Batch operations for pipeline management
    void resetAllSemaphores();
    void resetSemaphore(SemaphoreHandle handle, int value = 0);

    // Debug and information
    std::string getSemaphoreName(SemaphoreHandle handle) const;
    size_t getSemaphoreCount() const;

    // Frame lifecycle
    void reset();

private:
    mutable std::mutex m_mutex;
    std::unordered_map<SemaphoreHandle, std::unique_ptr<Semaphore>> m_semaphores;
    std::unordered_map<std::string, SemaphoreHandle> m_nameToHandle;
    uint32_t m_nextId = 1;

    SemaphoreHandle generateHandle();
};
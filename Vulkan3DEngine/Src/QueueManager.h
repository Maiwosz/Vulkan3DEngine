#pragma once
#include <vulkan/vulkan.h>
#include <memory>
#include <mutex>
#include <unordered_map>

// Forward declaration
struct QueueFamilyIndices;

// Globalny enum dla typów kolejek - używany w całym projekcie
enum class QueueType {
    Graphics,
    Present,
    Transfer,
    Compute
};

// Hash specialization for QueueType - MUST be declared before use
namespace std {
    template<>
    struct hash<QueueType> {
        size_t operator()(QueueType type) const noexcept {
            return hash<int>{}(static_cast<int>(type));
        }
    };
}

// Thread-safe wrapper dla kolejki Vulkan
class QueueWrapper {
public:
    explicit QueueWrapper(VkQueue queue, uint32_t familyIndex, QueueType type)
        : m_queue(queue), m_familyIndex(familyIndex), m_type(type) {
    }

    class Lock {
    public:
        Lock(VkQueue queue, std::unique_lock<std::mutex>&& lock)
            : m_queue(queue), m_lock(std::move(lock)) {
        }

        VkQueue getQueue() const { return m_queue; }

    private:
        VkQueue m_queue;
        std::unique_lock<std::mutex> m_lock;
    };

    [[nodiscard]] Lock lock() {
        return Lock(m_queue, std::unique_lock<std::mutex>(m_mutex));
    }

    [[nodiscard]] VkQueue getRawQueue() const { return m_queue; }
    [[nodiscard]] uint32_t getFamilyIndex() const { return m_familyIndex; }
    [[nodiscard]] QueueType getType() const { return m_type; }

private:
    VkQueue m_queue;
    uint32_t m_familyIndex;
    QueueType m_type;
    std::mutex m_mutex;
};

// Manager zarządzający dostępem do kolejek z automatyczną detekcją współdzielenia
class QueueManager {
public:
    QueueManager(VkDevice device, const QueueFamilyIndices& indices);
    ~QueueManager() = default;

    // Bezpieczny dostęp przez wrapper
    [[nodiscard]] std::shared_ptr<QueueWrapper> getQueue(QueueType type) const;

    // Raw queue (użyj ostrożnie - bez synchronizacji!)
    [[nodiscard]] VkQueue getRawQueue(QueueType type) const;

    // Informacje o kolejkach
    [[nodiscard]] uint32_t getQueueFamilyIndex(QueueType type) const;
    [[nodiscard]] bool isQueueShared(QueueType type) const;

private:
    void initializeQueues(VkDevice device, const QueueFamilyIndices& indices);
    void detectSharedQueues();

    std::unordered_map<QueueType, std::shared_ptr<QueueWrapper>> m_queues;
    std::unordered_map<QueueType, bool> m_sharedQueues;
};

#include "QueueManager.h"
#include "PhysicalDevice.h"
#include <stdexcept>
#include <spdlog/spdlog.h>

QueueManager::QueueManager(VkDevice device, const QueueFamilyIndices& indices) {
    initializeQueues(device, indices);
    detectSharedQueues();
}

void QueueManager::initializeQueues(VkDevice device, const QueueFamilyIndices& indices) {
    if (!indices.graphicsFamily.has_value() || !indices.presentFamily.has_value() ||
        !indices.transferFamily.has_value() || !indices.computeFamily.has_value()) {
        throw std::runtime_error("Not all required queue families are available");
    }

    // Pobierz wszystkie kolejki z device
    VkQueue graphicsQueue, presentQueue, transferQueue, computeQueue;
    vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
    vkGetDeviceQueue(device, indices.transferFamily.value(), 0, &transferQueue);
    vkGetDeviceQueue(device, indices.computeFamily.value(), 0, &computeQueue);

    // ✅ Mapuj VkQueue handle -> lista typów kolejek które go używają
    std::unordered_map<VkQueue, std::vector<QueueType>> queueToTypes;

    queueToTypes[graphicsQueue].push_back(QueueType::Graphics);
    queueToTypes[presentQueue].push_back(QueueType::Present);
    queueToTypes[transferQueue].push_back(QueueType::Transfer);
    queueToTypes[computeQueue].push_back(QueueType::Compute);

    // Mapuj VkQueue -> już utworzony wrapper
    std::unordered_map<VkQueue, std::shared_ptr<QueueWrapper>> queueToWrapper;

    // Struktura pomocnicza: (QueueType, VkQueue, familyIndex)
    struct QueueInfo {
        QueueType type;
        VkQueue queue;
        uint32_t familyIndex;
    };

    std::vector<QueueInfo> allQueues = {
        {QueueType::Graphics, graphicsQueue, indices.graphicsFamily.value()},
        {QueueType::Present, presentQueue, indices.presentFamily.value()},
        {QueueType::Transfer, transferQueue, indices.transferFamily.value()},
        {QueueType::Compute, computeQueue, indices.computeFamily.value()}
    };

    // Dla każdej kolejki: albo utwórz nowy wrapper, albo użyj istniejącego
    for (const auto& info : allQueues) {
        if (queueToWrapper.find(info.queue) != queueToWrapper.end()) {
            // Ten VkQueue już ma wrapper - użyj go ponownie
            m_queues[info.type] = queueToWrapper[info.queue];

            SPDLOG_INFO("Queue type {} SHARES wrapper with existing queue (handle: 0x{:x})",
                static_cast<int>(info.type),
                reinterpret_cast<uintptr_t>(info.queue));
        }
        else {
            // Pierwszy raz widzimy ten VkQueue - utwórz nowy wrapper
            auto wrapper = std::make_shared<QueueWrapper>(
                info.queue, info.familyIndex, info.type);

            m_queues[info.type] = wrapper;
            queueToWrapper[info.queue] = wrapper;

            SPDLOG_INFO("Queue type {} created NEW wrapper (handle: 0x{:x})",
                static_cast<int>(info.type),
                reinterpret_cast<uintptr_t>(info.queue));
        }
    }
}

void QueueManager::detectSharedQueues() {
    // Wykryj które typy kolejek są współdzielone (ten sam VkQueue handle)
    std::unordered_map<VkQueue, std::vector<QueueType>> queueMap;

    for (const auto& [type, wrapper] : m_queues) {
        queueMap[wrapper->getRawQueue()].push_back(type);
    }

    // Oznacz współdzielone kolejki
    for (auto& [type, wrapper] : m_queues) {
        VkQueue queue = wrapper->getRawQueue();
        m_sharedQueues[type] = queueMap[queue].size() > 1;

        if (m_sharedQueues[type]) {
            SPDLOG_DEBUG("Queue type {} is SHARED with {} other types",
                static_cast<int>(type), queueMap[queue].size() - 1);
        }
    }
}

std::shared_ptr<QueueWrapper> QueueManager::getQueue(QueueType type) const {
    auto it = m_queues.find(type);
    if (it == m_queues.end()) {
        throw std::invalid_argument("Invalid queue type");
    }
    return it->second;
}

VkQueue QueueManager::getRawQueue(QueueType type) const {
    return getQueue(type)->getRawQueue();
}

uint32_t QueueManager::getQueueFamilyIndex(QueueType type) const {
    return getQueue(type)->getFamilyIndex();
}

bool QueueManager::isQueueShared(QueueType type) const {
    auto it = m_sharedQueues.find(type);
    return it != m_sharedQueues.end() && it->second;
}

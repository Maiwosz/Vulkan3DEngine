#include "SemaphoreManager.h"
#include <spdlog/spdlog.h>
#include <thread>
#include <chrono>

SemaphoreHandle SemaphoreManager::generateHandle() {
    return SemaphoreHandle(m_nextId++);
}

Semaphore* SemaphoreManager::getResource(SemaphoreHandle handle) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_semaphores.find(handle);
    return (it != m_semaphores.end()) ? it->second.get() : nullptr;
}

const Semaphore* SemaphoreManager::getResource(SemaphoreHandle handle) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_semaphores.find(handle);
    return (it != m_semaphores.end()) ? it->second.get() : nullptr;
}

bool SemaphoreManager::isValid(SemaphoreHandle handle) const {
    if (!handle.isValid()) return false;

    std::lock_guard<std::mutex> lock(m_mutex);
    return m_semaphores.find(handle) != m_semaphores.end();
}

void SemaphoreManager::releaseResource(SemaphoreHandle handle) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_semaphores.find(handle);
    if (it != m_semaphores.end()) {
        // Remove from name mapping if exists
        if (!it->second->name.empty()) {
            m_nameToHandle.erase(it->second->name);
        }

        SPDLOG_DEBUG("Released semaphore '{}' with handle {}",
            it->second->name, handle.id);
        m_semaphores.erase(it);
    }
}

void SemaphoreManager::addReference(SemaphoreHandle handle) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_semaphores.find(handle);
    if (it != m_semaphores.end()) {
        it->second->referenceCount++;
        SPDLOG_DEBUG("Added reference to semaphore '{}', ref count: {}",
            it->second->name, it->second->referenceCount);
    }
}

void SemaphoreManager::removeReference(SemaphoreHandle handle) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_semaphores.find(handle);
    if (it != m_semaphores.end()) {
        it->second->referenceCount--;
        SPDLOG_DEBUG("Removed reference from semaphore '{}', ref count: {}",
            it->second->name, it->second->referenceCount);

        // Auto-cleanup when no references remain
        if (it->second->referenceCount <= 0) {
            SPDLOG_DEBUG("Auto-releasing semaphore '{}' with no references", it->second->name);
            releaseResource(handle);
        }
    }
}

SemaphoreHandle SemaphoreManager::createSemaphore(const std::string& name, int initialValue) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check if semaphore with this name already exists
    auto nameIt = m_nameToHandle.find(name);
    if (nameIt != m_nameToHandle.end()) {
        SPDLOG_WARN("Semaphore '{}' already exists, returning existing handle", name);
        return nameIt->second;
    }

    SemaphoreHandle handle = generateHandle();
    auto semaphore = std::make_unique<Semaphore>(initialValue, name);
    semaphore->referenceCount = 1; // Start with one reference

    m_semaphores[handle] = std::move(semaphore);
    if (!name.empty()) {
        m_nameToHandle[name] = handle;
    }

    SPDLOG_DEBUG("Created semaphore '{}' with handle {} and initial value {}",
        name, handle.id, initialValue);

    return handle;
}

SemaphoreHandle SemaphoreManager::findSemaphore(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_nameToHandle.find(name);
    return (it != m_nameToHandle.end()) ? it->second : SemaphoreHandle{};
}

void SemaphoreManager::notify(SemaphoreHandle handle, int count) {
    auto* semaphore = getResource(handle);
    if (!semaphore) {
        SPDLOG_WARN("Attempted to notify invalid semaphore handle {}", handle.id);
        return;
    }

    int oldValue = semaphore->value.fetch_add(count);
    SPDLOG_DEBUG("Semaphore '{}' (handle {}) notified by {}, new value: {}",
        semaphore->name, handle.id, count, oldValue + count);
}

bool SemaphoreManager::tryWait(SemaphoreHandle handle, int count) {
    auto* semaphore = getResource(handle);
    if (!semaphore) {
        SPDLOG_WARN("Attempted to wait on invalid semaphore handle {}", handle.id);
        return false;
    }

    int expected = semaphore->value.load();
    while (expected >= count) {
        if (semaphore->value.compare_exchange_weak(expected, expected - count)) {
            SPDLOG_DEBUG("Semaphore '{}' (handle {}) decremented by {}, new value: {}",
                semaphore->name, handle.id, count, expected - count);
            return true;
        }
    }
    return false;
}

void SemaphoreManager::wait(SemaphoreHandle handle, int count) {
    while (!tryWait(handle, count)) {
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
}

int SemaphoreManager::getValue(SemaphoreHandle handle) const {
    const Semaphore* semaphore = getResource(handle);
    return semaphore ? semaphore->value.load() : -1;
}

void SemaphoreManager::setValue(SemaphoreHandle handle, int value) {
    Semaphore* semaphore = getResource(handle);
    if (semaphore) {
        semaphore->value.store(value);
        SPDLOG_DEBUG("Semaphore '{}' (handle {}) value set to {}",
            semaphore->name, handle.id, value);
    }
    else {
        SPDLOG_WARN("Attempted to set value on invalid semaphore handle {}", handle.id);
    }
}

void SemaphoreManager::resetAllSemaphores() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& [handle, semaphore] : m_semaphores) {
        semaphore->value.store(0);
        SPDLOG_DEBUG("Reset semaphore '{}' (handle {}) to 0", semaphore->name, handle.id);
    }
    SPDLOG_DEBUG("Reset all {} semaphores to 0", m_semaphores.size());
}

void SemaphoreManager::resetSemaphore(SemaphoreHandle handle, int value) {
    setValue(handle, value);
}

std::string SemaphoreManager::getSemaphoreName(SemaphoreHandle handle) const {
    const Semaphore* semaphore = getResource(handle);
    return semaphore ? semaphore->name : "";
}

size_t SemaphoreManager::getSemaphoreCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_semaphores.size();
}

void SemaphoreManager::reset() {
    resetAllSemaphores();
    SPDLOG_DEBUG("SemaphoreManager reset for next frame");
}
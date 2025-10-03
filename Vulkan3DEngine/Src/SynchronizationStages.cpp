#include "SynchronizationStages.h"
#include <spdlog/spdlog.h>

// WaitForStage Implementation
WaitForStage::WaitForStage(ProcessingContext& context, SemaphoreHandle semaphoreHandle, int waitCount)
    : ProcessingStage(context), m_semaphoreHandle(semaphoreHandle), m_waitCount(waitCount) {

    auto& semManager = m_context.getSemaphoreManager();
    std::string semaphoreName = semManager.getSemaphoreName(semaphoreHandle);

    SPDLOG_DEBUG("Created WaitForStage for semaphore '{}' (handle {}) with count {}",
        semaphoreName, semaphoreHandle.id, waitCount);
}

ProcessingResult WaitForStage::process(std::shared_ptr<RenderOrder> order) {
    try {
        auto& semManager = m_context.getSemaphoreManager();
        std::string semaphoreName = semManager.getSemaphoreName(m_semaphoreHandle);

        SPDLOG_DEBUG("WaitForStage attempting wait on semaphore '{}' (handle {}) for entity {}",
            semaphoreName, m_semaphoreHandle.id, order->entity.id);

        // Use tryWait instead of blocking wait
        bool waitSuccessful = semManager.tryWait(m_semaphoreHandle, m_waitCount);

        if (waitSuccessful) {
            SPDLOG_DEBUG("WaitForStage completed wait on semaphore '{}' (handle {}) for entity {}",
                semaphoreName, m_semaphoreHandle.id, order->entity.id);
            return ProcessingResult::Success;
        }
        else {
            SPDLOG_DEBUG("WaitForStage blocked on semaphore '{}' (handle {}) for entity {} - will retry later",
                semaphoreName, m_semaphoreHandle.id, order->entity.id);
            return ProcessingResult::Blocked;
        }
    }
    catch (const std::exception& e) {
        auto& semManager = m_context.getSemaphoreManager();
        std::string semaphoreName = semManager.getSemaphoreName(m_semaphoreHandle);

        SPDLOG_ERROR("WaitForStage failed on semaphore '{}' (handle {}) for entity {}: {}",
            semaphoreName, m_semaphoreHandle.id, order->entity.id, e.what());
        return ProcessingResult::Failure;
    }
}

// NotifyStage Implementation  
NotifyStage::NotifyStage(ProcessingContext& context, SemaphoreHandle semaphoreHandle, int notifyCount)
    : ProcessingStage(context), m_semaphoreHandle(semaphoreHandle), m_notifyCount(notifyCount) {

    auto& semManager = m_context.getSemaphoreManager();
    std::string semaphoreName = semManager.getSemaphoreName(semaphoreHandle);

    SPDLOG_DEBUG("Created NotifyStage for semaphore '{}' (handle {}) with count {}",
        semaphoreName, semaphoreHandle.id, notifyCount);
}

ProcessingResult NotifyStage::process(std::shared_ptr<RenderOrder> order) {
    try {
        auto& semManager = m_context.getSemaphoreManager();
        std::string semaphoreName = semManager.getSemaphoreName(m_semaphoreHandle);

        semManager.notify(m_semaphoreHandle, m_notifyCount);
        SPDLOG_DEBUG("NotifyStage signaled semaphore '{}' (handle {}) by {} for entity {}",
            semaphoreName, m_semaphoreHandle.id, m_notifyCount, order->entity.id);
        return ProcessingResult::Success;
    }
    catch (const std::exception& e) {
        auto& semManager = m_context.getSemaphoreManager();
        std::string semaphoreName = semManager.getSemaphoreName(m_semaphoreHandle);

        SPDLOG_ERROR("NotifyStage failed on semaphore '{}' (handle {}) for entity {}: {}",
            semaphoreName, m_semaphoreHandle.id, order->entity.id, e.what());
        return ProcessingResult::Failure;
    }
}

// TryWaitStage Implementation
TryWaitStage::TryWaitStage(ProcessingContext& context, SemaphoreHandle semaphoreHandle, int waitCount)
    : ProcessingStage(context), m_semaphoreHandle(semaphoreHandle), m_waitCount(waitCount) {

    auto& semManager = m_context.getSemaphoreManager();
    std::string semaphoreName = semManager.getSemaphoreName(semaphoreHandle);

    SPDLOG_DEBUG("Created TryWaitStage for semaphore '{}' (handle {}) with count {}",
        semaphoreName, semaphoreHandle.id, waitCount);
}

ProcessingResult TryWaitStage::process(std::shared_ptr<RenderOrder> order) {
    try {
        auto& semManager = m_context.getSemaphoreManager();
        std::string semaphoreName = semManager.getSemaphoreName(m_semaphoreHandle);

        m_lastWaitSuccessful = semManager.tryWait(m_semaphoreHandle, m_waitCount);
        SPDLOG_DEBUG("TryWaitStage on semaphore '{}' (handle {}) for entity {}: {}",
            semaphoreName, m_semaphoreHandle.id, order->entity.id,
            m_lastWaitSuccessful ? "successful" : "failed");

        // TryWaitStage zawsze zwraca sukces - informacja o rezultacie wait'a 
        // jest dostępna przez wasLastWaitSuccessful()
        return ProcessingResult::Success;
    }
    catch (const std::exception& e) {
        auto& semManager = m_context.getSemaphoreManager();
        std::string semaphoreName = semManager.getSemaphoreName(m_semaphoreHandle);

        SPDLOG_ERROR("TryWaitStage exception on semaphore '{}' (handle {}) for entity {}: {}",
            semaphoreName, m_semaphoreHandle.id, order->entity.id, e.what());
        m_lastWaitSuccessful = false;
        return ProcessingResult::Failure;
    }
}
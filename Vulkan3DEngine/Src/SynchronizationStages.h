#pragma once
#include "ProcessingStage.h"
#include "SemaphoreManager.h"
#include <string>

// Stage that waits for a semaphore before proceeding
class WaitForStage : public ProcessingStage {
public:
    WaitForStage(ProcessingContext& context, SemaphoreHandle semaphoreHandle, int waitCount = 1);

    ProcessingResult process(std::shared_ptr<RenderOrder> order) override;

    // Get semaphore handle for debugging/info
    SemaphoreHandle getSemaphoreHandle() const { return m_semaphoreHandle; }

private:
    SemaphoreHandle m_semaphoreHandle;
    int m_waitCount;
};

// Stage that signals a semaphore after processing
class NotifyStage : public ProcessingStage {
public:
    NotifyStage(ProcessingContext& context, SemaphoreHandle semaphoreHandle, int notifyCount = 1);

    ProcessingResult process(std::shared_ptr<RenderOrder> order) override;

    // Get semaphore handle for debugging/info
    SemaphoreHandle getSemaphoreHandle() const { return m_semaphoreHandle; }

private:
    SemaphoreHandle m_semaphoreHandle;
    int m_notifyCount;
};

// Stage that tries to wait without blocking - useful for conditional processing
class TryWaitStage : public ProcessingStage {
public:
    TryWaitStage(ProcessingContext& context, SemaphoreHandle semaphoreHandle, int waitCount = 1);

    ProcessingResult process(std::shared_ptr<RenderOrder> order) override;

    // Check if last try wait was successful
    bool wasLastWaitSuccessful() const { return m_lastWaitSuccessful; }

    // Get semaphore handle for debugging/info
    SemaphoreHandle getSemaphoreHandle() const { return m_semaphoreHandle; }

private:
    SemaphoreHandle m_semaphoreHandle;
    int m_waitCount;
    bool m_lastWaitSuccessful = false;
};
#include "ProcessingPipeline.h"
#include "SynchronizationStages.h"
#include <spdlog/spdlog.h>
#include <algorithm>

ProcessingPipeline::ProcessingPipeline(const std::string& name, RenderOrderType expectedType, ProcessingContext& context)
    : m_name(name), m_expectedType(expectedType), m_context(context) {
    SPDLOG_DEBUG("Created processing pipeline '{}' for type: {}", name, renderOrderTypeToString(expectedType));
}

void ProcessingPipeline::addStage(std::shared_ptr<ProcessingStage> stage) {
    if (!stage) {
        SPDLOG_ERROR("Attempted to add null stage to pipeline '{}'", m_name);
        return;
    }

    m_stages.push_back(stage);

    // Check if this stage uses semaphores and register them automatically
    // For synchronization stages, we can extract their semaphore handles
    if (auto waitStage = std::dynamic_pointer_cast<WaitForStage>(stage)) {
        registerSemaphore(waitStage->getSemaphoreHandle());
        SPDLOG_DEBUG("Pipeline '{}' auto-registered WaitForStage semaphore (handle {})",
            m_name, waitStage->getSemaphoreHandle().id);
    }
    else if (auto notifyStage = std::dynamic_pointer_cast<NotifyStage>(stage)) {
        registerSemaphore(notifyStage->getSemaphoreHandle());
        SPDLOG_DEBUG("Pipeline '{}' auto-registered NotifyStage semaphore (handle {})",
            m_name, notifyStage->getSemaphoreHandle().id);
    }
    else if (auto tryWaitStage = std::dynamic_pointer_cast<TryWaitStage>(stage)) {
        registerSemaphore(tryWaitStage->getSemaphoreHandle());
        SPDLOG_DEBUG("Pipeline '{}' auto-registered TryWaitStage semaphore (handle {})",
            m_name, tryWaitStage->getSemaphoreHandle().id);
    }

    SPDLOG_DEBUG("Added stage to pipeline '{}', total stages: {}", m_name, m_stages.size());
}

void ProcessingPipeline::registerSemaphore(SemaphoreHandle handle) {
    if (!handle.isValid()) {
        SPDLOG_WARN("Attempted to register invalid semaphore handle in pipeline '{}'", m_name);
        return;
    }

    if (m_registeredSemaphores.insert(handle).second) {
        SPDLOG_DEBUG("Pipeline '{}' registered semaphore '{}' (handle {})",
            m_name, getSemaphoreManager().getSemaphoreName(handle), handle.id);
    }
    else {
        SPDLOG_DEBUG("Pipeline '{}' semaphore '{}' (handle {}) already registered",
            m_name, getSemaphoreManager().getSemaphoreName(handle), handle.id);
    }
}

void ProcessingPipeline::unregisterSemaphore(SemaphoreHandle handle) {
    if (m_registeredSemaphores.erase(handle) > 0) {
        SPDLOG_DEBUG("Pipeline '{}' unregistered semaphore '{}' (handle {})",
            m_name, getSemaphoreManager().getSemaphoreName(handle), handle.id);
    }
}

void ProcessingPipeline::resetAllSemaphores() {
    auto& semManager = getSemaphoreManager();

    for (const auto& handle : m_registeredSemaphores) {
        if (semManager.isValid(handle)) {
            semManager.setValue(handle, 0);
            SPDLOG_DEBUG("Pipeline '{}' reset semaphore '{}' (handle {}) to 0",
                m_name, semManager.getSemaphoreName(handle), handle.id);
        }
        else {
            SPDLOG_WARN("Pipeline '{}' tried to reset invalid semaphore handle {}",
                m_name, handle.id);
        }
    }

    SPDLOG_DEBUG("Pipeline '{}' reset all {} registered semaphores", m_name, m_registeredSemaphores.size());
}

void ProcessingPipeline::signalAllSemaphores() {
    auto& semManager = getSemaphoreManager();

    for (const auto& handle : m_registeredSemaphores) {
        if (semManager.isValid(handle)) {
            semManager.notify(handle, 1);
            SPDLOG_DEBUG("Pipeline '{}' signaled semaphore '{}' (handle {})",
                m_name, semManager.getSemaphoreName(handle), handle.id);
        }
        else {
            SPDLOG_WARN("Pipeline '{}' tried to signal invalid semaphore handle {}",
                m_name, handle.id);
        }
    }

    SPDLOG_DEBUG("Pipeline '{}' signaled all {} registered semaphores", m_name, m_registeredSemaphores.size());
}

void ProcessingPipeline::resetSemaphore(SemaphoreHandle handle, int value) {
    auto& semManager = getSemaphoreManager();

    if (m_registeredSemaphores.count(handle) == 0) {
        SPDLOG_WARN("Pipeline '{}' tried to reset unregistered semaphore handle {}",
            m_name, handle.id);
        return;
    }

    if (semManager.isValid(handle)) {
        semManager.setValue(handle, value);
        SPDLOG_DEBUG("Pipeline '{}' reset semaphore '{}' (handle {}) to {}",
            m_name, semManager.getSemaphoreName(handle), handle.id, value);
    }
    else {
        SPDLOG_WARN("Pipeline '{}' tried to reset invalid semaphore handle {}",
            m_name, handle.id);
    }
}

void ProcessingPipeline::signalSemaphore(SemaphoreHandle handle, int count) {
    auto& semManager = getSemaphoreManager();

    if (m_registeredSemaphores.count(handle) == 0) {
        SPDLOG_WARN("Pipeline '{}' tried to signal unregistered semaphore handle {}",
            m_name, handle.id);
        return;
    }

    if (semManager.isValid(handle)) {
        semManager.notify(handle, count);
        SPDLOG_DEBUG("Pipeline '{}' signaled semaphore '{}' (handle {}) by {}",
            m_name, semManager.getSemaphoreName(handle), handle.id, count);
    }
    else {
        SPDLOG_WARN("Pipeline '{}' tried to signal invalid semaphore handle {}",
            m_name, handle.id);
    }
}

void ProcessingPipeline::emergencyReset() {
    SPDLOG_WARN("Pipeline '{}' performing emergency reset", m_name);
    resetAllSemaphores();
    // Additional emergency cleanup can be added here if needed
}

std::vector<SemaphoreHandle> ProcessingPipeline::getRegisteredSemaphores() const {
    return std::vector<SemaphoreHandle>(m_registeredSemaphores.begin(), m_registeredSemaphores.end());
}

void ProcessingPipeline::logSemaphoreStatus() const {
    const auto& semManager = getSemaphoreManager();

    SPDLOG_DEBUG("Pipeline '{}' semaphore status ({} registered):", m_name, m_registeredSemaphores.size());
    for (const auto& handle : m_registeredSemaphores) {
        if (semManager.isValid(handle)) {
            int value = semManager.getValue(handle);
            std::string name = semManager.getSemaphoreName(handle);
            SPDLOG_DEBUG("  Semaphore '{}' (handle {}): value = {}", name, handle.id, value);
        }
        else {
            SPDLOG_DEBUG("  Semaphore handle {}: INVALID", handle.id);
        }
    }
}

void ProcessingPipeline::execute(std::shared_ptr<RenderOrder> order) {
    if (!validateOrder(order)) {
        return;
    }

    OrderProgress progress(order);

    // Process through all stages
    while (progress.currentStage < m_stages.size() && !progress.isDone()) {
        ProcessingResult result = processOrderAtStage(progress);
        progress.updateProgress(result);

        // For single order execution, we don't retry blocked orders
        if (result == ProcessingResult::Blocked) {
            SPDLOG_DEBUG("Pipeline '{}' order blocked at stage {} for entity {} - stopping single order execution",
                m_name, progress.currentStage, order->entity.id);
            return;
        }

        if (result == ProcessingResult::Failure) {
            SPDLOG_WARN("Pipeline '{}' order failed at stage {} for entity {} (type: {})",
                m_name, progress.currentStage, order->entity.id, renderOrderTypeToString(order->getType()));
            return;
        }
    }

    progress.checkCompletion(m_stages.size());

    if (progress.isComplete && progress.lastResult == ProcessingResult::Success) {
        SPDLOG_DEBUG("Pipeline '{}' completed processing order for entity: {}", m_name, order->entity.id);
    }
}

void ProcessingPipeline::executeBatch(const std::vector<std::shared_ptr<RenderOrder>>& orders) {
    if (orders.empty()) {
        SPDLOG_DEBUG("Pipeline '{}' has no orders to process", m_name);
        // If no orders, we might need to signal completion semaphores anyway
        // This handles the case where other pipelines are waiting for this one
        // but this pipeline has no work to do
        return;
    }

    // Create progress tracking for all valid orders
    std::vector<OrderProgress> orderProgress;
    orderProgress.reserve(orders.size());

    for (const auto& order : orders) {
        if (validateOrder(order)) {
            orderProgress.emplace_back(order);
        }
    }

    if (orderProgress.empty()) {
        SPDLOG_DEBUG("Pipeline '{}' has no valid orders to process", m_name);
        return;
    }

    const size_t totalOrders = orderProgress.size();
    SPDLOG_DEBUG("Pipeline '{}' starting batch processing with {} valid orders", m_name, totalOrders);

    int iteration = 0;
    bool anyProgress = false;
    const int maxIterations = 1000; // Safety limit

    // Process until all orders are completed or failed
    do {
        iteration++;
        anyProgress = false;

        SPDLOG_DEBUG("Pipeline '{}' processing iteration {}", m_name, iteration);

        // Process each order that can still be processed
        for (auto& progress : orderProgress) {
            if (progress.isDone()) {
                continue; // Skip completed or failed orders
            }

            // Check if order has more stages to process
            if (progress.currentStage >= m_stages.size()) {
                progress.checkCompletion(m_stages.size());
                if (progress.isComplete) {
                    anyProgress = true;
                    SPDLOG_DEBUG("Pipeline '{}' completed order for entity: {}",
                        m_name, progress.order->entity.id);
                }
                continue;
            }

            // Process current stage
            ProcessingResult result = processOrderAtStage(progress);
            ProcessingResult oldResult = progress.lastResult;
            progress.updateProgress(result);

            // Check if we made progress (state changed or advanced stage)
            if (result != ProcessingResult::Blocked || oldResult == ProcessingResult::Blocked) {
                anyProgress = true;
            }

            if (result == ProcessingResult::Success) {
                SPDLOG_DEBUG("Pipeline '{}' entity {} advanced to stage {}",
                    m_name, progress.order->entity.id, progress.currentStage);
            }
            else if (result == ProcessingResult::Failure) {
                SPDLOG_WARN("Pipeline '{}' stage {} failed for entity {} (type: {})",
                    m_name, progress.currentStage - 1, progress.order->entity.id,
                    renderOrderTypeToString(progress.order->getType()));
            }
            else if (result == ProcessingResult::Blocked) {
                SPDLOG_DEBUG("Pipeline '{}' entity {} blocked at stage {} - will retry",
                    m_name, progress.order->entity.id, progress.currentStage);
            }
        }

        // Log progress every few iterations for debugging
        if (iteration % 10 == 0 || iteration == 1) {
            logProgress(iteration, orderProgress);
        }

        // Check if all orders are done
        bool allDone = true;
        for (const auto& progress : orderProgress) {
            if (!progress.isDone()) {
                allDone = false;
                break;
            }
        }

        if (allDone) {
            break;
        }

        // Safety check for infinite loops
        if (iteration >= maxIterations) {
            SPDLOG_ERROR("Pipeline '{}' exceeded maximum iterations ({}), performing emergency reset",
                m_name, maxIterations);
            emergencyReset();
            break;
        }

        // If no progress was made, log it but continue
        if (!anyProgress) {
            SPDLOG_DEBUG("Pipeline '{}' iteration {} made no progress - all remaining orders blocked",
                m_name, iteration);
        }

    } while (true);

    // Final statistics
    size_t completedCount = 0;
    size_t failedCount = 0;
    size_t blockedCount = 0;

    for (const auto& progress : orderProgress) {
        if (progress.isComplete && progress.lastResult == ProcessingResult::Success) {
            completedCount++;
        }
        else if (progress.lastResult == ProcessingResult::Failure) {
            failedCount++;
        }
        else {
            blockedCount++;
        }
    }

    SPDLOG_DEBUG("Pipeline '{}' completed batch processing after {} iterations: {} successful, {} failed, {} blocked",
        m_name, iteration, completedCount, failedCount, blockedCount);
}

bool ProcessingPipeline::validateOrder(std::shared_ptr<RenderOrder> order) const {
    if (!order) {
        SPDLOG_WARN("Pipeline '{}' received null render order", m_name);
        return false;
    }

    if (order->getType() != m_expectedType) {
        SPDLOG_WARN("Pipeline '{}' expected type {}, got type {} for entity {} - discarding order",
            m_name,
            renderOrderTypeToString(m_expectedType),
            renderOrderTypeToString(order->getType()),
            order->entity.id);
        return false;
    }

    return true;
}

ProcessingResult ProcessingPipeline::processOrderAtStage(OrderProgress& progress) {
    try {
        return m_stages[progress.currentStage]->process(progress.order);
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Exception in pipeline '{}' stage {} for entity {}: {}",
            m_name, progress.currentStage, progress.order->entity.id, e.what());
        SPDLOG_WARN("Pipeline '{}' marking order as failed due to exception", m_name);
        return ProcessingResult::Failure;
    }
}

void ProcessingPipeline::logProgress(int iteration, const std::vector<OrderProgress>& orderProgress) const {
    size_t completedCount = 0;
    size_t failedCount = 0;
    size_t blockedCount = 0;
    size_t processingCount = 0;

    for (const auto& progress : orderProgress) {
        if (progress.isComplete && progress.lastResult == ProcessingResult::Success) {
            completedCount++;
        }
        else if (progress.lastResult == ProcessingResult::Failure) {
            failedCount++;
        }
        else if (progress.lastResult == ProcessingResult::Blocked) {
            blockedCount++;
        }
        else {
            processingCount++;
        }
    }

    SPDLOG_DEBUG("Pipeline '{}' iteration {} status: {} completed, {} failed, {} blocked, {} processing",
        m_name, iteration, completedCount, failedCount, blockedCount, processingCount);
}

SemaphoreManager& ProcessingPipeline::getSemaphoreManager() {
    return m_context.getSemaphoreManager();
}

const SemaphoreManager& ProcessingPipeline::getSemaphoreManager() const {
    return m_context.getSemaphoreManager();
}
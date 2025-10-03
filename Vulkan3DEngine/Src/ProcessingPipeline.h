#pragma once
#include <memory>
#include <vector>
#include <string>
#include <unordered_set>
#include "ProcessingStage.h"
#include "RenderOrder.h"
#include "ProcessingContext.h"
#include "SemaphoreManager.h"

// Simplified structure to track order progress through pipeline
struct OrderProgress {
    std::shared_ptr<RenderOrder> order;
    size_t currentStage = 0;
    ProcessingResult lastResult = ProcessingResult::Success;
    bool isComplete = false;

    OrderProgress(std::shared_ptr<RenderOrder> o) : order(std::move(o)) {}

    // Check if order is done (completed successfully or failed)
    bool isDone() const {
        return isComplete || lastResult == ProcessingResult::Failure;
    }

    // Check if order can be processed (not blocked and not done)
    bool canProcess() const {
        return !isDone() && lastResult != ProcessingResult::Blocked;
    }

    // Update progress based on processing result
    void updateProgress(ProcessingResult result) {
        lastResult = result;
        if (result == ProcessingResult::Success) {
            currentStage++;
        }
        else if (result == ProcessingResult::Failure) {
            isComplete = true; // Mark as complete but failed
        }
        // For Blocked, just update lastResult, don't advance stage
    }

    // Check if all stages completed successfully
    void checkCompletion(size_t totalStages) {
        if (currentStage >= totalStages && lastResult == ProcessingResult::Success) {
            isComplete = true;
        }
    }
};

class ProcessingPipeline {
public:
    ProcessingPipeline(const std::string& name, RenderOrderType expectedType, ProcessingContext& context);
    ~ProcessingPipeline() = default;

    // Non-copyable, non-movable  
    ProcessingPipeline(const ProcessingPipeline&) = delete;
    ProcessingPipeline& operator=(const ProcessingPipeline&) = delete;
    ProcessingPipeline(ProcessingPipeline&&) = delete;
    ProcessingPipeline& operator=(ProcessingPipeline&&) = delete;

    // Pipeline configuration - compile-time static setup
    void addStage(std::shared_ptr<ProcessingStage> stage);

    // Pipeline execution
    void execute(std::shared_ptr<RenderOrder> order);
    void executeBatch(const std::vector<std::shared_ptr<RenderOrder>>& orders);

    // Semaphore registration - stages register their semaphores with the pipeline
    void registerSemaphore(SemaphoreHandle handle);
    void unregisterSemaphore(SemaphoreHandle handle);

    // Pipeline semaphore management
    void resetAllSemaphores();
    void signalAllSemaphores();
    void resetSemaphore(SemaphoreHandle handle, int value = 0);
    void signalSemaphore(SemaphoreHandle handle, int count = 1);

    // Emergency pipeline reset - resets all semaphores and clears any state
    void emergencyReset();

    // Pipeline information
    const std::string& getName() const { return m_name; }
    RenderOrderType getExpectedType() const { return m_expectedType; }
    size_t getStageCount() const { return m_stages.size(); }
    size_t getSemaphoreCount() const { return m_registeredSemaphores.size(); }

    // Debug information
    std::vector<SemaphoreHandle> getRegisteredSemaphores() const;
    void logSemaphoreStatus() const;

private:
    std::string m_name;
    RenderOrderType m_expectedType;
    ProcessingContext& m_context;
    std::vector<std::shared_ptr<ProcessingStage>> m_stages;

    // Semaphore tracking - all semaphores used by this pipeline's stages
    std::unordered_set<SemaphoreHandle> m_registeredSemaphores;

    // Validation and processing helpers
    bool validateOrder(std::shared_ptr<RenderOrder> order) const;
    ProcessingResult processOrderAtStage(OrderProgress& progress);
    void logProgress(int iteration, const std::vector<OrderProgress>& orderProgress) const;

    // Helper to access semaphore manager
    SemaphoreManager& getSemaphoreManager();
    const SemaphoreManager& getSemaphoreManager() const;
};
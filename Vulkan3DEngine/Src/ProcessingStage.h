#pragma once
#include <memory>
#include <vector>
#include "RenderOrder.h"
#include "ProcessingContext.h"

// Result of processing a render order through a stage
enum class ProcessingResult {
    Success,    // Order processed successfully, continue to next stage
    Failure,    // Order failed processing, discard completely
    Blocked     // Order is blocked waiting for conditions, retry later
};

// Convert ProcessingResult to string for logging
inline std::string processingResultToString(ProcessingResult result) {
    switch (result) {
    case ProcessingResult::Success: return "Success";
    case ProcessingResult::Failure: return "Failure";
    case ProcessingResult::Blocked: return "Blocked";
    default: return "Unknown";
    }
}

class ProcessingStage {
public:
    ProcessingStage(ProcessingContext& context) : m_context(context) {}
    virtual ~ProcessingStage() = default;

    // Non-copyable, non-movable
    ProcessingStage(const ProcessingStage&) = delete;
    ProcessingStage& operator=(const ProcessingStage&) = delete;
    ProcessingStage(ProcessingStage&&) = delete;
    ProcessingStage& operator=(ProcessingStage&&) = delete;

    // Main processing method - each stage implements its own logic
    // Returns ProcessingResult indicating success, failure, or blocked state
    virtual ProcessingResult process(std::shared_ptr<RenderOrder> order) = 0;
protected:
    ProcessingContext& m_context;
};
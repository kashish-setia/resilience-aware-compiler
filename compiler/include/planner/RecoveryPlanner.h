#pragma once

#include "classifier/StateClassifier.h"
#include "analysis/DependencyGraph.h"
#include <string>
#include <vector>
#include <set>
#include <memory>

namespace edge {

enum class CheckpointStrategy {
    CHECKPOINT_EVERYTHING,
    LIVENESS_ONLY,
    MANUAL_ANNOTATION,
    RESILIENCE_AWARE
};

std::string checkpointStrategyToString(CheckpointStrategy strategy);
CheckpointStrategy stringToCheckpointStrategy(const std::string& str);

struct CheckpointPlan {
    size_t checkpointId{0};
    CheckpointStrategy strategy{CheckpointStrategy::RESILIENCE_AWARE};
    size_t memoryBudgetBytes{0}; // 0 = unlimited
    bool isFeasible{true};
    std::string infeasibilityReason;

    std::vector<std::string> savedVariables;
    size_t totalSavedBytes{0};

    std::vector<std::string> recomputedVariables;
    std::vector<std::string> recomputeOrder; // topologically ordered
    size_t totalRecomputedBytes{0};

    std::vector<std::string> discardedVariables;
    std::vector<std::string> optionalVariablesExcluded;

    size_t baselineEverythingBytes{0};
    size_t bytesSaved{0};
    double savingsPercent{0.0};

    std::vector<std::string> decisionLog;

    std::string toJson() const;
};

class RecoveryPlanner {
private:
    const DependencyGraph& depGraph_;

public:
    explicit RecoveryPlanner(const DependencyGraph& depGraph);

    CheckpointPlan plan(
        size_t checkpointId,
        const std::vector<VariableClassification>& classifications,
        CheckpointStrategy strategy = CheckpointStrategy::RESILIENCE_AWARE,
        size_t memoryBudgetBytes = 0
    ) const;
};

} // namespace edge

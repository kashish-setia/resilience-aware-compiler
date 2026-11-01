#pragma once

#include "ir/Bytecode.h"
#include "runtime/VirtualMachine.h"
#include "runtime/CheckpointStore.h"
#include "planner/RecoveryPlanner.h"
#include "analysis/DependencyGraph.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>

namespace edge {

struct SimulationLogEvent {
    std::string phase; // "INIT", "FORWARD", "CHECKPOINT", "FAULT", "RESTORE", "RECOMPUTE", "RESUME", "COMPLETE"
    std::string message;
    size_t cycle;
};

struct SimulationResult {
    CheckpointStrategy strategy{CheckpointStrategy::RESILIENCE_AWARE};
    bool isCorrect{false};
    std::string failurePointDescription;

    std::vector<std::string> referenceOutputs;
    std::vector<std::string> recoveredOutputs;

    std::unordered_map<std::string, Value> referenceFinalVars;
    std::unordered_map<std::string, Value> recoveredFinalVars;

    size_t checkpointSizeBytes{0};
    size_t baselineSizeBytes{0};
    size_t savedVarCount{0};
    size_t recomputedVarCount{0};
    double recoveryTimeMs{0.0};
    size_t totalCycles{0};

    std::vector<SimulationLogEvent> events;

    std::string toJson() const;
};

class FailureSimulator {
private:
    const BytecodeProgram& program_;
    const DependencyGraph& depGraph_;

    Value evaluateExpr(const Expr* expr, const std::unordered_map<std::string, Value>& currentVars) const;

public:
    FailureSimulator(const BytecodeProgram& program, const DependencyGraph& depGraph);

    SimulationResult run(
        const CheckpointPlan& plan,
        int32_t faultTriggerCycle = -1,
        size_t faultStatementId = 0
    );
};

} // namespace edge

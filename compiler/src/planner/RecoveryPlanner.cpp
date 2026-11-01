#include "planner/RecoveryPlanner.h"
#include <sstream>
#include <iomanip>

namespace edge {

std::string checkpointStrategyToString(CheckpointStrategy strategy) {
    switch (strategy) {
        case CheckpointStrategy::CHECKPOINT_EVERYTHING: return "CHECKPOINT_EVERYTHING";
        case CheckpointStrategy::LIVENESS_ONLY: return "LIVENESS_ONLY";
        case CheckpointStrategy::MANUAL_ANNOTATION: return "MANUAL_ANNOTATION";
        case CheckpointStrategy::RESILIENCE_AWARE: return "RESILIENCE_AWARE";
    }
    return "UNKNOWN";
}

CheckpointStrategy stringToCheckpointStrategy(const std::string& str) {
    if (str == "CHECKPOINT_EVERYTHING") return CheckpointStrategy::CHECKPOINT_EVERYTHING;
    if (str == "LIVENESS_ONLY") return CheckpointStrategy::LIVENESS_ONLY;
    if (str == "MANUAL_ANNOTATION") return CheckpointStrategy::MANUAL_ANNOTATION;
    if (str == "RESILIENCE_AWARE") return CheckpointStrategy::RESILIENCE_AWARE;
    return CheckpointStrategy::RESILIENCE_AWARE;
}

static std::string escapeJson(const std::string& s) {
    std::string res;
    for (char c : s) {
        if (c == '"') res += "\\\"";
        else if (c == '\\') res += "\\\\";
        else if (c == '\n') res += "\\n";
        else if (c == '\r') res += "\\r";
        else if (c == '\t') res += "\\t";
        else res += c;
    }
    return res;
}

std::string CheckpointPlan::toJson() const {
    std::ostringstream oss;
    oss << "{"
        << "\"checkpointId\":" << checkpointId << ","
        << "\"strategy\":\"" << checkpointStrategyToString(strategy) << "\","
        << "\"memoryBudgetBytes\":" << memoryBudgetBytes << ","
        << "\"isFeasible\":" << (isFeasible ? "true" : "false") << ","
        << "\"infeasibilityReason\":\"" << escapeJson(infeasibilityReason) << "\","
        << "\"totalSavedBytes\":" << totalSavedBytes << ","
        << "\"baselineEverythingBytes\":" << baselineEverythingBytes << ","
        << "\"bytesSaved\":" << bytesSaved << ","
        << "\"savingsPercent\":" << std::fixed << std::setprecision(1) << savingsPercent << ","
        << "\"savedVariables\":[";
    for (size_t i = 0; i < savedVariables.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "\"" << escapeJson(savedVariables[i]) << "\"";
    }
    oss << "],\"recomputedVariables\":[";
    for (size_t i = 0; i < recomputedVariables.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "\"" << escapeJson(recomputedVariables[i]) << "\"";
    }
    oss << "],\"recomputeOrder\":[";
    for (size_t i = 0; i < recomputeOrder.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "\"" << escapeJson(recomputeOrder[i]) << "\"";
    }
    oss << "],\"discardedVariables\":[";
    for (size_t i = 0; i < discardedVariables.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "\"" << escapeJson(discardedVariables[i]) << "\"";
    }
    oss << "],\"optionalVariablesExcluded\":[";
    for (size_t i = 0; i < optionalVariablesExcluded.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "\"" << escapeJson(optionalVariablesExcluded[i]) << "\"";
    }
    oss << "],\"decisionLog\":[";
    for (size_t i = 0; i < decisionLog.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "\"" << escapeJson(decisionLog[i]) << "\"";
    }
    oss << "]}";
    return oss.str();
}

RecoveryPlanner::RecoveryPlanner(const DependencyGraph& depGraph)
    : depGraph_(depGraph) {}

CheckpointPlan RecoveryPlanner::plan(
    size_t checkpointId,
    const std::vector<VariableClassification>& classifications,
    CheckpointStrategy strategy,
    size_t memoryBudgetBytes
) const {
    CheckpointPlan p;
    p.checkpointId = checkpointId;
    p.strategy = strategy;
    p.memoryBudgetBytes = memoryBudgetBytes;
    p.isFeasible = true;

    // Calculate baseline storage (saving all known variables)
    for (const auto& vc : classifications) {
        p.baselineEverythingBytes += vc.storageSizeBytes;
    }

    std::set<std::string> toRecomputeSet;

    if (strategy == CheckpointStrategy::CHECKPOINT_EVERYTHING) {
        p.decisionLog.push_back("Strategy CHECKPOINT_EVERYTHING: Saving all variables defined in program.");
        for (const auto& vc : classifications) {
            p.savedVariables.push_back(vc.name);
            p.totalSavedBytes += vc.storageSizeBytes;
        }
    } else if (strategy == CheckpointStrategy::LIVENESS_ONLY) {
        p.decisionLog.push_back("Strategy LIVENESS_ONLY: Saving all live variables regardless of recomputability.");
        for (const auto& vc : classifications) {
            if (vc.classification != StateClass::DISPOSABLE) {
                p.savedVariables.push_back(vc.name);
                p.totalSavedBytes += vc.storageSizeBytes;
            } else {
                p.discardedVariables.push_back(vc.name);
            }
        }
    } else if (strategy == CheckpointStrategy::MANUAL_ANNOTATION) {
        p.decisionLog.push_back("Strategy MANUAL_ANNOTATION: Saving variables marked with @critical and recomputing live dependencies.");

        // 1. Identify explicitly annotated @critical variables
        std::set<std::string> savedSet;
        for (const auto& vc : classifications) {
            if (vc.annotation == AnnotationKind::CRITICAL) {
                p.savedVariables.push_back(vc.name);
                p.totalSavedBytes += vc.storageSizeBytes;
                savedSet.insert(vc.name);
                p.decisionLog.push_back("Preserved manually annotated @critical variable '" + vc.name + "' (" + 
                                       std::to_string(vc.storageSizeBytes) + " bytes).");
            }
        }

        // 2. Classify and schedule remaining live variables for recomputation or safety preservation
        for (const auto& vc : classifications) {
            if (vc.annotation == AnnotationKind::CRITICAL) {
                continue; // Already saved
            }

            if (vc.classification == StateClass::DISPOSABLE) {
                p.discardedVariables.push_back(vc.name);
                p.decisionLog.push_back("Discarded DISPOSABLE variable '" + vc.name + "'.");
                continue;
            }

            // Check if this live variable can be safely reconstructed from saved inputs
            std::set<std::string> visited;
            if (depGraph_.canRecompute(vc.name, savedSet, visited)) {
                p.recomputedVariables.push_back(vc.name);
                toRecomputeSet.insert(vc.name);
                p.totalRecomputedBytes += vc.storageSizeBytes;
                p.decisionLog.push_back("Scheduled recomputation for live variable '" + vc.name + "' from manual dependencies.");
            } else if (vc.classification == StateClass::OPTIONAL) {
                p.optionalVariablesExcluded.push_back(vc.name);
                p.discardedVariables.push_back(vc.name);
                p.decisionLog.push_back("Excluded OPTIONAL variable '" + vc.name + "' under manual annotation.");
            } else {
                // Live variable that cannot be reconstructed from dependencies: preserve in NVM to ensure recovery correctness
                p.savedVariables.push_back(vc.name);
                p.totalSavedBytes += vc.storageSizeBytes;
                savedSet.insert(vc.name);
                p.decisionLog.push_back("Preserved unrecomputable live variable '" + vc.name + "' (" + 
                                       std::to_string(vc.storageSizeBytes) + " bytes) to guarantee recovery correctness.");
            }
        }

        // 3. Collect transitive dependency closure and build topological recomputation order
        std::set<std::string> fullRecomputeSet = toRecomputeSet;
        std::set<std::string> visitedClosure;
        for (const auto& var : toRecomputeSet) {
            depGraph_.collectTransitiveDependencies(var, savedSet, fullRecomputeSet, visitedClosure);
        }

        p.recomputeOrder = depGraph_.getTopologicalRecomputeOrder(fullRecomputeSet);
    } else { // RESILIENCE_AWARE
        p.decisionLog.push_back("Strategy RESILIENCE_AWARE: Optimal selection via 4-way classification.");

        size_t criticalBytes = 0;
        for (const auto& vc : classifications) {
            if (vc.classification == StateClass::CRITICAL) {
                p.savedVariables.push_back(vc.name);
                p.totalSavedBytes += vc.storageSizeBytes;
                criticalBytes += vc.storageSizeBytes;
                p.decisionLog.push_back("Preserved CRITICAL variable '" + vc.name + "' (" + 
                                       std::to_string(vc.storageSizeBytes) + " bytes): " + vc.reason);
            } else if (vc.classification == StateClass::RECOMPUTABLE) {
                p.recomputedVariables.push_back(vc.name);
                toRecomputeSet.insert(vc.name);
                p.totalRecomputedBytes += vc.storageSizeBytes;
                p.decisionLog.push_back("Scheduled RECOMPUTABLE variable '" + vc.name + "' for recovery recomputation.");
            } else if (vc.classification == StateClass::DISPOSABLE) {
                p.discardedVariables.push_back(vc.name);
                p.decisionLog.push_back("Discarded DISPOSABLE variable '" + vc.name + "': dead or volatile debug state.");
            } else if (vc.classification == StateClass::OPTIONAL) {
                // Check if optional fits in remaining budget
                size_t remainingBudget = (memoryBudgetBytes > 0 && memoryBudgetBytes >= p.totalSavedBytes) ? 
                                         (memoryBudgetBytes - p.totalSavedBytes) : 0;
                if (memoryBudgetBytes == 0 || vc.storageSizeBytes <= remainingBudget) {
                    p.savedVariables.push_back(vc.name);
                    p.totalSavedBytes += vc.storageSizeBytes;
                    p.decisionLog.push_back("Included OPTIONAL variable '" + vc.name + "' within available storage budget.");
                } else {
                    p.optionalVariablesExcluded.push_back(vc.name);
                    p.discardedVariables.push_back(vc.name);
                    p.decisionLog.push_back("Excluded OPTIONAL variable '" + vc.name + "' due to tight storage budget constraint.");
                }
            }
        }

        // Budget feasibility check
        if (memoryBudgetBytes > 0 && criticalBytes > memoryBudgetBytes) {
            p.isFeasible = false;
            p.infeasibilityReason = "Critical variables require " + std::to_string(criticalBytes) + 
                                    " bytes, exceeding the configured budget of " + 
                                    std::to_string(memoryBudgetBytes) + " bytes.";
            p.decisionLog.push_back("FEASIBILITY ERROR: " + p.infeasibilityReason);
        }

        // Collect transitive dependency closure for all recomputable variables
        std::set<std::string> savedSet(p.savedVariables.begin(), p.savedVariables.end());
        std::set<std::string> fullRecomputeSet = toRecomputeSet;
        std::set<std::string> visited;
        for (const auto& var : toRecomputeSet) {
            depGraph_.collectTransitiveDependencies(var, savedSet, fullRecomputeSet, visited);
        }

        // Compute topological recomputation order
        p.recomputeOrder = depGraph_.getTopologicalRecomputeOrder(fullRecomputeSet);
    }

    if (p.baselineEverythingBytes > 0 && p.totalSavedBytes <= p.baselineEverythingBytes) {
        p.bytesSaved = p.baselineEverythingBytes - p.totalSavedBytes;
        p.savingsPercent = (static_cast<double>(p.bytesSaved) / p.baselineEverythingBytes) * 100.0;
    }

    return p;
}

} // namespace edge

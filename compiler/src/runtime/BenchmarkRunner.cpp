#include "runtime/BenchmarkRunner.h"
#include <sstream>
#include <iomanip>

namespace edge {

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

std::string StrategyComparison::toJson() const {
    std::ostringstream oss;
    oss << "{"
        << "\"strategyName\":\"" << escapeJson(strategyName) << "\","
        << "\"storageBytes\":" << storageBytes << ","
        << "\"recoveryTimeMs\":" << recoveryTimeMs << ","
        << "\"savedVarCount\":" << savedVarCount << ","
        << "\"recomputedVarCount\":" << recomputedVarCount << ","
        << "\"isCorrect\":" << (isCorrect ? "true" : "false") << ","
        << "\"savingsPercent\":" << std::fixed << std::setprecision(1) << savingsPercent << ","
        << "\"codeSizeInstructions\":" << codeSizeInstructions
        << "}";
    return oss.str();
}

std::string BenchmarkSuiteResult::toJson() const {
    std::ostringstream oss;
    oss << "{"
        << "\"benchmarkName\":\"" << escapeJson(benchmarkName) << "\","
        << "\"description\":\"" << escapeJson(description) << "\","
        << "\"compilationTimeMs\":" << compilationTimeMs << ","
        << "\"comparisons\":[";
    for (size_t i = 0; i < comparisons.size(); ++i) {
        if (i > 0) oss << ",";
        oss << comparisons[i].toJson();
    }
    oss << "]}";
    return oss.str();
}

BenchmarkSuiteResult BenchmarkRunner::runBenchmark(
    const std::string& name,
    const std::string& description,
    const Program& /*ast*/,
    const DependencyGraph& depGraph,
    const std::vector<VariableClassification>& classifications,
    const BytecodeProgram& bytecode,
    double compilationTimeMs
) {
    BenchmarkSuiteResult suite;
    suite.benchmarkName = name;
    suite.description = description;
    suite.compilationTimeMs = compilationTimeMs;

    RecoveryPlanner planner(depGraph);
    FailureSimulator simulator(bytecode, depGraph);

    std::vector<CheckpointStrategy> strategies = {
        CheckpointStrategy::CHECKPOINT_EVERYTHING,
        CheckpointStrategy::LIVENESS_ONLY,
        CheckpointStrategy::MANUAL_ANNOTATION,
        CheckpointStrategy::RESILIENCE_AWARE
    };

    size_t baselineStorage = 0;

    for (auto strat : strategies) {
        CheckpointPlan plan = planner.plan(1, classifications, strat, 0);
        SimulationResult simRes = simulator.run(plan);

        StrategyComparison comp;
        comp.strategyName = checkpointStrategyToString(strat);
        comp.storageBytes = plan.totalSavedBytes;
        comp.recoveryTimeMs = simRes.recoveryTimeMs;
        comp.savedVarCount = plan.savedVariables.size();
        comp.recomputedVarCount = plan.recomputeOrder.size();
        comp.isCorrect = simRes.isCorrect;
        comp.codeSizeInstructions = bytecode.size();

        if (strat == CheckpointStrategy::CHECKPOINT_EVERYTHING) {
            baselineStorage = comp.storageBytes;
            comp.savingsPercent = 0.0;
        } else if (baselineStorage > 0) {
            comp.savingsPercent = (static_cast<double>(baselineStorage - comp.storageBytes) / baselineStorage) * 100.0;
        }

        suite.comparisons.push_back(comp);
    }

    return suite;
}

} // namespace edge

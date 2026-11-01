#pragma once

#include "planner/RecoveryPlanner.h"
#include "runtime/FailureSimulator.h"
#include "ir/Bytecode.h"
#include "analysis/DependencyGraph.h"
#include <string>
#include <vector>

namespace edge {

struct StrategyComparison {
    std::string strategyName;
    size_t storageBytes{0};
    double recoveryTimeMs{0.0};
    size_t savedVarCount{0};
    size_t recomputedVarCount{0};
    bool isCorrect{false};
    double savingsPercent{0.0};
    size_t codeSizeInstructions{0};

    std::string toJson() const;
};

struct BenchmarkSuiteResult {
    std::string benchmarkName;
    std::string description;
    std::vector<StrategyComparison> comparisons;
    double compilationTimeMs{0.0};

    std::string toJson() const;
};

class BenchmarkRunner {
public:
    static BenchmarkSuiteResult runBenchmark(
        const std::string& name,
        const std::string& description,
        const Program& ast,
        const DependencyGraph& depGraph,
        const std::vector<VariableClassification>& classifications,
        const BytecodeProgram& bytecode,
        double compilationTimeMs = 0.0
    );

    static std::string runAllDefaultBenchmarks();
};

} // namespace edge

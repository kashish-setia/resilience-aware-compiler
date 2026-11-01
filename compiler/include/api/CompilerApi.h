#pragma once

#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "semantic/SemanticAnalyzer.h"
#include "cfg/CFGBuilder.h"
#include "analysis/LivenessAnalysis.h"
#include "analysis/DefUseAnalysis.h"
#include "analysis/DependencyGraph.h"
#include "classifier/StateClassifier.h"
#include "planner/RecoveryPlanner.h"
#include "ir/CodeGenerator.h"
#include "runtime/FailureSimulator.h"
#include "runtime/BenchmarkRunner.h"
#include <string>
#include <vector>
#include <memory>

namespace edge {

struct CompilationResult {
    bool success{false};
    std::string programName;
    double compilationTimeMs{0.0};

    std::string diagnosticsJson;
    std::string tokensJson;
    std::string astJson;
    std::string symbolTableJson;
    std::string cfgJson;
    std::string livenessJson;
    std::string defUseJson;
    std::string dependencyJson;
    std::string classificationsJson;
    std::string recoveryPlanJson;
    std::string bytecodeJson;
    std::string disassembled;

    std::string toJson() const;
};

struct ExampleProgram {
    std::string id;
    std::string title;
    std::string description;
    std::string category;
    std::string sourceCode;

    std::string toJson() const;
};

class CompilerApi {
public:
    static CompilationResult compile(
        const std::string& sourceCode,
        size_t memoryBudgetBytes = 0,
        const std::string& filename = "input.edge"
    );

    static std::string simulate(
        const std::string& sourceCode,
        CheckpointStrategy strategy = CheckpointStrategy::RESILIENCE_AWARE,
        size_t memoryBudgetBytes = 0,
        int32_t faultCycle = -1,
        size_t faultStatementId = 0
    );

    static std::string runBenchmarks(const std::string& sourceCode = "");

    static std::vector<ExampleProgram> getExamples();
};

} // namespace edge

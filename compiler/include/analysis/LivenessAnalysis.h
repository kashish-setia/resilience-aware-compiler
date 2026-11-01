#pragma once

#include "cfg/CFG.h"
#include "parser/AST.h"
#include <string>
#include <set>
#include <unordered_map>

namespace edge {

struct BlockLivenessFacts {
    size_t blockId;
    std::set<std::string> use;
    std::set<std::string> def;
    std::set<std::string> liveIn;
    std::set<std::string> liveOut;

    std::string toJson() const;
};

class LivenessAnalysis {
private:
    CFG& cfg_;
    size_t iterationCount_{0};

    void computeUseDef();
    void extractExprVariables(const Expr* expr, std::set<std::string>& reads);
    void analyzeStatementUseDef(const Stmt* stmt, std::set<std::string>& blockUse, std::set<std::string>& blockDef);

public:
    explicit LivenessAnalysis(CFG& cfg);

    void run();

    size_t getIterationCount() const { return iterationCount_; }
    std::set<std::string> getLiveVariablesAtCheckpoint(size_t checkpointId) const;
    std::set<std::string> getLiveVariablesAtBlock(size_t blockId) const;

    std::vector<BlockLivenessFacts> getAllFacts() const;
    std::string toJson() const;
};

} // namespace edge

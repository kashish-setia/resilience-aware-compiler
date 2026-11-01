#pragma once

#include "parser/AST.h"
#include <string>
#include <vector>
#include <set>
#include <unordered_map>
#include <memory>

namespace edge {

enum class CFGEdgeType {
    FALLTHROUGH,
    TRUE_BRANCH,
    FALSE_BRANCH,
    LOOP_BACK,
    LOOP_EXIT
};

std::string cfgEdgeTypeToString(CFGEdgeType type);

struct CFGEdge {
    size_t targetBlockId;
    CFGEdgeType type;

    CFGEdge(size_t target, CFGEdgeType t) : targetBlockId(target), type(t) {}
};

class BasicBlock {
public:
    size_t id{0};
    std::string label;
    std::vector<const Stmt*> statements;
    std::vector<size_t> predecessors;
    std::vector<CFGEdge> successors;

    // Data-flow sets for Liveness Analysis
    std::set<std::string> use;
    std::set<std::string> def;
    std::set<std::string> liveIn;
    std::set<std::string> liveOut;

    BasicBlock(size_t id, std::string label) : id(id), label(std::move(label)) {}

    void addStatement(const Stmt* stmt) {
        statements.push_back(stmt);
    }

    void addSuccessor(size_t targetId, CFGEdgeType type) {
        successors.emplace_back(targetId, type);
    }

    bool hasCheckpoint() const {
        for (const auto* s : statements) {
            if (dynamic_cast<const CheckpointStmt*>(s)) return true;
        }
        return false;
    }

    const CheckpointStmt* getCheckpointStmt() const {
        for (const auto* s : statements) {
            if (auto* c = dynamic_cast<const CheckpointStmt*>(s)) return c;
        }
        return nullptr;
    }

    std::string toString() const;
    std::string toJson() const;
};

class CFG {
private:
    std::vector<std::shared_ptr<BasicBlock>> blocks_;
    size_t entryBlockId_{0};
    size_t exitBlockId_{0};

public:
    CFG() = default;

    std::shared_ptr<BasicBlock> createBlock(const std::string& label);
    std::shared_ptr<BasicBlock> getBlock(size_t id) const;
    const std::vector<std::shared_ptr<BasicBlock>>& getBlocks() const { return blocks_; }

    void setEntryBlock(size_t id) { entryBlockId_ = id; }
    void setExitBlock(size_t id) { exitBlockId_ = id; }

    size_t getEntryBlockId() const { return entryBlockId_; }
    size_t getExitBlockId() const { return exitBlockId_; }

    void addEdge(size_t fromId, size_t toId, CFGEdgeType type);
    void finalizePredecessors();

    std::string toDot() const;
    std::string toJson() const;
};

} // namespace edge

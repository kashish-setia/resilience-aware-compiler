#include "analysis/LivenessAnalysis.h"
#include <sstream>
#include <algorithm>

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

std::string BlockLivenessFacts::toJson() const {
    std::ostringstream oss;
    oss << "{\"blockId\":" << blockId << ",\"use\":[";
    size_t idx = 0;
    for (const auto& s : use) {
        if (idx++ > 0) oss << ",";
        oss << "\"" << escapeJson(s) << "\"";
    }
    oss << "],\"def\":[";
    idx = 0;
    for (const auto& s : def) {
        if (idx++ > 0) oss << ",";
        oss << "\"" << escapeJson(s) << "\"";
    }
    oss << "],\"liveIn\":[";
    idx = 0;
    for (const auto& s : liveIn) {
        if (idx++ > 0) oss << ",";
        oss << "\"" << escapeJson(s) << "\"";
    }
    oss << "],\"liveOut\":[";
    idx = 0;
    for (const auto& s : liveOut) {
        if (idx++ > 0) oss << ",";
        oss << "\"" << escapeJson(s) << "\"";
    }
    oss << "]}";
    return oss.str();
}

LivenessAnalysis::LivenessAnalysis(CFG& cfg) : cfg_(cfg) {}

void LivenessAnalysis::extractExprVariables(const Expr* expr, std::set<std::string>& reads) {
    if (!expr) return;

    if (auto* var = dynamic_cast<const VariableExpr*>(expr)) {
        reads.insert(var->name);
    } else if (auto* bin = dynamic_cast<const BinaryExpr*>(expr)) {
        extractExprVariables(bin->left.get(), reads);
        extractExprVariables(bin->right.get(), reads);
    } else if (auto* un = dynamic_cast<const UnaryExpr*>(expr)) {
        extractExprVariables(un->operand.get(), reads);
    } else if (auto* call = dynamic_cast<const CallExpr*>(expr)) {
        for (const auto& arg : call->args) {
            extractExprVariables(arg.get(), reads);
        }
    }
}

void LivenessAnalysis::analyzeStatementUseDef(const Stmt* stmt, std::set<std::string>& blockUse, std::set<std::string>& blockDef) {
    if (!stmt) return;

    if (auto* decl = dynamic_cast<const VarDeclStmt*>(stmt)) {
        if (decl->initializer) {
            std::set<std::string> exprReads;
            extractExprVariables(decl->initializer.get(), exprReads);
            for (const auto& r : exprReads) {
                if (blockDef.find(r) == blockDef.end()) {
                    blockUse.insert(r);
                }
            }
        }
        blockDef.insert(decl->name);
    } else if (auto* assign = dynamic_cast<const AssignStmt*>(stmt)) {
        std::set<std::string> exprReads;
        extractExprVariables(assign->value.get(), exprReads);
        for (const auto& r : exprReads) {
            if (blockDef.find(r) == blockDef.end()) {
                blockUse.insert(r);
            }
        }
        blockDef.insert(assign->name);
    } else if (auto* print = dynamic_cast<const PrintStmt*>(stmt)) {
        std::set<std::string> exprReads;
        extractExprVariables(print->expression.get(), exprReads);
        for (const auto& r : exprReads) {
            if (blockDef.find(r) == blockDef.end()) {
                blockUse.insert(r);
            }
        }
    } else if (auto* ifStmt = dynamic_cast<const IfStmt*>(stmt)) {
        std::set<std::string> exprReads;
        extractExprVariables(ifStmt->condition.get(), exprReads);
        for (const auto& r : exprReads) {
            if (blockDef.find(r) == blockDef.end()) {
                blockUse.insert(r);
            }
        }
    } else if (auto* whileStmt = dynamic_cast<const WhileStmt*>(stmt)) {
        std::set<std::string> exprReads;
        extractExprVariables(whileStmt->condition.get(), exprReads);
        for (const auto& r : exprReads) {
            if (blockDef.find(r) == blockDef.end()) {
                blockUse.insert(r);
            }
        }
    }
}

void LivenessAnalysis::computeUseDef() {
    for (auto& block : cfg_.getBlocks()) {
        block->use.clear();
        block->def.clear();
        for (const auto* stmt : block->statements) {
            analyzeStatementUseDef(stmt, block->use, block->def);
        }
    }
}

void LivenessAnalysis::run() {
    computeUseDef();

    for (auto& block : cfg_.getBlocks()) {
        block->liveIn.clear();
        block->liveOut.clear();
    }

    bool changed = true;
    iterationCount_ = 0;

    while (changed) {
        changed = false;
        iterationCount_++;

        // Backward traversal across blocks
        const auto& blocks = cfg_.getBlocks();
        for (auto it = blocks.rbegin(); it != blocks.rend(); ++it) {
            auto& block = *it;

            // LIVE_OUT[B] = union over succ(B) of LIVE_IN[S]
            std::set<std::string> newLiveOut;
            for (const auto& edge : block->successors) {
                auto succBlock = cfg_.getBlock(edge.targetBlockId);
                if (succBlock) {
                    newLiveOut.insert(succBlock->liveIn.begin(), succBlock->liveIn.end());
                }
            }

            // LIVE_IN[B] = USE[B] union (LIVE_OUT[B] \ DEF[B])
            std::set<std::string> newLiveIn = block->use;
            for (const auto& var : newLiveOut) {
                if (block->def.find(var) == block->def.end()) {
                    newLiveIn.insert(var);
                }
            }

            if (newLiveIn != block->liveIn || newLiveOut != block->liveOut) {
                changed = true;
                block->liveIn = std::move(newLiveIn);
                block->liveOut = std::move(newLiveOut);
            }
        }
    }
}

std::set<std::string> LivenessAnalysis::getLiveVariablesAtCheckpoint(size_t checkpointId) const {
    for (const auto& b : cfg_.getBlocks()) {
        for (const auto* s : b->statements) {
            if (auto* c = dynamic_cast<const CheckpointStmt*>(s)) {
                if (c->checkpointId == checkpointId) {
                    // Liveness at this checkpoint is its block's liveOut
                    // (since checkpoint blocks have the checkpoint as their statement)
                    return b->liveOut;
                }
            }
        }
    }

    // Default: find block that has checkpoint
    for (const auto& b : cfg_.getBlocks()) {
        if (b->hasCheckpoint()) {
            return b->liveOut;
        }
    }
    return {};
}

std::set<std::string> LivenessAnalysis::getLiveVariablesAtBlock(size_t blockId) const {
    auto b = cfg_.getBlock(blockId);
    if (b) return b->liveIn;
    return {};
}

std::vector<BlockLivenessFacts> LivenessAnalysis::getAllFacts() const {
    std::vector<BlockLivenessFacts> facts;
    for (const auto& b : cfg_.getBlocks()) {
        BlockLivenessFacts f;
        f.blockId = b->id;
        f.use = b->use;
        f.def = b->def;
        f.liveIn = b->liveIn;
        f.liveOut = b->liveOut;
        facts.push_back(f);
    }
    return facts;
}

std::string LivenessAnalysis::toJson() const {
    std::ostringstream oss;
    oss << "{\"iterations\":" << iterationCount_ << ",\"blockFacts\":[";
    auto facts = getAllFacts();
    for (size_t i = 0; i < facts.size(); ++i) {
        if (i > 0) oss << ",";
        oss << facts[i].toJson();
    }
    oss << "]}";
    return oss.str();
}

} // namespace edge

#include "analysis/DefUseAnalysis.h"
#include <sstream>

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

void DefUseAnalysis::extractReads(const Expr* expr, std::vector<std::string>& reads) {
    if (!expr) return;
    if (auto* var = dynamic_cast<const VariableExpr*>(expr)) {
        reads.push_back(var->name);
    } else if (auto* bin = dynamic_cast<const BinaryExpr*>(expr)) {
        extractReads(bin->left.get(), reads);
        extractReads(bin->right.get(), reads);
    } else if (auto* un = dynamic_cast<const UnaryExpr*>(expr)) {
        extractReads(un->operand.get(), reads);
    } else if (auto* call = dynamic_cast<const CallExpr*>(expr)) {
        for (const auto& a : call->args) {
            extractReads(a.get(), reads);
        }
    }
}

void DefUseAnalysis::visitStatement(const Stmt* stmt) {
    if (!stmt) return;

    if (auto* decl = dynamic_cast<const VarDeclStmt*>(stmt)) {
        varDefs_[decl->name].push_back(decl->statementId);
        stmtDefs_[decl->statementId].push_back(decl->name);
        if (decl->initializer) {
            std::vector<std::string> reads;
            extractReads(decl->initializer.get(), reads);
            for (const auto& r : reads) {
                varUses_[r].push_back(decl->statementId);
                stmtUses_[decl->statementId].push_back(r);
            }
        }
    } else if (auto* assign = dynamic_cast<const AssignStmt*>(stmt)) {
        varDefs_[assign->name].push_back(assign->statementId);
        stmtDefs_[assign->statementId].push_back(assign->name);
        std::vector<std::string> reads;
        extractReads(assign->value.get(), reads);
        for (const auto& r : reads) {
            varUses_[r].push_back(assign->statementId);
            stmtUses_[assign->statementId].push_back(r);
        }
    } else if (auto* print = dynamic_cast<const PrintStmt*>(stmt)) {
        std::vector<std::string> reads;
        extractReads(print->expression.get(), reads);
        for (const auto& r : reads) {
            varUses_[r].push_back(print->statementId);
            stmtUses_[print->statementId].push_back(r);
        }
    } else if (auto* ifStmt = dynamic_cast<const IfStmt*>(stmt)) {
        std::vector<std::string> reads;
        extractReads(ifStmt->condition.get(), reads);
        for (const auto& r : reads) {
            varUses_[r].push_back(ifStmt->statementId);
            stmtUses_[ifStmt->statementId].push_back(r);
        }
        if (ifStmt->thenBranch) visitStatement(ifStmt->thenBranch.get());
        if (ifStmt->elseBranch) visitStatement(ifStmt->elseBranch.get());
    } else if (auto* whileStmt = dynamic_cast<const WhileStmt*>(stmt)) {
        std::vector<std::string> reads;
        extractReads(whileStmt->condition.get(), reads);
        for (const auto& r : reads) {
            varUses_[r].push_back(whileStmt->statementId);
            stmtUses_[whileStmt->statementId].push_back(r);
        }
        if (whileStmt->body) visitStatement(whileStmt->body.get());
    } else if (auto* block = dynamic_cast<const BlockStmt*>(stmt)) {
        for (const auto& s : block->statements) {
            if (s) visitStatement(s.get());
        }
    }
}

void DefUseAnalysis::analyze(const Program& program) {
    varDefs_.clear();
    varUses_.clear();
    stmtDefs_.clear();
    stmtUses_.clear();

    for (const auto& s : program.statements) {
        if (s) visitStatement(s.get());
    }
}

static const std::vector<size_t> emptyVec;

const std::vector<size_t>& DefUseAnalysis::getDefs(const std::string& var) const {
    auto it = varDefs_.find(var);
    if (it != varDefs_.end()) return it->second;
    return emptyVec;
}

const std::vector<size_t>& DefUseAnalysis::getUses(const std::string& var) const {
    auto it = varUses_.find(var);
    if (it != varUses_.end()) return it->second;
    return emptyVec;
}

std::string DefUseAnalysis::toJson() const {
    std::ostringstream oss;
    oss << "{\"variables\":[";
    bool first = true;
    for (const auto& pair : varDefs_) {
        if (!first) oss << ",";
        first = false;
        oss << "{\"name\":\"" << escapeJson(pair.first) << "\",\"defs\":[";
        for (size_t i = 0; i < pair.second.size(); ++i) {
            if (i > 0) oss << ",";
            oss << pair.second[i];
        }
        oss << "],\"uses\":[";
        auto uit = varUses_.find(pair.first);
        if (uit != varUses_.end()) {
            for (size_t i = 0; i < uit->second.size(); ++i) {
                if (i > 0) oss << ",";
                oss << uit->second[i];
            }
        }
        oss << "]}";
    }
    oss << "]}";
    return oss.str();
}

} // namespace edge

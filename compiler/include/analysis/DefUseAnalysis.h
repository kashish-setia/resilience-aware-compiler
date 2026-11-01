#pragma once

#include "parser/AST.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <set>

namespace edge {

struct DefUseInfo {
    size_t statementId;
    std::string variable;
    std::set<size_t> uses;   // statement IDs that read this def
};

struct UseDefInfo {
    size_t statementId;
    std::string variable;
    std::set<size_t> defs;   // statement IDs that defined this variable
};

class DefUseAnalysis {
private:
    std::unordered_map<std::string, std::vector<size_t>> varDefs_;
    std::unordered_map<std::string, std::vector<size_t>> varUses_;
    std::unordered_map<size_t, std::vector<std::string>> stmtDefs_;
    std::unordered_map<size_t, std::vector<std::string>> stmtUses_;

    void extractReads(const Expr* expr, std::vector<std::string>& reads);
    void visitStatement(const Stmt* stmt);

public:
    DefUseAnalysis() = default;

    void analyze(const Program& program);

    const std::vector<size_t>& getDefs(const std::string& var) const;
    const std::vector<size_t>& getUses(const std::string& var) const;

    std::string toJson() const;
};

} // namespace edge

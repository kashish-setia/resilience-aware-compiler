#pragma once

#include "parser/AST.h"
#include "cfg/CFG.h"
#include <memory>

namespace edge {

class CFGBuilder {
private:
    std::unique_ptr<CFG> cfg_;
    std::shared_ptr<BasicBlock> currentBlock_;

    void buildStatement(const Stmt* stmt);
    void buildIf(const IfStmt* stmt);
    void buildWhile(const WhileStmt* stmt);
    void buildBlock(const BlockStmt* stmt);

public:
    CFGBuilder() = default;

    std::unique_ptr<CFG> build(const Program& program);
};

} // namespace edge

#pragma once

#include "parser/AST.h"
#include "ir/Bytecode.h"
#include <memory>

namespace edge {

class CodeGenerator : public ASTVisitor {
private:
    std::unique_ptr<BytecodeProgram> program_;

    void emit(Instruction instr);

public:
    CodeGenerator() = default;

    std::unique_ptr<BytecodeProgram> generate(const Program& program);

    void visit(Program& node) override;
    void visit(VarDeclStmt& node) override;
    void visit(AssignStmt& node) override;
    void visit(BlockStmt& node) override;
    void visit(IfStmt& node) override;
    void visit(WhileStmt& node) override;
    void visit(PrintStmt& node) override;
    void visit(CheckpointStmt& node) override;
    void visit(FaultStmt& node) override;

    void visit(LiteralExpr& node) override;
    void visit(VariableExpr& node) override;
    void visit(BinaryExpr& node) override;
    void visit(UnaryExpr& node) override;
    void visit(CallExpr& node) override;
};

} // namespace edge

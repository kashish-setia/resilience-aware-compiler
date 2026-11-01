#pragma once

#include "parser/AST.h"
#include "semantic/SymbolTable.h"
#include "common/Diagnostics.h"
#include <memory>

namespace edge {

class SemanticAnalyzer : public ASTVisitor {
private:
    SymbolTable symbolTable_;
    DiagnosticEngine& diag_;
    TypeKind currentExprType_{TypeKind::UNKNOWN};
    bool currentExprIsExternal_{false};

    bool checkTypeMatch(TypeKind expected, TypeKind actual, const SourceLocation& loc, const std::string& context);
    bool isNumeric(TypeKind type) const;

public:
    explicit SemanticAnalyzer(DiagnosticEngine& diag);

    bool analyze(Program& program);

    SymbolTable& getSymbolTable() { return symbolTable_; }
    const SymbolTable& getSymbolTable() const { return symbolTable_; }

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

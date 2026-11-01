#pragma once

#include "common/SourceLocation.h"
#include "common/Types.h"
#include "common/Annotations.h"
#include "common/Value.h"
#include <string>
#include <vector>
#include <memory>

namespace edge {

enum class BinaryOp {
    ADD, SUB, MUL, DIV, MOD,
    EQ, NEQ, LT, LTE, GT, GTE,
    AND, OR
};

enum class UnaryOp {
    NEG, NOT
};

std::string binaryOpToString(BinaryOp op);
std::string unaryOpToString(UnaryOp op);

class ASTVisitor;

class ASTNode {
public:
    SourceLocation location;

    explicit ASTNode(SourceLocation loc) : location(std::move(loc)) {}
    virtual ~ASTNode() = default;

    virtual void accept(ASTVisitor& visitor) = 0;
    virtual std::string toString(int indent = 0) const = 0;
    virtual std::string toJson() const = 0;

protected:
    static std::string makeIndent(int indent) {
        return std::string(indent * 2, ' ');
    }
};

// Expressions
class Expr : public ASTNode {
public:
    TypeKind evaluatedType{TypeKind::UNKNOWN};

    explicit Expr(SourceLocation loc) : ASTNode(std::move(loc)) {}
};

class LiteralExpr : public Expr {
public:
    Value value;

    LiteralExpr(Value val, SourceLocation loc)
        : Expr(std::move(loc)), value(std::move(val)) {
        evaluatedType = value.getType();
    }

    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override;
    std::string toJson() const override;
};

class VariableExpr : public Expr {
public:
    std::string name;

    VariableExpr(std::string name, SourceLocation loc)
        : Expr(std::move(loc)), name(std::move(name)) {}

    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override;
    std::string toJson() const override;
};

class BinaryExpr : public Expr {
public:
    BinaryOp op;
    std::unique_ptr<Expr> left;
    std::unique_ptr<Expr> right;

    BinaryExpr(BinaryOp op, std::unique_ptr<Expr> left, std::unique_ptr<Expr> right, SourceLocation loc)
        : Expr(std::move(loc)), op(op), left(std::move(left)), right(std::move(right)) {}

    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override;
    std::string toJson() const override;
};

class UnaryExpr : public Expr {
public:
    UnaryOp op;
    std::unique_ptr<Expr> operand;

    UnaryExpr(UnaryOp op, std::unique_ptr<Expr> operand, SourceLocation loc)
        : Expr(std::move(loc)), op(op), operand(std::move(operand)) {}

    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override;
    std::string toJson() const override;
};

class CallExpr : public Expr {
public:
    std::string callee;
    std::vector<std::unique_ptr<Expr>> args;

    CallExpr(std::string callee, std::vector<std::unique_ptr<Expr>> args, SourceLocation loc)
        : Expr(std::move(loc)), callee(std::move(callee)), args(std::move(args)) {}

    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override;
    std::string toJson() const override;
};

// Statements
class Stmt : public ASTNode {
public:
    size_t statementId{0};

    explicit Stmt(SourceLocation loc) : ASTNode(std::move(loc)) {}
};

class VarDeclStmt : public Stmt {
public:
    std::string name;
    TypeKind type;
    AnnotationKind annotation;
    std::unique_ptr<Expr> initializer;

    VarDeclStmt(std::string name, TypeKind type, AnnotationKind ann, std::unique_ptr<Expr> init, SourceLocation loc)
        : Stmt(std::move(loc)), name(std::move(name)), type(type), annotation(ann), initializer(std::move(init)) {}

    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override;
    std::string toJson() const override;
};

class AssignStmt : public Stmt {
public:
    std::string name;
    std::unique_ptr<Expr> value;

    AssignStmt(std::string name, std::unique_ptr<Expr> val, SourceLocation loc)
        : Stmt(std::move(loc)), name(std::move(name)), value(std::move(val)) {}

    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override;
    std::string toJson() const override;
};

class BlockStmt : public Stmt {
public:
    std::vector<std::unique_ptr<Stmt>> statements;

    BlockStmt(std::vector<std::unique_ptr<Stmt>> stmts, SourceLocation loc)
        : Stmt(std::move(loc)), statements(std::move(stmts)) {}

    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override;
    std::string toJson() const override;
};

class IfStmt : public Stmt {
public:
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Stmt> thenBranch;
    std::unique_ptr<Stmt> elseBranch;

    IfStmt(std::unique_ptr<Expr> cond, std::unique_ptr<Stmt> thenB, std::unique_ptr<Stmt> elseB, SourceLocation loc)
        : Stmt(std::move(loc)), condition(std::move(cond)), thenBranch(std::move(thenB)), elseBranch(std::move(elseB)) {}

    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override;
    std::string toJson() const override;
};

class WhileStmt : public Stmt {
public:
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Stmt> body;

    WhileStmt(std::unique_ptr<Expr> cond, std::unique_ptr<Stmt> body, SourceLocation loc)
        : Stmt(std::move(loc)), condition(std::move(cond)), body(std::move(body)) {}

    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override;
    std::string toJson() const override;
};

class PrintStmt : public Stmt {
public:
    std::unique_ptr<Expr> expression;

    PrintStmt(std::unique_ptr<Expr> expr, SourceLocation loc)
        : Stmt(std::move(loc)), expression(std::move(expr)) {}

    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override;
    std::string toJson() const override;
};

class CheckpointStmt : public Stmt {
public:
    std::string label;
    size_t checkpointId{0};

    CheckpointStmt(std::string label, size_t id, SourceLocation loc)
        : Stmt(std::move(loc)), label(std::move(label)), checkpointId(id) {}

    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override;
    std::string toJson() const override;
};

class FaultStmt : public Stmt {
public:
    std::string reason;

    FaultStmt(std::string reason, SourceLocation loc)
        : Stmt(std::move(loc)), reason(std::move(reason)) {}

    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override;
    std::string toJson() const override;
};

class Program : public ASTNode {
public:
    std::string name;
    std::vector<std::unique_ptr<Stmt>> statements;

    Program(std::string name, std::vector<std::unique_ptr<Stmt>> stmts, SourceLocation loc)
        : ASTNode(std::move(loc)), name(std::move(name)), statements(std::move(stmts)) {}

    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override;
    std::string toJson() const override;
};

class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;

    virtual void visit(Program& node) = 0;
    virtual void visit(VarDeclStmt& node) = 0;
    virtual void visit(AssignStmt& node) = 0;
    virtual void visit(BlockStmt& node) = 0;
    virtual void visit(IfStmt& node) = 0;
    virtual void visit(WhileStmt& node) = 0;
    virtual void visit(PrintStmt& node) = 0;
    virtual void visit(CheckpointStmt& node) = 0;
    virtual void visit(FaultStmt& node) = 0;

    virtual void visit(LiteralExpr& node) = 0;
    virtual void visit(VariableExpr& node) = 0;
    virtual void visit(BinaryExpr& node) = 0;
    virtual void visit(UnaryExpr& node) = 0;
    virtual void visit(CallExpr& node) = 0;
};

} // namespace edge

#pragma once

#include "lexer/Token.h"
#include "parser/AST.h"
#include "common/Diagnostics.h"
#include <vector>
#include <memory>

namespace edge {

class Parser {
private:
    std::vector<Token> tokens_;
    size_t cursor_{0};
    DiagnosticEngine& diag_;
    size_t nextStmtId_{1};
    size_t nextCheckpointId_{1};

    const Token& peek() const;
    const Token& previous() const;
    bool isAtEnd() const;
    Token advance();
    bool check(TokenKind kind) const;
    bool match(TokenKind kind);
    Token consume(TokenKind kind, const std::string& errorMessage);

    void synchronize();

    // Grammar rules
    std::unique_ptr<Program> parseProgram();
    std::unique_ptr<Stmt> parseStatement();
    std::unique_ptr<VarDeclStmt> parseVarDecl(AnnotationKind ann = AnnotationKind::NONE);
    std::unique_ptr<AssignStmt> parseAssign();
    std::unique_ptr<BlockStmt> parseBlock();
    std::unique_ptr<IfStmt> parseIf();
    std::unique_ptr<WhileStmt> parseWhile();
    std::unique_ptr<PrintStmt> parsePrint();
    std::unique_ptr<CheckpointStmt> parseCheckpoint();
    std::unique_ptr<FaultStmt> parseFault();

    std::unique_ptr<Expr> parseExpression();
    std::unique_ptr<Expr> parseLogicalOr();
    std::unique_ptr<Expr> parseLogicalAnd();
    std::unique_ptr<Expr> parseEquality();
    std::unique_ptr<Expr> parseRelational();
    std::unique_ptr<Expr> parseAdditive();
    std::unique_ptr<Expr> parseMultiplicative();
    std::unique_ptr<Expr> parseUnary();
    std::unique_ptr<Expr> parsePrimary();

public:
    Parser(std::vector<Token> tokens, DiagnosticEngine& diag);

    std::unique_ptr<Program> parse();
};

} // namespace edge

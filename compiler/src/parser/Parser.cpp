#include "parser/Parser.h"
#include <iostream>

namespace edge {

Parser::Parser(std::vector<Token> tokens, DiagnosticEngine& diag)
    : tokens_(std::move(tokens)), cursor_(0), diag_(diag) {}

const Token& Parser::peek() const {
    if (cursor_ >= tokens_.size()) {
        return tokens_.back();
    }
    return tokens_[cursor_];
}

const Token& Parser::previous() const {
    return tokens_[cursor_ - 1];
}

bool Parser::isAtEnd() const {
    return peek().kind == TokenKind::TOKEN_EOF;
}

Token Parser::advance() {
    if (!isAtEnd()) cursor_++;
    return previous();
}

bool Parser::check(TokenKind kind) const {
    if (isAtEnd()) return false;
    return peek().kind == kind;
}

bool Parser::match(TokenKind kind) {
    if (check(kind)) {
        advance();
        return true;
    }
    return false;
}

Token Parser::consume(TokenKind kind, const std::string& errorMessage) {
    if (check(kind)) return advance();
    diag_.error(errorMessage, peek().location);
    return peek();
}

void Parser::synchronize() {
    advance();
    while (!isAtEnd()) {
        if (previous().kind == TokenKind::SEMICOLON) return;

        switch (peek().kind) {
            case TokenKind::KW_PROGRAM:
            case TokenKind::KW_INT:
            case TokenKind::KW_FLOAT:
            case TokenKind::KW_BOOL:
            case TokenKind::KW_STRING:
            case TokenKind::KW_IF:
            case TokenKind::KW_WHILE:
            case TokenKind::KW_PRINT:
            case TokenKind::KW_CHECKPOINT:
            case TokenKind::KW_FAULT:
            case TokenKind::ANN_CRITICAL:
            case TokenKind::ANN_OPTIONAL:
            case TokenKind::ANN_DISPOSABLE:
                return;
            default:
                break;
        }
        advance();
    }
}

std::unique_ptr<Program> Parser::parse() {
    return parseProgram();
}

std::unique_ptr<Program> Parser::parseProgram() {
    SourceLocation loc = peek().location;
    consume(TokenKind::KW_PROGRAM, "Expected 'program' keyword");
    Token nameTok = consume(TokenKind::IDENTIFIER, "Expected program name");
    consume(TokenKind::LBRACE, "Expected '{' after program name");

    std::vector<std::unique_ptr<Stmt>> statements;
    while (!check(TokenKind::RBRACE) && !isAtEnd()) {
        auto stmt = parseStatement();
        if (stmt) {
            statements.push_back(std::move(stmt));
        } else {
            synchronize();
        }
    }

    consume(TokenKind::RBRACE, "Expected '}' at the end of program");
    return std::make_unique<Program>(nameTok.lexeme, std::move(statements), loc);
}

std::unique_ptr<Stmt> Parser::parseStatement() {
    if (match(TokenKind::LBRACE)) {
        return parseBlock();
    }
    if (match(TokenKind::KW_IF)) {
        return parseIf();
    }
    if (match(TokenKind::KW_WHILE)) {
        return parseWhile();
    }
    if (match(TokenKind::KW_PRINT)) {
        return parsePrint();
    }
    if (match(TokenKind::KW_CHECKPOINT)) {
        return parseCheckpoint();
    }
    if (match(TokenKind::KW_FAULT)) {
        return parseFault();
    }

    if (match(TokenKind::ANN_CRITICAL)) {
        return parseVarDecl(AnnotationKind::CRITICAL);
    }
    if (match(TokenKind::ANN_OPTIONAL)) {
        return parseVarDecl(AnnotationKind::OPTIONAL);
    }
    if (match(TokenKind::ANN_DISPOSABLE)) {
        return parseVarDecl(AnnotationKind::DISPOSABLE);
    }

    if (check(TokenKind::KW_INT) || check(TokenKind::KW_FLOAT) || 
        check(TokenKind::KW_BOOL) || check(TokenKind::KW_STRING)) {
        return parseVarDecl(AnnotationKind::NONE);
    }

    if (check(TokenKind::IDENTIFIER)) {
        // Lookahead to distinguish assignment `x = ...` from other statements
        if (cursor_ + 1 < tokens_.size() && tokens_[cursor_ + 1].kind == TokenKind::EQUAL) {
            return parseAssign();
        }
    }

    diag_.error("Expected statement", peek().location);
    advance();
    return nullptr;
}

std::unique_ptr<BlockStmt> Parser::parseBlock() {
    SourceLocation loc = previous().location;
    std::vector<std::unique_ptr<Stmt>> stmts;
    while (!check(TokenKind::RBRACE) && !isAtEnd()) {
        auto s = parseStatement();
        if (s) stmts.push_back(std::move(s));
    }
    consume(TokenKind::RBRACE, "Expected '}' to close block");
    auto block = std::make_unique<BlockStmt>(std::move(stmts), loc);
    block->statementId = nextStmtId_++;
    return block;
}

std::unique_ptr<VarDeclStmt> Parser::parseVarDecl(AnnotationKind ann) {
    SourceLocation loc = peek().location;
    TypeKind type = TypeKind::UNKNOWN;

    if (match(TokenKind::KW_INT)) type = TypeKind::INT;
    else if (match(TokenKind::KW_FLOAT)) type = TypeKind::FLOAT;
    else if (match(TokenKind::KW_BOOL)) type = TypeKind::BOOL;
    else if (match(TokenKind::KW_STRING)) type = TypeKind::STRING;
    else {
        diag_.error("Expected type specification (int, float, bool, string)", loc);
        return nullptr;
    }

    Token nameTok = consume(TokenKind::IDENTIFIER, "Expected variable name in declaration");
    std::unique_ptr<Expr> init = nullptr;

    if (match(TokenKind::EQUAL)) {
        init = parseExpression();
    }

    consume(TokenKind::SEMICOLON, "Expected ';' after variable declaration");

    auto decl = std::make_unique<VarDeclStmt>(nameTok.lexeme, type, ann, std::move(init), loc);
    decl->statementId = nextStmtId_++;
    return decl;
}

std::unique_ptr<AssignStmt> Parser::parseAssign() {
    Token nameTok = consume(TokenKind::IDENTIFIER, "Expected variable name in assignment");
    consume(TokenKind::EQUAL, "Expected '=' in assignment");
    auto val = parseExpression();
    consume(TokenKind::SEMICOLON, "Expected ';' after assignment");

    auto assign = std::make_unique<AssignStmt>(nameTok.lexeme, std::move(val), nameTok.location);
    assign->statementId = nextStmtId_++;
    return assign;
}

std::unique_ptr<IfStmt> Parser::parseIf() {
    SourceLocation loc = previous().location;
    consume(TokenKind::LPAREN, "Expected '(' after 'if'");
    auto cond = parseExpression();
    consume(TokenKind::RPAREN, "Expected ')' after if condition");

    auto thenBranch = parseStatement();
    std::unique_ptr<Stmt> elseBranch = nullptr;

    if (match(TokenKind::KW_ELSE)) {
        elseBranch = parseStatement();
    }

    auto ifStmt = std::make_unique<IfStmt>(std::move(cond), std::move(thenBranch), std::move(elseBranch), loc);
    ifStmt->statementId = nextStmtId_++;
    return ifStmt;
}

std::unique_ptr<WhileStmt> Parser::parseWhile() {
    SourceLocation loc = previous().location;
    consume(TokenKind::LPAREN, "Expected '(' after 'while'");
    auto cond = parseExpression();
    consume(TokenKind::RPAREN, "Expected ')' after while condition");

    auto body = parseStatement();

    auto whileStmt = std::make_unique<WhileStmt>(std::move(cond), std::move(body), loc);
    whileStmt->statementId = nextStmtId_++;
    return whileStmt;
}

std::unique_ptr<PrintStmt> Parser::parsePrint() {
    SourceLocation loc = previous().location;
    consume(TokenKind::LPAREN, "Expected '(' after 'print'");
    auto expr = parseExpression();
    consume(TokenKind::RPAREN, "Expected ')' after print argument");
    consume(TokenKind::SEMICOLON, "Expected ';' after print statement");

    auto printStmt = std::make_unique<PrintStmt>(std::move(expr), loc);
    printStmt->statementId = nextStmtId_++;
    return printStmt;
}

std::unique_ptr<CheckpointStmt> Parser::parseCheckpoint() {
    SourceLocation loc = previous().location;
    std::string label = "chk_" + std::to_string(nextCheckpointId_);

    if (match(TokenKind::LPAREN)) {
        if (check(TokenKind::IDENTIFIER) || check(TokenKind::STRING_LITERAL)) {
            label = advance().lexeme;
        }
        consume(TokenKind::RPAREN, "Expected ')' after checkpoint label");
    }

    consume(TokenKind::SEMICOLON, "Expected ';' after checkpoint");

    auto chk = std::make_unique<CheckpointStmt>(label, nextCheckpointId_++, loc);
    chk->statementId = nextStmtId_++;
    return chk;
}

std::unique_ptr<FaultStmt> Parser::parseFault() {
    SourceLocation loc = previous().location;
    std::string reason = "simulated_power_loss";

    if (match(TokenKind::LPAREN)) {
        if (check(TokenKind::STRING_LITERAL)) {
            reason = advance().lexeme;
        }
        consume(TokenKind::RPAREN, "Expected ')' after fault reason");
    }

    consume(TokenKind::SEMICOLON, "Expected ';' after fault");

    auto fault = std::make_unique<FaultStmt>(reason, loc);
    fault->statementId = nextStmtId_++;
    return fault;
}

// Expression parsing
std::unique_ptr<Expr> Parser::parseExpression() {
    return parseLogicalOr();
}

std::unique_ptr<Expr> Parser::parseLogicalOr() {
    auto expr = parseLogicalAnd();
    while (match(TokenKind::OR_OR)) {
        Token opTok = previous();
        auto right = parseLogicalAnd();
        expr = std::make_unique<BinaryExpr>(BinaryOp::OR, std::move(expr), std::move(right), opTok.location);
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseLogicalAnd() {
    auto expr = parseEquality();
    while (match(TokenKind::AND_AND)) {
        Token opTok = previous();
        auto right = parseEquality();
        expr = std::make_unique<BinaryExpr>(BinaryOp::AND, std::move(expr), std::move(right), opTok.location);
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseEquality() {
    auto expr = parseRelational();
    while (check(TokenKind::EQ_EQ) || check(TokenKind::BANG_EQ)) {
        Token opTok = advance();
        BinaryOp op = (opTok.kind == TokenKind::EQ_EQ) ? BinaryOp::EQ : BinaryOp::NEQ;
        auto right = parseRelational();
        expr = std::make_unique<BinaryExpr>(op, std::move(expr), std::move(right), opTok.location);
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseRelational() {
    auto expr = parseAdditive();
    while (check(TokenKind::LESS) || check(TokenKind::LESS_EQ) ||
           check(TokenKind::GREATER) || check(TokenKind::GREATER_EQ)) {
        Token opTok = advance();
        BinaryOp op = BinaryOp::LT;
        if (opTok.kind == TokenKind::LESS) op = BinaryOp::LT;
        else if (opTok.kind == TokenKind::LESS_EQ) op = BinaryOp::LTE;
        else if (opTok.kind == TokenKind::GREATER) op = BinaryOp::GT;
        else if (opTok.kind == TokenKind::GREATER_EQ) op = BinaryOp::GTE;

        auto right = parseAdditive();
        expr = std::make_unique<BinaryExpr>(op, std::move(expr), std::move(right), opTok.location);
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseAdditive() {
    auto expr = parseMultiplicative();
    while (check(TokenKind::PLUS) || check(TokenKind::MINUS)) {
        Token opTok = advance();
        BinaryOp op = (opTok.kind == TokenKind::PLUS) ? BinaryOp::ADD : BinaryOp::SUB;
        auto right = parseMultiplicative();
        expr = std::make_unique<BinaryExpr>(op, std::move(expr), std::move(right), opTok.location);
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseMultiplicative() {
    auto expr = parseUnary();
    while (check(TokenKind::STAR) || check(TokenKind::SLASH) || check(TokenKind::PERCENT)) {
        Token opTok = advance();
        BinaryOp op = BinaryOp::MUL;
        if (opTok.kind == TokenKind::STAR) op = BinaryOp::MUL;
        else if (opTok.kind == TokenKind::SLASH) op = BinaryOp::DIV;
        else if (opTok.kind == TokenKind::PERCENT) op = BinaryOp::MOD;

        auto right = parseUnary();
        expr = std::make_unique<BinaryExpr>(op, std::move(expr), std::move(right), opTok.location);
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseUnary() {
    if (check(TokenKind::BANG) || check(TokenKind::MINUS)) {
        Token opTok = advance();
        UnaryOp op = (opTok.kind == TokenKind::BANG) ? UnaryOp::NOT : UnaryOp::NEG;
        auto operand = parseUnary();
        return std::make_unique<UnaryExpr>(op, std::move(operand), opTok.location);
    }
    return parsePrimary();
}

std::unique_ptr<Expr> Parser::parsePrimary() {
    if (match(TokenKind::INT_LITERAL)) {
        int64_t val = std::stoll(previous().lexeme);
        return std::make_unique<LiteralExpr>(Value(val), previous().location);
    }
    if (match(TokenKind::FLOAT_LITERAL)) {
        double val = std::stod(previous().lexeme);
        return std::make_unique<LiteralExpr>(Value(val), previous().location);
    }
    if (match(TokenKind::BOOL_LITERAL)) {
        bool val = (previous().lexeme == "true");
        return std::make_unique<LiteralExpr>(Value(val), previous().location);
    }
    if (match(TokenKind::STRING_LITERAL)) {
        return std::make_unique<LiteralExpr>(Value(previous().lexeme), previous().location);
    }

    if (match(TokenKind::IDENTIFIER)) {
        Token idTok = previous();
        // Check for function call
        if (match(TokenKind::LPAREN)) {
            std::vector<std::unique_ptr<Expr>> args;
            if (!check(TokenKind::RPAREN)) {
                do {
                    args.push_back(parseExpression());
                } while (match(TokenKind::COMMA));
            }
            consume(TokenKind::RPAREN, "Expected ')' after function arguments");
            return std::make_unique<CallExpr>(idTok.lexeme, std::move(args), idTok.location);
        }
        return std::make_unique<VariableExpr>(idTok.lexeme, idTok.location);
    }

    if (match(TokenKind::LPAREN)) {
        auto expr = parseExpression();
        consume(TokenKind::RPAREN, "Expected ')' after parenthesized expression");
        return expr;
    }

    diag_.error("Expected expression, found '" + peek().lexeme + "'", peek().location);
    advance();
    return nullptr;
}

} // namespace edge

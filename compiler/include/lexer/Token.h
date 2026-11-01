#pragma once

#include "common/SourceLocation.h"
#include <string>

namespace edge {

enum class TokenKind {
    // End of file / error
    TOKEN_EOF,
    TOKEN_ERROR,

    // Literals and Identifiers
    IDENTIFIER,
    INT_LITERAL,
    FLOAT_LITERAL,
    BOOL_LITERAL,
    STRING_LITERAL,

    // Keywords
    KW_PROGRAM,
    KW_INT,
    KW_FLOAT,
    KW_BOOL,
    KW_STRING,
    KW_IF,
    KW_ELSE,
    KW_WHILE,
    KW_PRINT,
    KW_CHECKPOINT,
    KW_FAULT,

    // Annotations
    ANN_CRITICAL,
    ANN_OPTIONAL,
    ANN_DISPOSABLE,

    // Operators
    PLUS,           // +
    MINUS,          // -
    STAR,           // *
    SLASH,          // /
    PERCENT,        // %
    EQUAL,          // =

    // Relational Operators
    EQ_EQ,          // ==
    BANG_EQ,        // !=
    LESS,           // <
    LESS_EQ,        // <=
    GREATER,        // >
    GREATER_EQ,     // >=

    // Logical Operators
    AND_AND,        // &&
    OR_OR,          // ||
    BANG,           // !

    // Delimiters
    LPAREN,         // (
    RPAREN,         // )
    LBRACE,         // {
    RBRACE,         // }
    COMMA,          // ,
    SEMICOLON       // ;
};

std::string tokenKindToString(TokenKind kind);

struct Token {
    TokenKind kind;
    std::string lexeme;
    SourceLocation location;

    Token(TokenKind k, std::string lex, SourceLocation loc)
        : kind(k), lexeme(std::move(lex)), location(std::move(loc)) {}

    std::string toString() const;
    std::string toJson() const;
};

} // namespace edge

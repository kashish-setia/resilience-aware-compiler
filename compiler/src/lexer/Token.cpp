#include "lexer/Token.h"
#include <sstream>

namespace edge {

std::string tokenKindToString(TokenKind kind) {
    switch (kind) {
        case TokenKind::TOKEN_EOF: return "EOF";
        case TokenKind::TOKEN_ERROR: return "ERROR";
        case TokenKind::IDENTIFIER: return "IDENTIFIER";
        case TokenKind::INT_LITERAL: return "INT_LITERAL";
        case TokenKind::FLOAT_LITERAL: return "FLOAT_LITERAL";
        case TokenKind::BOOL_LITERAL: return "BOOL_LITERAL";
        case TokenKind::STRING_LITERAL: return "STRING_LITERAL";

        case TokenKind::KW_PROGRAM: return "program";
        case TokenKind::KW_INT: return "int";
        case TokenKind::KW_FLOAT: return "float";
        case TokenKind::KW_BOOL: return "bool";
        case TokenKind::KW_STRING: return "string";
        case TokenKind::KW_IF: return "if";
        case TokenKind::KW_ELSE: return "else";
        case TokenKind::KW_WHILE: return "while";
        case TokenKind::KW_PRINT: return "print";
        case TokenKind::KW_CHECKPOINT: return "checkpoint";
        case TokenKind::KW_FAULT: return "fault";

        case TokenKind::ANN_CRITICAL: return "@critical";
        case TokenKind::ANN_OPTIONAL: return "@optional";
        case TokenKind::ANN_DISPOSABLE: return "@disposable";

        case TokenKind::PLUS: return "+";
        case TokenKind::MINUS: return "-";
        case TokenKind::STAR: return "*";
        case TokenKind::SLASH: return "/";
        case TokenKind::PERCENT: return "%";
        case TokenKind::EQUAL: return "=";

        case TokenKind::EQ_EQ: return "==";
        case TokenKind::BANG_EQ: return "!=";
        case TokenKind::LESS: return "<";
        case TokenKind::LESS_EQ: return "<=";
        case TokenKind::GREATER: return ">";
        case TokenKind::GREATER_EQ: return ">=";

        case TokenKind::AND_AND: return "&&";
        case TokenKind::OR_OR: return "||";
        case TokenKind::BANG: return "!";

        case TokenKind::LPAREN: return "(";
        case TokenKind::RPAREN: return ")";
        case TokenKind::LBRACE: return "{";
        case TokenKind::RBRACE: return "}";
        case TokenKind::COMMA: return ",";
        case TokenKind::SEMICOLON: return ";";
    }
    return "UNKNOWN";
}

std::string Token::toString() const {
    std::ostringstream oss;
    oss << "[" << location.toString() << "] " << tokenKindToString(kind) << " '" << lexeme << "'";
    return oss.str();
}

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

std::string Token::toJson() const {
    std::ostringstream oss;
    oss << "{"
        << "\"kind\":\"" << tokenKindToString(kind) << "\","
        << "\"lexeme\":\"" << escapeJson(lexeme) << "\","
        << "\"line\":" << location.line << ","
        << "\"column\":" << location.column
        << "}";
    return oss.str();
}

} // namespace edge

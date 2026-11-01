#pragma once

#include "lexer/Token.h"
#include "common/Diagnostics.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace edge {

class Lexer {
private:
    std::string source_;
    std::string filename_;
    size_t cursor_{0};
    size_t line_{1};
    size_t column_{1};
    DiagnosticEngine& diag_;

    std::unordered_map<std::string, TokenKind> keywords_;
    std::unordered_map<std::string, TokenKind> annotations_;

    void initKeywords();
    char peek() const;
    char peekNext() const;
    char advance();
    bool isAtEnd() const;
    bool match(char expected);

    void skipWhitespaceAndComments();
    Token scanIdentifierOrKeyword();
    Token scanNumber();
    Token scanString();
    Token scanAnnotation();

public:
    Lexer(std::string source, DiagnosticEngine& diag, std::string filename = "<source>");

    Token nextToken();
    std::vector<Token> tokenize();
};

} // namespace edge

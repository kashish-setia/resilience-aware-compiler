#include "lexer/Lexer.h"
#include <cctype>

namespace edge {

Lexer::Lexer(std::string source, DiagnosticEngine& diag, std::string filename)
    : source_(std::move(source)), filename_(std::move(filename)), cursor_(0), line_(1), column_(1), diag_(diag) {
    initKeywords();
}

void Lexer::initKeywords() {
    keywords_["program"] = TokenKind::KW_PROGRAM;
    keywords_["int"] = TokenKind::KW_INT;
    keywords_["float"] = TokenKind::KW_FLOAT;
    keywords_["bool"] = TokenKind::KW_BOOL;
    keywords_["string"] = TokenKind::KW_STRING;
    keywords_["if"] = TokenKind::KW_IF;
    keywords_["else"] = TokenKind::KW_ELSE;
    keywords_["while"] = TokenKind::KW_WHILE;
    keywords_["print"] = TokenKind::KW_PRINT;
    keywords_["checkpoint"] = TokenKind::KW_CHECKPOINT;
    keywords_["fault"] = TokenKind::KW_FAULT;
    keywords_["true"] = TokenKind::BOOL_LITERAL;
    keywords_["false"] = TokenKind::BOOL_LITERAL;

    annotations_["critical"] = TokenKind::ANN_CRITICAL;
    annotations_["optional"] = TokenKind::ANN_OPTIONAL;
    annotations_["disposable"] = TokenKind::ANN_DISPOSABLE;
}

char Lexer::peek() const {
    if (isAtEnd()) return '\0';
    return source_[cursor_];
}

char Lexer::peekNext() const {
    if (cursor_ + 1 >= source_.size()) return '\0';
    return source_[cursor_ + 1];
}

char Lexer::advance() {
    char c = source_[cursor_++];
    if (c == '\n') {
        line_++;
        column_ = 1;
    } else {
        column_++;
    }
    return c;
}

bool Lexer::isAtEnd() const {
    return cursor_ >= source_.size();
}

bool Lexer::match(char expected) {
    if (isAtEnd() || source_[cursor_] != expected) return false;
    advance();
    return true;
}

void Lexer::skipWhitespaceAndComments() {
    while (!isAtEnd()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
        } else if (c == '/' && peekNext() == '/') {
            // Line comment
            advance(); // '/'
            advance(); // '/'
            while (!isAtEnd() && peek() != '\n') {
                advance();
            }
        } else if (c == '/' && peekNext() == '*') {
            // Block comment
            SourceLocation startLoc(line_, column_, filename_);
            advance(); // '/'
            advance(); // '*'
            bool closed = false;
            while (!isAtEnd()) {
                if (peek() == '*' && peekNext() == '/') {
                    advance(); // '*'
                    advance(); // '/'
                    closed = true;
                    break;
                }
                advance();
            }
            if (!closed) {
                diag_.error("Unterminated block comment", startLoc);
            }
        } else {
            break;
        }
    }
}

Token Lexer::scanIdentifierOrKeyword() {
    size_t startCol = column_;
    size_t startLine = line_;
    size_t startCursor = cursor_;

    while (std::isalnum(peek()) || peek() == '_') {
        advance();
    }

    std::string text = source_.substr(startCursor, cursor_ - startCursor);
    SourceLocation loc(startLine, startCol, filename_);

    auto it = keywords_.find(text);
    if (it != keywords_.end()) {
        return Token(it->second, text, loc);
    }
    return Token(TokenKind::IDENTIFIER, text, loc);
}

Token Lexer::scanAnnotation() {
    size_t startCol = column_;
    size_t startLine = line_;
    advance(); // Consume '@'

    size_t nameStart = cursor_;
    while (std::isalpha(peek()) || peek() == '_') {
        advance();
    }
    std::string name = source_.substr(nameStart, cursor_ - nameStart);
    SourceLocation loc(startLine, startCol, filename_);

    auto it = annotations_.find(name);
    if (it != annotations_.end()) {
        return Token(it->second, "@" + name, loc);
    }

    diag_.error("Unknown annotation '@" + name + "'", loc);
    return Token(TokenKind::TOKEN_ERROR, "@" + name, loc);
}

Token Lexer::scanNumber() {
    size_t startCol = column_;
    size_t startLine = line_;
    size_t startCursor = cursor_;
    bool isFloat = false;

    while (std::isdigit(peek())) {
        advance();
    }

    if (peek() == '.' && std::isdigit(peekNext())) {
        isFloat = true;
        advance(); // consume '.'
        while (std::isdigit(peek())) {
            advance();
        }
    }

    std::string text = source_.substr(startCursor, cursor_ - startCursor);
    SourceLocation loc(startLine, startCol, filename_);

    return Token(isFloat ? TokenKind::FLOAT_LITERAL : TokenKind::INT_LITERAL, text, loc);
}

Token Lexer::scanString() {
    size_t startCol = column_;
    size_t startLine = line_;
    advance(); // Consume leading '"'

    std::string content;
    bool closed = false;

    while (!isAtEnd()) {
        char c = peek();
        if (c == '"') {
            advance();
            closed = true;
            break;
        }
        if (c == '\\') {
            advance();
            if (isAtEnd()) break;
            char esc = advance();
            switch (esc) {
                case 'n': content += '\n'; break;
                case 't': content += '\t'; break;
                case 'r': content += '\r'; break;
                case '"': content += '"'; break;
                case '\\': content += '\\'; break;
                default: content += esc; break;
            }
        } else {
            content += advance();
        }
    }

    SourceLocation loc(startLine, startCol, filename_);
    if (!closed) {
        diag_.error("Unterminated string literal", loc);
        return Token(TokenKind::TOKEN_ERROR, content, loc);
    }
    return Token(TokenKind::STRING_LITERAL, content, loc);
}

Token Lexer::nextToken() {
    skipWhitespaceAndComments();

    if (isAtEnd()) {
        return Token(TokenKind::TOKEN_EOF, "", SourceLocation(line_, column_, filename_));
    }

    size_t startLine = line_;
    size_t startCol = column_;
    SourceLocation loc(startLine, startCol, filename_);
    char c = peek();

    if (c == '@') {
        return scanAnnotation();
    }

    if (std::isalpha(c) || c == '_') {
        return scanIdentifierOrKeyword();
    }

    if (std::isdigit(c)) {
        return scanNumber();
    }

    if (c == '"') {
        return scanString();
    }

    advance();

    switch (c) {
        case '+': return Token(TokenKind::PLUS, "+", loc);
        case '-': return Token(TokenKind::MINUS, "-", loc);
        case '*': return Token(TokenKind::STAR, "*", loc);
        case '/': return Token(TokenKind::SLASH, "/", loc);
        case '%': return Token(TokenKind::PERCENT, "%", loc);
        case '(': return Token(TokenKind::LPAREN, "(", loc);
        case ')': return Token(TokenKind::RPAREN, ")", loc);
        case '{': return Token(TokenKind::LBRACE, "{", loc);
        case '}': return Token(TokenKind::RBRACE, "}", loc);
        case ',': return Token(TokenKind::COMMA, ",", loc);
        case ';': return Token(TokenKind::SEMICOLON, ";", loc);

        case '=':
            if (match('=')) return Token(TokenKind::EQ_EQ, "==", loc);
            return Token(TokenKind::EQUAL, "=", loc);

        case '!':
            if (match('=')) return Token(TokenKind::BANG_EQ, "!=", loc);
            return Token(TokenKind::BANG, "!", loc);

        case '<':
            if (match('=')) return Token(TokenKind::LESS_EQ, "<=", loc);
            return Token(TokenKind::LESS, "<", loc);

        case '>':
            if (match('=')) return Token(TokenKind::GREATER_EQ, ">=", loc);
            return Token(TokenKind::GREATER, ">", loc);

        case '&':
            if (match('&')) return Token(TokenKind::AND_AND, "&&", loc);
            diag_.error("Unexpected character '&', did you mean '&&'?", loc);
            return Token(TokenKind::TOKEN_ERROR, "&", loc);

        case '|':
            if (match('|')) return Token(TokenKind::OR_OR, "||", loc);
            diag_.error("Unexpected character '|', did you mean '||'?", loc);
            return Token(TokenKind::TOKEN_ERROR, "|", loc);

        default:
            diag_.error(std::string("Unexpected character '") + c + "'", loc);
            return Token(TokenKind::TOKEN_ERROR, std::string(1, c), loc);
    }
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (true) {
        Token tok = nextToken();
        tokens.push_back(tok);
        if (tok.kind == TokenKind::TOKEN_EOF || tok.kind == TokenKind::TOKEN_ERROR) {
            break;
        }
    }
    return tokens;
}

} // namespace edge

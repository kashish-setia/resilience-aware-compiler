#include "semantic/SymbolTable.h"
#include <sstream>

namespace edge {

std::string Symbol::toString() const {
    std::ostringstream oss;
    oss << name << " (" << typeToString(type) << ", " << annotationToString(annotation)
        << ", " << sizeBytes << " bytes, decl: " << location.toString() << ")";
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

std::string Symbol::toJson() const {
    std::ostringstream oss;
    oss << "{"
        << "\"name\":\"" << escapeJson(name) << "\","
        << "\"type\":\"" << typeToString(type) << "\","
        << "\"annotation\":\"" << annotationToString(annotation) << "\","
        << "\"sizeBytes\":" << sizeBytes << ","
        << "\"statementId\":" << statementId << ","
        << "\"line\":" << location.line << ","
        << "\"column\":" << location.column << ","
        << "\"isExternalSensor\":" << (isExternalSensor ? "true" : "false")
        << "}";
    return oss.str();
}

SymbolTable::SymbolTable() {
    globalScope_ = std::make_shared<Scope>(nullptr);
    currentScope_ = globalScope_;
}

void SymbolTable::enterScope() {
    currentScope_ = std::make_shared<Scope>(currentScope_);
}

void SymbolTable::exitScope() {
    if (currentScope_->parent) {
        currentScope_ = currentScope_->parent;
    }
}

bool SymbolTable::define(Symbol sym) {
    if (!currentScope_->define(sym)) {
        return false;
    }
    allSymbols_.push_back(sym);
    return true;
}

Symbol* SymbolTable::lookup(const std::string& name) {
    return currentScope_->lookup(name);
}

const Symbol* SymbolTable::lookup(const std::string& name) const {
    return currentScope_->lookup(name);
}

std::string SymbolTable::toJson() const {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < allSymbols_.size(); ++i) {
        if (i > 0) oss << ",";
        oss << allSymbols_[i].toJson();
    }
    oss << "]";
    return oss.str();
}

} // namespace edge

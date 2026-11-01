#pragma once

#include "common/Types.h"
#include "common/Annotations.h"
#include "common/SourceLocation.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <optional>

namespace edge {

struct Symbol {
    std::string name;
    TypeKind type{TypeKind::UNKNOWN};
    AnnotationKind annotation{AnnotationKind::NONE};
    SourceLocation location;
    size_t statementId{0};
    size_t sizeBytes{4};
    bool isAssigned{false};
    bool isRead{false};
    bool isExternalSensor{false};

    std::string toString() const;
    std::string toJson() const;
};

class Scope {
public:
    std::shared_ptr<Scope> parent;
    std::unordered_map<std::string, Symbol> symbols;

    explicit Scope(std::shared_ptr<Scope> p = nullptr) : parent(std::move(p)) {}

    bool define(const Symbol& sym) {
        if (symbols.find(sym.name) != symbols.end()) {
            return false;
        }
        symbols[sym.name] = sym;
        return true;
    }

    Symbol* lookup(const std::string& name) {
        auto it = symbols.find(name);
        if (it != symbols.end()) return &it->second;
        if (parent) return parent->lookup(name);
        return nullptr;
    }
};

class SymbolTable {
private:
    std::shared_ptr<Scope> currentScope_;
    std::shared_ptr<Scope> globalScope_;
    std::vector<Symbol> allSymbols_;

public:
    SymbolTable();

    void enterScope();
    void exitScope();

    bool define(Symbol sym);
    Symbol* lookup(const std::string& name);
    const Symbol* lookup(const std::string& name) const;

    const std::vector<Symbol>& getAllSymbols() const { return allSymbols_; }
    std::vector<Symbol>& getAllSymbols() { return allSymbols_; }

    std::string toJson() const;
};

} // namespace edge

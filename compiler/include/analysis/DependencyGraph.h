#pragma once

#include "parser/AST.h"
#include "semantic/SymbolTable.h"
#include <string>
#include <vector>
#include <set>
#include <unordered_map>
#include <memory>

namespace edge {

struct DependencyNode {
    std::string name;
    TypeKind type{TypeKind::UNKNOWN};
    AnnotationKind annotation{AnnotationKind::NONE};
    bool isSensorRead{false};
    bool hasMultipleDefs{false};
    std::set<std::string> dependencies; // variables this depends on
    std::set<std::string> dependents;   // variables that depend on this
    const Expr* definitionExpr{nullptr};
    int recomputeCost{1}; // ALU cost estimate

    std::string toJson() const;
};

class DependencyGraph {
private:
    std::unordered_map<std::string, DependencyNode> nodes_;

    void extractDependencies(const Expr* expr, std::set<std::string>& deps, bool& hasSensorCall, int& cost);

public:
    DependencyGraph() = default;

    void build(const Program& program, const SymbolTable& symTable);

    const DependencyNode* getNode(const std::string& name) const;
    const std::unordered_map<std::string, DependencyNode>& getNodes() const { return nodes_; }

    bool canRecompute(const std::string& varName, const std::set<std::string>& savedSet, std::set<std::string>& visited) const;
    void collectTransitiveDependencies(const std::string& varName, const std::set<std::string>& savedSet, std::set<std::string>& needed, std::set<std::string>& visited) const;
    std::vector<std::string> getTopologicalRecomputeOrder(const std::set<std::string>& toRecompute) const;

    std::string toJson() const;
};

} // namespace edge

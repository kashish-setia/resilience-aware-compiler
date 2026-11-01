#include "analysis/DependencyGraph.h"
#include <sstream>
#include <algorithm>
#include <queue>
#include <functional>

namespace edge {

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

std::string DependencyNode::toJson() const {
    std::ostringstream oss;
    oss << "{\"name\":\"" << escapeJson(name) << "\","
        << "\"type\":\"" << typeToString(type) << "\","
        << "\"annotation\":\"" << annotationToString(annotation) << "\","
        << "\"isSensorRead\":" << (isSensorRead ? "true" : "false") << ","
        << "\"recomputeCost\":" << recomputeCost << ","
        << "\"dependencies\":[";
    size_t idx = 0;
    for (const auto& d : dependencies) {
        if (idx++ > 0) oss << ",";
        oss << "\"" << escapeJson(d) << "\"";
    }
    oss << "],\"dependents\":[";
    idx = 0;
    for (const auto& d : dependents) {
        if (idx++ > 0) oss << ",";
        oss << "\"" << escapeJson(d) << "\"";
    }
    oss << "]}";
    return oss.str();
}

void DependencyGraph::extractDependencies(const Expr* expr, std::set<std::string>& deps, bool& hasSensorCall, int& cost) {
    if (!expr) return;
    cost++;

    if (auto* var = dynamic_cast<const VariableExpr*>(expr)) {
        deps.insert(var->name);
    } else if (auto* bin = dynamic_cast<const BinaryExpr*>(expr)) {
        extractDependencies(bin->left.get(), deps, hasSensorCall, cost);
        extractDependencies(bin->right.get(), deps, hasSensorCall, cost);
    } else if (auto* un = dynamic_cast<const UnaryExpr*>(expr)) {
        extractDependencies(un->operand.get(), deps, hasSensorCall, cost);
    } else if (auto* call = dynamic_cast<const CallExpr*>(expr)) {
        if (call->callee == "read_sensor") {
            hasSensorCall = true;
        }
        for (const auto& a : call->args) {
            extractDependencies(a.get(), deps, hasSensorCall, cost);
        }
    }
}

void DependencyGraph::build(const Program& program, const SymbolTable& symTable) {
    nodes_.clear();

    // Initialize nodes from symbol table
    for (const auto& sym : symTable.getAllSymbols()) {
        DependencyNode node;
        node.name = sym.name;
        node.type = sym.type;
        node.annotation = sym.annotation;
        node.isSensorRead = sym.isExternalSensor;
        nodes_[sym.name] = node;
    }

    std::function<void(const Stmt*)> processStatement = [&](const Stmt* stmt) {
        if (!stmt) return;

        if (auto* decl = dynamic_cast<const VarDeclStmt*>(stmt)) {
            auto& node = nodes_[decl->name];
            if (node.definitionExpr != nullptr) {
                node.hasMultipleDefs = true;
            }
            if (decl->initializer) {
                node.definitionExpr = decl->initializer.get();
                bool hasSensor = false;
                int cost = 0;
                extractDependencies(decl->initializer.get(), node.dependencies, hasSensor, cost);
                if (hasSensor) node.isSensorRead = true;
                node.recomputeCost = cost;
            }
        } else if (auto* assign = dynamic_cast<const AssignStmt*>(stmt)) {
            auto& node = nodes_[assign->name];
            if (node.definitionExpr != nullptr) {
                node.hasMultipleDefs = true;
            }
            node.definitionExpr = assign->value.get();
            bool hasSensor = false;
            int cost = 0;
            extractDependencies(assign->value.get(), node.dependencies, hasSensor, cost);
            if (hasSensor) node.isSensorRead = true;
            node.recomputeCost = cost;
        } else if (auto* ifStmt = dynamic_cast<const IfStmt*>(stmt)) {
            if (ifStmt->thenBranch) processStatement(ifStmt->thenBranch.get());
            if (ifStmt->elseBranch) processStatement(ifStmt->elseBranch.get());
        } else if (auto* whileStmt = dynamic_cast<const WhileStmt*>(stmt)) {
            if (whileStmt->body) processStatement(whileStmt->body.get());
        } else if (auto* block = dynamic_cast<const BlockStmt*>(stmt)) {
            for (const auto& s : block->statements) {
                if (s) processStatement(s.get());
            }
        }
    };

    // Inspect statement ASTs to extract dependency edges
    for (const auto& stmt : program.statements) {
        if (stmt) processStatement(stmt.get());
    }

    // Connect dependents
    for (auto& pair : nodes_) {
        for (const auto& dep : pair.second.dependencies) {
            auto it = nodes_.find(dep);
            if (it != nodes_.end()) {
                it->second.dependents.insert(pair.first);
            }
        }
    }
}

const DependencyNode* DependencyGraph::getNode(const std::string& name) const {
    auto it = nodes_.find(name);
    if (it != nodes_.end()) return &it->second;
    return nullptr;
}

bool DependencyGraph::canRecompute(const std::string& varName, const std::set<std::string>& savedSet, std::set<std::string>& visited) const {
    // If it is in savedSet, it is directly available!
    if (savedSet.find(varName) != savedSet.end()) {
        return true;
    }

    auto* node = getNode(varName);
    if (!node) return false;

    // External sensor reads cannot be recomputed without hardware side effects
    if (node->isSensorRead) return false;

    // Variables with multiple reaching definitions (e.g. from conditional branches) cannot be safely recomputed
    if (node->hasMultipleDefs) return false;

    // Cycle detection
    if (visited.find(varName) != visited.end()) {
        return false;
    }
    visited.insert(varName);

    // If it has no definition expression or dependencies, it cannot be reconstructed unless saved
    if (!node->definitionExpr && node->dependencies.empty()) {
        return false;
    }

    // All input dependencies must be available in savedSet OR recomputable
    for (const auto& dep : node->dependencies) {
        if (savedSet.find(dep) == savedSet.end()) {
            if (!canRecompute(dep, savedSet, visited)) {
                return false;
            }
        }
    }

    return true;
}

void DependencyGraph::collectTransitiveDependencies(
    const std::string& varName,
    const std::set<std::string>& savedSet,
    std::set<std::string>& needed,
    std::set<std::string>& visited
) const {
    if (savedSet.find(varName) != savedSet.end()) return;
    if (visited.find(varName) != visited.end()) return;
    visited.insert(varName);

    auto* node = getNode(varName);
    if (!node) return;

    needed.insert(varName);

    for (const auto& dep : node->dependencies) {
        if (savedSet.find(dep) == savedSet.end()) {
            collectTransitiveDependencies(dep, savedSet, needed, visited);
        }
    }
}

std::vector<std::string> DependencyGraph::getTopologicalRecomputeOrder(const std::set<std::string>& toRecompute) const {
    // Kahn's algorithm or in-degree based topological sorting
    std::unordered_map<std::string, int> inDegree;
    std::unordered_map<std::string, std::vector<std::string>> adj;

    for (const auto& var : toRecompute) {
        inDegree[var] = 0;
    }

    for (const auto& var : toRecompute) {
        auto* node = getNode(var);
        if (node) {
            for (const auto& dep : node->dependencies) {
                if (toRecompute.find(dep) != toRecompute.end()) {
                    adj[dep].push_back(var);
                    inDegree[var]++;
                }
            }
        }
    }

    std::queue<std::string> q;
    for (const auto& pair : inDegree) {
        if (pair.second == 0) {
            q.push(pair.first);
        }
    }

    std::vector<std::string> order;
    while (!q.empty()) {
        std::string u = q.front();
        q.pop();
        order.push_back(u);

        for (const auto& v : adj[u]) {
            if (--inDegree[v] == 0) {
                q.push(v);
            }
        }
    }

    return order;
}

std::string DependencyGraph::toJson() const {
    std::ostringstream oss;
    oss << "{\"nodes\":[";
    bool first = true;
    for (const auto& pair : nodes_) {
        if (!first) oss << ",";
        first = false;
        oss << pair.second.toJson();
    }
    oss << "]}";
    return oss.str();
}

} // namespace edge

#include "classifier/StateClassifier.h"
#include <sstream>

namespace edge {

std::string stateClassToString(StateClass sc) {
    switch (sc) {
        case StateClass::CRITICAL: return "CRITICAL";
        case StateClass::RECOMPUTABLE: return "RECOMPUTABLE";
        case StateClass::DISPOSABLE: return "DISPOSABLE";
        case StateClass::OPTIONAL: return "OPTIONAL";
    }
    return "UNKNOWN";
}

StateClass stringToStateClass(const std::string& str) {
    if (str == "CRITICAL") return StateClass::CRITICAL;
    if (str == "RECOMPUTABLE") return StateClass::RECOMPUTABLE;
    if (str == "DISPOSABLE") return StateClass::DISPOSABLE;
    if (str == "OPTIONAL") return StateClass::OPTIONAL;
    return StateClass::DISPOSABLE;
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

std::string VariableClassification::toJson() const {
    std::ostringstream oss;
    oss << "{"
        << "\"name\":\"" << escapeJson(name) << "\","
        << "\"type\":\"" << typeToString(type) << "\","
        << "\"classification\":\"" << stateClassToString(classification) << "\","
        << "\"reason\":\"" << escapeJson(reason) << "\","
        << "\"isSelectedForCheckpoint\":" << (isSelectedForCheckpoint ? "true" : "false") << ","
        << "\"isRecomputable\":" << (isRecomputable ? "true" : "false") << ","
        << "\"storageSizeBytes\":" << storageSizeBytes << ","
        << "\"recomputeCost\":" << recomputeCost << ","
        << "\"annotation\":\"" << annotationToString(annotation) << "\","
        << "\"dependencies\":[";
    for (size_t i = 0; i < dependencies.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "\"" << escapeJson(dependencies[i]) << "\"";
    }
    oss << "]}";
    return oss.str();
}

StateClassifier::StateClassifier(const SymbolTable& symTable, const LivenessAnalysis& liveness, const DependencyGraph& depGraph)
    : symTable_(symTable), liveness_(liveness), depGraph_(depGraph) {}

std::vector<VariableClassification> StateClassifier::classifyVariablesAtCheckpoint(size_t checkpointId) const {
    std::set<std::string> liveVars = liveness_.getLiveVariablesAtCheckpoint(checkpointId);
    std::vector<VariableClassification> results;

    // First pass: identify variables that are definitively CRITICAL
    // (Explicit @critical, hardware sensor inputs, or live root inputs without dependencies)
    std::set<std::string> criticalSet;
    for (const auto& sym : symTable_.getAllSymbols()) {
        auto* node = depGraph_.getNode(sym.name);
        bool isLive = (liveVars.find(sym.name) != liveVars.end());

        if (sym.annotation == AnnotationKind::CRITICAL) {
            criticalSet.insert(sym.name);
        } else if (isLive && node && (node->isSensorRead || node->hasMultipleDefs)) {
            criticalSet.insert(sym.name);
        } else if (isLive && (!node || (node->dependencies.empty() && !node->definitionExpr))) {
            criticalSet.insert(sym.name);
        }
    }

    // Second pass: full classification with explainable decisions
    for (const auto& sym : symTable_.getAllSymbols()) {
        VariableClassification vc;
        vc.name = sym.name;
        vc.type = sym.type;
        vc.annotation = sym.annotation;
        vc.storageSizeBytes = getTypeSize(sym.type);

        auto* node = depGraph_.getNode(sym.name);
        if (node) {
            vc.dependencies.assign(node->dependencies.begin(), node->dependencies.end());
            vc.recomputeCost = node->recomputeCost;
        }

        bool isLive = (liveVars.find(sym.name) != liveVars.end());

        // 1. Explicit CRITICAL check
        if (sym.annotation == AnnotationKind::CRITICAL) {
            vc.classification = StateClass::CRITICAL;
            vc.reason = "Explicitly annotated @critical; programmer safety guarantee requires preservation in persistent NVM";
            vc.isSelectedForCheckpoint = true;
            vc.isRecomputable = false;
        }
        // 2. Explicit DISPOSABLE check
        else if (sym.annotation == AnnotationKind::DISPOSABLE) {
            vc.classification = StateClass::DISPOSABLE;
            vc.reason = "Explicitly annotated @disposable; volatile debug or temporary state discarded across checkpoints";
            vc.isSelectedForCheckpoint = false;
            vc.isRecomputable = false;
        }
        // 3. Explicit OPTIONAL check
        else if (sym.annotation == AnnotationKind::OPTIONAL) {
            vc.classification = StateClass::OPTIONAL;
            vc.reason = "Annotated @optional; auxiliary precomputed cache state. Can be saved if NVM storage budget permits or safely dropped";
            vc.isSelectedForCheckpoint = false; // By default dropped unless planner admits it
            vc.isRecomputable = false;
        }
        // 4. External hardware sensor read OR multiple conditional definitions
        else if (node && (node->isSensorRead || node->hasMultipleDefs) && isLive) {
            vc.classification = StateClass::CRITICAL;
            vc.reason = node->hasMultipleDefs ? 
                "Live state with multiple conditional control-flow definitions; cannot be reconstructed by static recomputation" :
                "External hardware sensor input; physical environment cannot be deterministically re-evaluated after a crash";
            vc.isSelectedForCheckpoint = true;
            vc.isRecomputable = false;
        }
        // 5. Dead variable check
        else if (!isLive) {
            vc.classification = StateClass::DISPOSABLE;
            vc.reason = "Dead variable at checkpoint point; never read downstream before reassignment or program termination";
            vc.isSelectedForCheckpoint = false;
            vc.isRecomputable = false;
        }
        // 6. RECOMPUTABLE vs CRITICAL check
        else {
            std::set<std::string> visited;
            bool canRec = depGraph_.canRecompute(sym.name, criticalSet, visited);
            if (canRec && node && node->definitionExpr) {
                vc.classification = StateClass::RECOMPUTABLE;
                vc.reason = "Pure deterministic expression safely reconstructible from saved critical inputs in topological dependency order";
                vc.isRecomputable = true;
                vc.isSelectedForCheckpoint = false;
            } else {
                vc.classification = StateClass::CRITICAL;
                vc.reason = "Live state required downstream that cannot be reconstructed from existing saved dependencies";
                vc.isSelectedForCheckpoint = true;
                vc.isRecomputable = false;
                criticalSet.insert(sym.name);
            }
        }

        results.push_back(vc);
    }

    return results;
}

std::string StateClassifier::toJson(const std::vector<VariableClassification>& classifications) const {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < classifications.size(); ++i) {
        if (i > 0) oss << ",";
        oss << classifications[i].toJson();
    }
    oss << "]";
    return oss.str();
}

} // namespace edge

#pragma once

#include "common/Types.h"
#include "common/Annotations.h"
#include "semantic/SymbolTable.h"
#include "analysis/LivenessAnalysis.h"
#include "analysis/DependencyGraph.h"
#include <string>
#include <vector>
#include <set>
#include <unordered_map>

namespace edge {

enum class StateClass {
    CRITICAL,
    RECOMPUTABLE,
    DISPOSABLE,
    OPTIONAL
};

std::string stateClassToString(StateClass sc);
StateClass stringToStateClass(const std::string& str);

struct VariableClassification {
    std::string name;
    TypeKind type{TypeKind::UNKNOWN};
    StateClass classification{StateClass::DISPOSABLE};
    std::string reason;
    bool isSelectedForCheckpoint{false};
    bool isRecomputable{false};
    size_t storageSizeBytes{4};
    int recomputeCost{0};
    std::vector<std::string> dependencies;
    AnnotationKind annotation{AnnotationKind::NONE};

    std::string toJson() const;
};

class StateClassifier {
private:
    const SymbolTable& symTable_;
    const LivenessAnalysis& liveness_;
    const DependencyGraph& depGraph_;

public:
    StateClassifier(const SymbolTable& symTable, const LivenessAnalysis& liveness, const DependencyGraph& depGraph);

    std::vector<VariableClassification> classifyVariablesAtCheckpoint(size_t checkpointId) const;
    std::string toJson(const std::vector<VariableClassification>& classifications) const;
};

} // namespace edge

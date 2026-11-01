#include "cfg/CFG.h"
#include <sstream>

namespace edge {

std::string cfgEdgeTypeToString(CFGEdgeType type) {
    switch (type) {
        case CFGEdgeType::FALLTHROUGH: return "fallthrough";
        case CFGEdgeType::TRUE_BRANCH: return "true_branch";
        case CFGEdgeType::FALSE_BRANCH: return "false_branch";
        case CFGEdgeType::LOOP_BACK: return "loop_back";
        case CFGEdgeType::LOOP_EXIT: return "loop_exit";
    }
    return "unknown";
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

std::string BasicBlock::toString() const {
    std::ostringstream oss;
    oss << "Block B" << id << " (" << label << "):\n";
    for (const auto* s : statements) {
        oss << "  " << s->toString(1) << "\n";
    }
    oss << "  [USE: ";
    for (const auto& u : use) oss << u << " ";
    oss << "| DEF: ";
    for (const auto& d : def) oss << d << " ";
    oss << "| LIVE_IN: ";
    for (const auto& li : liveIn) oss << li << " ";
    oss << "| LIVE_OUT: ";
    for (const auto& lo : liveOut) oss << lo << " ";
    oss << "]\n";
    return oss.str();
}

std::string BasicBlock::toJson() const {
    std::ostringstream oss;
    oss << "{"
        << "\"id\":" << id << ","
        << "\"label\":\"" << escapeJson(label) << "\","
        << "\"hasCheckpoint\":" << (hasCheckpoint() ? "true" : "false") << ","
        << "\"statements\":[";
    for (size_t i = 0; i < statements.size(); ++i) {
        if (i > 0) oss << ",";
        oss << statements[i]->toJson();
    }
    oss << "],\"predecessors\":[";
    for (size_t i = 0; i < predecessors.size(); ++i) {
        if (i > 0) oss << ",";
        oss << predecessors[i];
    }
    oss << "],\"successors\":[";
    for (size_t i = 0; i < successors.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "{\"target\":" << successors[i].targetBlockId
            << ",\"type\":\"" << cfgEdgeTypeToString(successors[i].type) << "\"}";
    }
    oss << "],\"use\":[";
    size_t idx = 0;
    for (const auto& s : use) {
        if (idx++ > 0) oss << ",";
        oss << "\"" << escapeJson(s) << "\"";
    }
    oss << "],\"def\":[";
    idx = 0;
    for (const auto& s : def) {
        if (idx++ > 0) oss << ",";
        oss << "\"" << escapeJson(s) << "\"";
    }
    oss << "],\"liveIn\":[";
    idx = 0;
    for (const auto& s : liveIn) {
        if (idx++ > 0) oss << ",";
        oss << "\"" << escapeJson(s) << "\"";
    }
    oss << "],\"liveOut\":[";
    idx = 0;
    for (const auto& s : liveOut) {
        if (idx++ > 0) oss << ",";
        oss << "\"" << escapeJson(s) << "\"";
    }
    oss << "]}";
    return oss.str();
}

std::shared_ptr<BasicBlock> CFG::createBlock(const std::string& label) {
    size_t newId = blocks_.size();
    auto block = std::make_shared<BasicBlock>(newId, label);
    blocks_.push_back(block);
    return block;
}

std::shared_ptr<BasicBlock> CFG::getBlock(size_t id) const {
    if (id < blocks_.size()) return blocks_[id];
    return nullptr;
}

void CFG::addEdge(size_t fromId, size_t toId, CFGEdgeType type) {
    if (fromId < blocks_.size() && toId < blocks_.size()) {
        blocks_[fromId]->addSuccessor(toId, type);
    }
}

void CFG::finalizePredecessors() {
    for (auto& b : blocks_) {
        b->predecessors.clear();
    }
    for (const auto& b : blocks_) {
        for (const auto& edge : b->successors) {
            if (edge.targetBlockId < blocks_.size()) {
                blocks_[edge.targetBlockId]->predecessors.push_back(b->id);
            }
        }
    }
}

std::string CFG::toDot() const {
    std::ostringstream oss;
    oss << "digraph CFG {\n";
    oss << "  node [shape=box, style=filled, fillcolor=\"#f0f4f8\", fontname=\"Helvetica\"];\n";
    oss << "  edge [fontname=\"Helvetica\", fontsize=10];\n";

    for (const auto& b : blocks_) {
        std::string fill = (b->hasCheckpoint()) ? "#e6fffa" : "#f0f4f8";
        oss << "  B" << b->id << " [label=\"B" << b->id << ": " << b->label;
        if (b->hasCheckpoint()) {
            oss << "\\n[CHECKPOINT]";
        }
        oss << "\", fillcolor=\"" << fill << "\"];\n";
    }

    for (const auto& b : blocks_) {
        for (const auto& edge : b->successors) {
            oss << "  B" << b->id << " -> B" << edge.targetBlockId
                << " [label=\"" << cfgEdgeTypeToString(edge.type) << "\"];\n";
        }
    }

    oss << "}\n";
    return oss.str();
}

std::string CFG::toJson() const {
    std::ostringstream oss;
    oss << "{\"entryBlockId\":" << entryBlockId_
        << ",\"exitBlockId\":" << exitBlockId_
        << ",\"blocks\":[";
    for (size_t i = 0; i < blocks_.size(); ++i) {
        if (i > 0) oss << ",";
        oss << blocks_[i]->toJson();
    }
    oss << "]}";
    return oss.str();
}

} // namespace edge

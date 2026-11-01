#include "parser/AST.h"
#include <sstream>

namespace edge {

std::string binaryOpToString(BinaryOp op) {
    switch (op) {
        case BinaryOp::ADD: return "+";
        case BinaryOp::SUB: return "-";
        case BinaryOp::MUL: return "*";
        case BinaryOp::DIV: return "/";
        case BinaryOp::MOD: return "%";
        case BinaryOp::EQ: return "==";
        case BinaryOp::NEQ: return "!=";
        case BinaryOp::LT: return "<";
        case BinaryOp::LTE: return "<=";
        case BinaryOp::GT: return ">";
        case BinaryOp::GTE: return ">=";
        case BinaryOp::AND: return "&&";
        case BinaryOp::OR: return "||";
    }
    return "?";
}

std::string unaryOpToString(UnaryOp op) {
    switch (op) {
        case UnaryOp::NEG: return "-";
        case UnaryOp::NOT: return "!";
    }
    return "?";
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

// Accept implementations
void LiteralExpr::accept(ASTVisitor& v) { v.visit(*this); }
void VariableExpr::accept(ASTVisitor& v) { v.visit(*this); }
void BinaryExpr::accept(ASTVisitor& v) { v.visit(*this); }
void UnaryExpr::accept(ASTVisitor& v) { v.visit(*this); }
void CallExpr::accept(ASTVisitor& v) { v.visit(*this); }

void VarDeclStmt::accept(ASTVisitor& v) { v.visit(*this); }
void AssignStmt::accept(ASTVisitor& v) { v.visit(*this); }
void BlockStmt::accept(ASTVisitor& v) { v.visit(*this); }
void IfStmt::accept(ASTVisitor& v) { v.visit(*this); }
void WhileStmt::accept(ASTVisitor& v) { v.visit(*this); }
void PrintStmt::accept(ASTVisitor& v) { v.visit(*this); }
void CheckpointStmt::accept(ASTVisitor& v) { v.visit(*this); }
void FaultStmt::accept(ASTVisitor& v) { v.visit(*this); }
void Program::accept(ASTVisitor& v) { v.visit(*this); }

// String and JSON representations
std::string LiteralExpr::toString(int indent) const {
    return makeIndent(indent) + "Literal(" + value.asString() + ")";
}

std::string LiteralExpr::toJson() const {
    std::ostringstream oss;
    oss << "{\"node\":\"LiteralExpr\",\"value\":" << value.toJson()
        << ",\"type\":\"" << typeToString(evaluatedType) << "\"}";
    return oss.str();
}

std::string VariableExpr::toString(int indent) const {
    return makeIndent(indent) + "Variable(" + name + ")";
}

std::string VariableExpr::toJson() const {
    std::ostringstream oss;
    oss << "{\"node\":\"VariableExpr\",\"name\":\"" << escapeJson(name)
        << "\",\"type\":\"" << typeToString(evaluatedType) << "\"}";
    return oss.str();
}

std::string BinaryExpr::toString(int indent) const {
    std::ostringstream oss;
    oss << makeIndent(indent) << "BinaryExpr(" << binaryOpToString(op) << ")\n"
        << left->toString(indent + 1) << "\n"
        << right->toString(indent + 1);
    return oss.str();
}

std::string BinaryExpr::toJson() const {
    std::ostringstream oss;
    oss << "{\"node\":\"BinaryExpr\",\"op\":\"" << binaryOpToString(op) << "\","
        << "\"left\":" << left->toJson() << ","
        << "\"right\":" << right->toJson() << ","
        << "\"type\":\"" << typeToString(evaluatedType) << "\"}";
    return oss.str();
}

std::string UnaryExpr::toString(int indent) const {
    std::ostringstream oss;
    oss << makeIndent(indent) << "UnaryExpr(" << unaryOpToString(op) << ")\n"
        << operand->toString(indent + 1);
    return oss.str();
}

std::string UnaryExpr::toJson() const {
    std::ostringstream oss;
    oss << "{\"node\":\"UnaryExpr\",\"op\":\"" << unaryOpToString(op) << "\","
        << "\"operand\":" << operand->toJson() << ","
        << "\"type\":\"" << typeToString(evaluatedType) << "\"}";
    return oss.str();
}

std::string CallExpr::toString(int indent) const {
    std::ostringstream oss;
    oss << makeIndent(indent) << "CallExpr(" << callee << ")\n";
    for (const auto& a : args) {
        oss << a->toString(indent + 1) << "\n";
    }
    return oss.str();
}

std::string CallExpr::toJson() const {
    std::ostringstream oss;
    oss << "{\"node\":\"CallExpr\",\"callee\":\"" << escapeJson(callee) << "\",\"args\":[";
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) oss << ",";
        oss << args[i]->toJson();
    }
    oss << "],\"type\":\"" << typeToString(evaluatedType) << "\"}";
    return oss.str();
}

std::string VarDeclStmt::toString(int indent) const {
    std::ostringstream oss;
    oss << makeIndent(indent) << "VarDecl(" << annotationToString(annotation) << " "
        << typeToString(type) << " " << name << ")";
    if (initializer) {
        oss << " =\n" << initializer->toString(indent + 1);
    }
    return oss.str();
}

std::string VarDeclStmt::toJson() const {
    std::ostringstream oss;
    oss << "{\"node\":\"VarDeclStmt\",\"name\":\"" << escapeJson(name) << "\","
        << "\"type\":\"" << typeToString(type) << "\","
        << "\"annotation\":\"" << annotationToString(annotation) << "\","
        << "\"statementId\":" << statementId << ","
        << "\"initializer\":" << (initializer ? initializer->toJson() : "null") << "}";
    return oss.str();
}

std::string AssignStmt::toString(int indent) const {
    std::ostringstream oss;
    oss << makeIndent(indent) << "Assign(" << name << ") =\n" << value->toString(indent + 1);
    return oss.str();
}

std::string AssignStmt::toJson() const {
    std::ostringstream oss;
    oss << "{\"node\":\"AssignStmt\",\"name\":\"" << escapeJson(name) << "\","
        << "\"statementId\":" << statementId << ","
        << "\"value\":" << value->toJson() << "}";
    return oss.str();
}

std::string BlockStmt::toString(int indent) const {
    std::ostringstream oss;
    oss << makeIndent(indent) << "Block {\n";
    for (const auto& s : statements) {
        oss << s->toString(indent + 1) << "\n";
    }
    oss << makeIndent(indent) << "}";
    return oss.str();
}

std::string BlockStmt::toJson() const {
    std::ostringstream oss;
    oss << "{\"node\":\"BlockStmt\",\"statementId\":" << statementId << ",\"statements\":[";
    for (size_t i = 0; i < statements.size(); ++i) {
        if (i > 0) oss << ",";
        oss << statements[i]->toJson();
    }
    oss << "]}";
    return oss.str();
}

std::string IfStmt::toString(int indent) const {
    std::ostringstream oss;
    oss << makeIndent(indent) << "If (\n" << condition->toString(indent + 1) << "\n"
        << makeIndent(indent) << ") Then\n" << thenBranch->toString(indent + 1);
    if (elseBranch) {
        oss << "\n" << makeIndent(indent) << "Else\n" << elseBranch->toString(indent + 1);
    }
    return oss.str();
}

std::string IfStmt::toJson() const {
    std::ostringstream oss;
    oss << "{\"node\":\"IfStmt\",\"statementId\":" << statementId << ","
        << "\"condition\":" << condition->toJson() << ","
        << "\"thenBranch\":" << thenBranch->toJson() << ","
        << "\"elseBranch\":" << (elseBranch ? elseBranch->toJson() : "null") << "}";
    return oss.str();
}

std::string WhileStmt::toString(int indent) const {
    std::ostringstream oss;
    oss << makeIndent(indent) << "While (\n" << condition->toString(indent + 1) << "\n"
        << makeIndent(indent) << ") Body\n" << body->toString(indent + 1);
    return oss.str();
}

std::string WhileStmt::toJson() const {
    std::ostringstream oss;
    oss << "{\"node\":\"WhileStmt\",\"statementId\":" << statementId << ","
        << "\"condition\":" << condition->toJson() << ","
        << "\"body\":" << body->toJson() << "}";
    return oss.str();
}

std::string PrintStmt::toString(int indent) const {
    std::ostringstream oss;
    oss << makeIndent(indent) << "Print(\n" << expression->toString(indent + 1) << "\n"
        << makeIndent(indent) << ")";
    return oss.str();
}

std::string PrintStmt::toJson() const {
    std::ostringstream oss;
    oss << "{\"node\":\"PrintStmt\",\"statementId\":" << statementId << ","
        << "\"expression\":" << expression->toJson() << "}";
    return oss.str();
}

std::string CheckpointStmt::toString(int indent) const {
    return makeIndent(indent) + "Checkpoint(" + label + ", id=" + std::to_string(checkpointId) + ")";
}

std::string CheckpointStmt::toJson() const {
    std::ostringstream oss;
    oss << "{\"node\":\"CheckpointStmt\",\"statementId\":" << statementId << ","
        << "\"label\":\"" << escapeJson(label) << "\","
        << "\"checkpointId\":" << checkpointId << "}";
    return oss.str();
}

std::string FaultStmt::toString(int indent) const {
    return makeIndent(indent) + "Fault(" + reason + ")";
}

std::string FaultStmt::toJson() const {
    std::ostringstream oss;
    oss << "{\"node\":\"FaultStmt\",\"statementId\":" << statementId << ","
        << "\"reason\":\"" << escapeJson(reason) << "\"}";
    return oss.str();
}

std::string Program::toString(int indent) const {
    std::ostringstream oss;
    oss << makeIndent(indent) << "Program(" << name << ") {\n";
    for (const auto& s : statements) {
        oss << s->toString(indent + 1) << "\n";
    }
    oss << makeIndent(indent) << "}";
    return oss.str();
}

std::string Program::toJson() const {
    std::ostringstream oss;
    oss << "{\"node\":\"Program\",\"name\":\"" << escapeJson(name) << "\",\"statements\":[";
    for (size_t i = 0; i < statements.size(); ++i) {
        if (i > 0) oss << ",";
        oss << statements[i]->toJson();
    }
    oss << "]}";
    return oss.str();
}

} // namespace edge

#include "ir/Bytecode.h"
#include <sstream>
#include <iomanip>

namespace edge {

std::string opCodeToString(OpCode op) {
    switch (op) {
        case OpCode::OP_CONST: return "CONST";
        case OpCode::OP_LOAD_VAR: return "LOAD_VAR";
        case OpCode::OP_STORE_VAR: return "STORE_VAR";
        case OpCode::OP_ADD: return "ADD";
        case OpCode::OP_SUB: return "SUB";
        case OpCode::OP_MUL: return "MUL";
        case OpCode::OP_DIV: return "DIV";
        case OpCode::OP_MOD: return "MOD";
        case OpCode::OP_EQ: return "EQ";
        case OpCode::OP_NEQ: return "NEQ";
        case OpCode::OP_LT: return "LT";
        case OpCode::OP_LTE: return "LTE";
        case OpCode::OP_GT: return "GT";
        case OpCode::OP_GTE: return "GTE";
        case OpCode::OP_AND: return "AND";
        case OpCode::OP_OR: return "OR";
        case OpCode::OP_NOT: return "NOT";
        case OpCode::OP_NEG: return "NEG";
        case OpCode::OP_JUMP: return "JUMP";
        case OpCode::OP_JUMP_IF_FALSE: return "JUMP_IF_FALSE";
        case OpCode::OP_PRINT: return "PRINT";
        case OpCode::OP_READ_SENSOR: return "READ_SENSOR";
        case OpCode::OP_CALL_BUILTIN: return "CALL_BUILTIN";
        case OpCode::OP_CHECKPOINT: return "CHECKPOINT";
        case OpCode::OP_FAULT: return "FAULT";
        case OpCode::OP_HALT: return "HALT";
    }
    return "UNKNOWN";
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

std::string Instruction::toString(size_t index) const {
    std::ostringstream oss;
    oss << std::setw(4) << std::setfill('0') << index << ": "
        << std::setw(15) << std::setfill(' ') << std::left << opCodeToString(op);

    if (!operand.isVoid()) {
        oss << " " << operand.asString();
    }
    if (!symbol.empty()) {
        oss << " " << symbol;
    }
    if (targetAddress >= 0) {
        oss << " -> [" << std::setw(4) << std::setfill('0') << targetAddress << "]";
    }
    return oss.str();
}

std::string Instruction::toJson(size_t index) const {
    std::ostringstream oss;
    oss << "{\"address\":" << index
        << ",\"op\":\"" << opCodeToString(op) << "\""
        << ",\"operand\":" << operand.toJson()
        << ",\"symbol\":\"" << escapeJson(symbol) << "\""
        << ",\"targetAddress\":" << targetAddress
        << ",\"statementId\":" << statementId
        << "}";
    return oss.str();
}

std::string BytecodeProgram::disassemble() const {
    std::ostringstream oss;
    oss << "=== Disassembly: " << programName_ << " ===\n";
    for (size_t i = 0; i < instructions_.size(); ++i) {
        oss << instructions_[i].toString(i) << "\n";
    }
    return oss.str();
}

std::string BytecodeProgram::toJson() const {
    std::ostringstream oss;
    oss << "{\"programName\":\"" << escapeJson(programName_) << "\",\"instructions\":[";
    for (size_t i = 0; i < instructions_.size(); ++i) {
        if (i > 0) oss << ",";
        oss << instructions_[i].toJson(i);
    }
    oss << "]}";
    return oss.str();
}

} // namespace edge

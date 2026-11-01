#pragma once

#include "common/Value.h"
#include <string>
#include <vector>
#include <cstdint>

namespace edge {

enum class OpCode : uint8_t {
    OP_CONST,
    OP_LOAD_VAR,
    OP_STORE_VAR,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_EQ,
    OP_NEQ,
    OP_LT,
    OP_LTE,
    OP_GT,
    OP_GTE,
    OP_AND,
    OP_OR,
    OP_NOT,
    OP_NEG,
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_PRINT,
    OP_READ_SENSOR,
    OP_CALL_BUILTIN,
    OP_CHECKPOINT,
    OP_FAULT,
    OP_HALT
};

std::string opCodeToString(OpCode op);

struct Instruction {
    OpCode op;
    Value operand;
    std::string symbol;
    int32_t targetAddress{-1};
    size_t statementId{0};

    Instruction(OpCode op, Value val = Value(), std::string sym = "", int32_t target = -1, size_t stmtId = 0)
        : op(op), operand(std::move(val)), symbol(std::move(sym)), targetAddress(target), statementId(stmtId) {}

    std::string toString(size_t index) const;
    std::string toJson(size_t index) const;
};

class BytecodeProgram {
private:
    std::vector<Instruction> instructions_;
    std::string programName_;

public:
    explicit BytecodeProgram(std::string name = "main") : programName_(std::move(name)) {}

    size_t addInstruction(Instruction instr) {
        size_t addr = instructions_.size();
        instructions_.push_back(std::move(instr));
        return addr;
    }

    Instruction& getInstruction(size_t index) { return instructions_[index]; }
    const Instruction& getInstruction(size_t index) const { return instructions_[index]; }
    const std::vector<Instruction>& getInstructions() const { return instructions_; }
    size_t size() const { return instructions_.size(); }

    void patchJump(size_t instrIndex, int32_t targetAddress) {
        if (instrIndex < instructions_.size()) {
            instructions_[instrIndex].targetAddress = targetAddress;
        }
    }

    std::string disassemble() const;
    std::string toJson() const;
};

} // namespace edge

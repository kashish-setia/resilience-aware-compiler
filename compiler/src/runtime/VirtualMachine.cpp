#include "runtime/VirtualMachine.h"
#include <cmath>
#include <iostream>

namespace edge {

VirtualMachine::VirtualMachine() {
    // Default sensor mock values
    sensorMocks_[0] = Value(25);  // Temp raw
    sensorMocks_[1] = Value(72);  // Heart rate
    sensorMocks_[2] = Value(1013); // Pressure
    sensorMocks_[3] = Value(50);  // Humidity
}

void VirtualMachine::load(const BytecodeProgram* program) {
    program_ = program;
    reset();
}

void VirtualMachine::reset() {
    ip_ = 0;
    state_ = (program_ && program_->size() > 0) ? VMState::READY : VMState::HALTED;
    stack_.clear();
    variables_.clear();
    output_.clear();
    executionTrace_.clear();
    cycleCount_ = 0;
    lastCheckpointId_ = 0;
    lastCheckpointIp_ = 0;
    lastFaultReason_ = "";
}

void VirtualMachine::setSensorMock(int64_t channel, Value val) {
    sensorMocks_[channel] = std::move(val);
}

void VirtualMachine::setVariable(const std::string& name, Value val) {
    variables_[name] = std::move(val);
}

Value VirtualMachine::getVariable(const std::string& name) const {
    auto it = variables_.find(name);
    if (it != variables_.end()) return it->second;
    return Value();
}

void VirtualMachine::push(Value val) {
    stack_.push_back(std::move(val));
}

Value VirtualMachine::pop() {
    if (stack_.empty()) {
        throw std::runtime_error("VM Stack underflow at IP=" + std::to_string(ip_));
    }
    Value val = stack_.back();
    stack_.pop_back();
    return val;
}

Value VirtualMachine::peek() const {
    if (stack_.empty()) {
        throw std::runtime_error("VM Stack underflow peek at IP=" + std::to_string(ip_));
    }
    return stack_.back();
}

VMState VirtualMachine::step() {
    if (!program_ || ip_ < 0 || static_cast<size_t>(ip_) >= program_->size()) {
        state_ = VMState::HALTED;
        return state_;
    }

    const Instruction& instr = program_->getInstruction(static_cast<size_t>(ip_));
    cycleCount_++;

    ExecutionStep stepInfo;
    stepInfo.stepNumber = cycleCount_;
    stepInfo.ip = ip_;
    stepInfo.instructionStr = instr.toString(ip_);

    int32_t nextIp = ip_ + 1;

    switch (instr.op) {
        case OpCode::OP_CONST:
            push(instr.operand);
            stepInfo.actionDescription = "Pushed constant " + instr.operand.asString();
            break;

        case OpCode::OP_LOAD_VAR: {
            Value val = getVariable(instr.symbol);
            push(val);
            stepInfo.actionDescription = "Loaded variable '" + instr.symbol + "' = " + val.asString();
            break;
        }

        case OpCode::OP_STORE_VAR: {
            Value val = pop();
            setVariable(instr.symbol, val);
            stepInfo.actionDescription = "Stored variable '" + instr.symbol + "' = " + val.asString();
            break;
        }

        case OpCode::OP_ADD: {
            Value b = pop();
            Value a = pop();
            if (a.isFloat() || b.isFloat()) {
                push(Value(a.asFloat() + b.asFloat()));
            } else {
                push(Value(a.asInt() + b.asInt()));
            }
            stepInfo.actionDescription = "Evaluated ADD -> " + peek().asString();
            break;
        }

        case OpCode::OP_SUB: {
            Value b = pop();
            Value a = pop();
            if (a.isFloat() || b.isFloat()) {
                push(Value(a.asFloat() - b.asFloat()));
            } else {
                push(Value(a.asInt() - b.asInt()));
            }
            stepInfo.actionDescription = "Evaluated SUB -> " + peek().asString();
            break;
        }

        case OpCode::OP_MUL: {
            Value b = pop();
            Value a = pop();
            if (a.isFloat() || b.isFloat()) {
                push(Value(a.asFloat() * b.asFloat()));
            } else {
                push(Value(a.asInt() * b.asInt()));
            }
            stepInfo.actionDescription = "Evaluated MUL -> " + peek().asString();
            break;
        }

        case OpCode::OP_DIV: {
            Value b = pop();
            Value a = pop();
            if (b.asFloat() == 0.0) {
                push(Value(0.0)); // Edge zero-division safeguard
            } else if (a.isFloat() || b.isFloat()) {
                push(Value(a.asFloat() / b.asFloat()));
            } else {
                push(Value(a.asInt() / b.asInt()));
            }
            stepInfo.actionDescription = "Evaluated DIV -> " + peek().asString();
            break;
        }

        case OpCode::OP_MOD: {
            Value b = pop();
            Value a = pop();
            int64_t den = b.asInt();
            push(Value((den != 0) ? (a.asInt() % den) : 0));
            stepInfo.actionDescription = "Evaluated MOD -> " + peek().asString();
            break;
        }

        case OpCode::OP_EQ: {
            Value b = pop();
            Value a = pop();
            push(Value(a == b));
            stepInfo.actionDescription = "Evaluated EQ -> " + peek().asString();
            break;
        }

        case OpCode::OP_NEQ: {
            Value b = pop();
            Value a = pop();
            push(Value(a != b));
            stepInfo.actionDescription = "Evaluated NEQ -> " + peek().asString();
            break;
        }

        case OpCode::OP_LT: {
            Value b = pop();
            Value a = pop();
            push(Value(a.asFloat() < b.asFloat()));
            stepInfo.actionDescription = "Evaluated LT -> " + peek().asString();
            break;
        }

        case OpCode::OP_LTE: {
            Value b = pop();
            Value a = pop();
            push(Value(a.asFloat() <= b.asFloat()));
            stepInfo.actionDescription = "Evaluated LTE -> " + peek().asString();
            break;
        }

        case OpCode::OP_GT: {
            Value b = pop();
            Value a = pop();
            push(Value(a.asFloat() > b.asFloat()));
            stepInfo.actionDescription = "Evaluated GT -> " + peek().asString();
            break;
        }

        case OpCode::OP_GTE: {
            Value b = pop();
            Value a = pop();
            push(Value(a.asFloat() >= b.asFloat()));
            stepInfo.actionDescription = "Evaluated GTE -> " + peek().asString();
            break;
        }

        case OpCode::OP_AND: {
            Value b = pop();
            Value a = pop();
            push(Value(a.asBool() && b.asBool()));
            stepInfo.actionDescription = "Evaluated AND -> " + peek().asString();
            break;
        }

        case OpCode::OP_OR: {
            Value b = pop();
            Value a = pop();
            push(Value(a.asBool() || b.asBool()));
            stepInfo.actionDescription = "Evaluated OR -> " + peek().asString();
            break;
        }

        case OpCode::OP_NOT: {
            Value a = pop();
            push(Value(!a.asBool()));
            stepInfo.actionDescription = "Evaluated NOT -> " + peek().asString();
            break;
        }

        case OpCode::OP_NEG: {
            Value a = pop();
            if (a.isFloat()) push(Value(-a.asFloat()));
            else push(Value(-a.asInt()));
            stepInfo.actionDescription = "Evaluated NEG -> " + peek().asString();
            break;
        }

        case OpCode::OP_JUMP: {
            nextIp = instr.targetAddress;
            stepInfo.actionDescription = "Jumped unconditionally to [" + std::to_string(nextIp) + "]";
            break;
        }

        case OpCode::OP_JUMP_IF_FALSE: {
            Value cond = pop();
            if (!cond.asBool()) {
                nextIp = instr.targetAddress;
                stepInfo.actionDescription = "Condition FALSE: Jumped to [" + std::to_string(nextIp) + "]";
            } else {
                stepInfo.actionDescription = "Condition TRUE: Continued to next instruction";
            }
            break;
        }

        case OpCode::OP_PRINT: {
            Value val = pop();
            std::string outStr = val.asString();
            output_.push_back(outStr);
            stepInfo.actionDescription = "Emitted output: '" + outStr + "'";
            break;
        }

        case OpCode::OP_READ_SENSOR: {
            Value chVal = pop();
            int64_t channel = chVal.asInt();
            auto it = sensorMocks_.find(channel);
            Value sensorReading = (it != sensorMocks_.end()) ? it->second : Value(0);
            push(sensorReading);
            stepInfo.actionDescription = "Read sensor channel " + std::to_string(channel) + " = " + sensorReading.asString();
            break;
        }

        case OpCode::OP_CALL_BUILTIN: {
            int64_t argCount = instr.operand.asInt();
            std::string funcName = instr.symbol;
            if (funcName == "abs" && argCount == 1) {
                Value x = pop();
                if (x.isFloat()) push(Value(std::fabs(x.asFloat())));
                else push(Value(std::abs(x.asInt())));
            } else if (funcName == "min" && argCount == 2) {
                Value b = pop();
                Value a = pop();
                if (a.isFloat() || b.isFloat()) push(Value(std::min(a.asFloat(), b.asFloat())));
                else push(Value(std::min(a.asInt(), b.asInt())));
            } else if (funcName == "max" && argCount == 2) {
                Value b = pop();
                Value a = pop();
                if (a.isFloat() || b.isFloat()) push(Value(std::max(a.asFloat(), b.asFloat())));
                else push(Value(std::max(a.asInt(), b.asInt())));
            }
            stepInfo.actionDescription = "Invoked builtin " + funcName + " -> " + peek().asString();
            break;
        }

        case OpCode::OP_CHECKPOINT: {
            lastCheckpointId_ = static_cast<size_t>(instr.operand.asInt());
            lastCheckpointIp_ = nextIp; // resume from statement following checkpoint
            stepInfo.actionDescription = "Encountered checkpoint '" + instr.symbol + "' (id=" + std::to_string(lastCheckpointId_) + ")";
            state_ = VMState::AT_CHECKPOINT;
            ip_ = nextIp;
            executionTrace_.push_back(stepInfo);
            return state_;
        }

        case OpCode::OP_FAULT: {
            lastFaultReason_ = instr.operand.asString();
            stepInfo.actionDescription = "FAULT INJECTED: " + lastFaultReason_;
            state_ = VMState::FAULT_INJECTED;
            ip_ = nextIp;
            executionTrace_.push_back(stepInfo);
            return state_;
        }

        case OpCode::OP_HALT: {
            state_ = VMState::HALTED;
            stepInfo.actionDescription = "Program completed execution";
            executionTrace_.push_back(stepInfo);
            return state_;
        }
    }

    executionTrace_.push_back(stepInfo);
    ip_ = nextIp;

    if (ip_ >= static_cast<int32_t>(program_->size())) {
        state_ = VMState::HALTED;
    } else {
        state_ = VMState::RUNNING;
    }
    return state_;
}

VMState VirtualMachine::run(size_t maxCycles) {
    state_ = VMState::RUNNING;
    size_t count = 0;
    while (state_ == VMState::RUNNING && count++ < maxCycles) {
        step();
        if (state_ == VMState::AT_CHECKPOINT || state_ == VMState::FAULT_INJECTED || state_ == VMState::HALTED) {
            break;
        }
    }
    return state_;
}

void VirtualMachine::clearVolatileState() {
    stack_.clear();
    variables_.clear();
    output_.clear();
    ip_ = 0;
}

void VirtualMachine::restoreState(const std::unordered_map<std::string, Value>& savedVars, int32_t resumeIp) {
    clearVolatileState();
    variables_ = savedVars;
    ip_ = resumeIp;
    state_ = (ip_ < static_cast<int32_t>(program_->size())) ? VMState::RUNNING : VMState::HALTED;
}

} // namespace edge

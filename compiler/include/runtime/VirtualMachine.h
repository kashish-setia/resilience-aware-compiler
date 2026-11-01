#pragma once

#include "ir/Bytecode.h"
#include "common/Value.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>

namespace edge {

enum class VMState {
    READY,
    RUNNING,
    HALTED,
    AT_CHECKPOINT,
    FAULT_INJECTED,
    ERROR
};

struct ExecutionStep {
    size_t stepNumber;
    int32_t ip;
    std::string instructionStr;
    std::string actionDescription;
};

class VirtualMachine {
private:
    const BytecodeProgram* program_{nullptr};
    int32_t ip_{0};
    VMState state_{VMState::READY};
    std::vector<Value> stack_;
    std::unordered_map<std::string, Value> variables_;
    std::unordered_map<int64_t, Value> sensorMocks_;
    std::vector<std::string> output_;
    std::vector<ExecutionStep> executionTrace_;
    size_t cycleCount_{0};
    size_t lastCheckpointId_{0};
    int32_t lastCheckpointIp_{0};
    std::string lastFaultReason_;

    void push(Value val);
    Value pop();
    Value peek() const;

public:
    VirtualMachine();

    void load(const BytecodeProgram* program);
    void setSensorMock(int64_t channel, Value val);
    void setVariable(const std::string& name, Value val);
    Value getVariable(const std::string& name) const;
    const std::unordered_map<std::string, Value>& getVariables() const { return variables_; }

    VMState step();
    VMState run(size_t maxCycles = 100000);

    VMState getState() const { return state_; }
    int32_t getIP() const { return ip_; }
    size_t getCycles() const { return cycleCount_; }
    size_t getLastCheckpointId() const { return lastCheckpointId_; }
    int32_t getLastCheckpointIp() const { return lastCheckpointIp_; }
    const std::string& getLastFaultReason() const { return lastFaultReason_; }
    const std::vector<std::string>& getOutput() const { return output_; }
    const std::vector<ExecutionStep>& getExecutionTrace() const { return executionTrace_; }

    void clearVolatileState();
    void restoreState(const std::unordered_map<std::string, Value>& savedVars, int32_t resumeIp);

    void reset();
};

} // namespace edge

#pragma once

#include "common/Value.h"
#include "planner/RecoveryPlanner.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <cstdint>

namespace edge {

struct SerializedVariable {
    std::string name;
    TypeKind type;
    Value value;
    size_t sizeBytes;

    std::string toJson() const;
};

struct CheckpointRecord {
    uint32_t magic{0xED9EC0DE};
    size_t checkpointId{0};
    int32_t resumeIp{0};
    size_t totalBytes{0};
    uint32_t checksum{0};
    std::vector<SerializedVariable> variables;

    std::string toJson() const;
};

class CheckpointStore {
private:
    std::unordered_map<size_t, CheckpointRecord> store_;
    size_t totalNvmWrites_{0};
    size_t totalBytesWritten_{0};

    uint32_t computeChecksum(const std::vector<SerializedVariable>& vars) const;

public:
    CheckpointStore() = default;

    bool saveCheckpoint(size_t id, const CheckpointPlan& plan, const std::unordered_map<std::string, Value>& currentVars, int32_t resumeIp);
    bool loadCheckpoint(size_t id, std::unordered_map<std::string, Value>& restoredVars, int32_t& resumeIp) const;

    const CheckpointRecord* getRecord(size_t id) const;
    size_t getTotalBytesWritten() const { return totalBytesWritten_; }
    size_t getTotalNvmWrites() const { return totalNvmWrites_; }

    void clear();
    std::string toJson() const;
};

} // namespace edge

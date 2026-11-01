#include "runtime/CheckpointStore.h"
#include <sstream>

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

std::string SerializedVariable::toJson() const {
    std::ostringstream oss;
    oss << "{\"name\":\"" << escapeJson(name) << "\","
        << "\"type\":\"" << typeToString(type) << "\","
        << "\"value\":" << value.toJson() << ","
        << "\"sizeBytes\":" << sizeBytes << "}";
    return oss.str();
}

std::string CheckpointRecord::toJson() const {
    std::ostringstream oss;
    oss << "{\"checkpointId\":" << checkpointId << ","
        << "\"resumeIp\":" << resumeIp << ","
        << "\"totalBytes\":" << totalBytes << ","
        << "\"checksum\":" << checksum << ","
        << "\"variables\":[";
    for (size_t i = 0; i < variables.size(); ++i) {
        if (i > 0) oss << ",";
        oss << variables[i].toJson();
    }
    oss << "]}";
    return oss.str();
}

uint32_t CheckpointStore::computeChecksum(const std::vector<SerializedVariable>& vars) const {
    uint32_t crc = 0xFFFFFFFF;
    for (const auto& v : vars) {
        for (char c : v.name) {
            crc = (crc >> 8) ^ ((crc ^ c) & 0xFF);
        }
        for (char c : v.value.asString()) {
            crc = (crc >> 8) ^ ((crc ^ c) & 0xFF);
        }
    }
    return ~crc;
}

bool CheckpointStore::saveCheckpoint(
    size_t id,
    const CheckpointPlan& plan,
    const std::unordered_map<std::string, Value>& currentVars,
    int32_t resumeIp
) {
    CheckpointRecord rec;
    rec.checkpointId = id;
    rec.resumeIp = resumeIp;
    rec.totalBytes = 0;

    for (const auto& varName : plan.savedVariables) {
        auto it = currentVars.find(varName);
        if (it != currentVars.end()) {
            SerializedVariable sv;
            sv.name = varName;
            sv.type = it->second.getType();
            sv.value = it->second;
            sv.sizeBytes = it->second.getSizeBytes();
            rec.totalBytes += sv.sizeBytes;
            rec.variables.push_back(std::move(sv));
        }
    }

    rec.checksum = computeChecksum(rec.variables);
    store_[id] = rec;
    totalNvmWrites_++;
    totalBytesWritten_ += rec.totalBytes;
    return true;
}

bool CheckpointStore::loadCheckpoint(
    size_t id,
    std::unordered_map<std::string, Value>& restoredVars,
    int32_t& resumeIp
) const {
    auto it = store_.find(id);
    if (it == store_.end()) return false;

    const CheckpointRecord& rec = it->second;
    uint32_t currentChecksum = computeChecksum(rec.variables);
    if (currentChecksum != rec.checksum) {
        return false; // Checksum corruption
    }

    restoredVars.clear();
    for (const auto& v : rec.variables) {
        restoredVars[v.name] = v.value;
    }
    resumeIp = rec.resumeIp;
    return true;
}

const CheckpointRecord* CheckpointStore::getRecord(size_t id) const {
    auto it = store_.find(id);
    if (it != store_.end()) return &it->second;
    return nullptr;
}

void CheckpointStore::clear() {
    store_.clear();
    totalNvmWrites_ = 0;
    totalBytesWritten_ = 0;
}

std::string CheckpointStore::toJson() const {
    std::ostringstream oss;
    oss << "{\"totalNvmWrites\":" << totalNvmWrites_
        << ",\"totalBytesWritten\":" << totalBytesWritten_
        << ",\"checkpoints\":[";
    bool first = true;
    for (const auto& pair : store_) {
        if (!first) oss << ",";
        first = false;
        oss << pair.second.toJson();
    }
    oss << "]}";
    return oss.str();
}

} // namespace edge

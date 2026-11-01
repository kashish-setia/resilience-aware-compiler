#pragma once

#include "SourceLocation.h"
#include <string>
#include <vector>
#include <sstream>

namespace edge {

enum class DiagnosticLevel {
    INFO,
    WARNING,
    ERROR
};

struct Diagnostic {
    DiagnosticLevel level;
    std::string message;
    SourceLocation location;

    Diagnostic(DiagnosticLevel lvl, std::string msg, SourceLocation loc)
        : level(lvl), message(std::move(msg)), location(std::move(loc)) {}

    std::string toString() const {
        std::ostringstream oss;
        oss << "[" << (level == DiagnosticLevel::ERROR ? "ERROR" : 
                      level == DiagnosticLevel::WARNING ? "WARNING" : "INFO")
            << "] " << location.toString() << ": " << message;
        return oss.str();
    }

    std::string toJson() const {
        std::ostringstream oss;
        oss << "{"
            << "\"level\":\"" << (level == DiagnosticLevel::ERROR ? "error" : 
                                 level == DiagnosticLevel::WARNING ? "warning" : "info") << "\","
            << "\"message\":\"" << escapeJson(message) << "\","
            << "\"line\":" << location.line << ","
            << "\"column\":" << location.column
            << "}";
        return oss.str();
    }

private:
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
};

class DiagnosticEngine {
private:
    std::vector<Diagnostic> diagnostics_;

public:
    void report(DiagnosticLevel level, const std::string& message, const SourceLocation& location) {
        diagnostics_.emplace_back(level, message, location);
    }

    void error(const std::string& message, const SourceLocation& location) {
        report(DiagnosticLevel::ERROR, message, location);
    }

    void warning(const std::string& message, const SourceLocation& location) {
        report(DiagnosticLevel::WARNING, message, location);
    }

    void info(const std::string& message, const SourceLocation& location) {
        report(DiagnosticLevel::INFO, message, location);
    }

    bool hasErrors() const {
        for (const auto& d : diagnostics_) {
            if (d.level == DiagnosticLevel::ERROR) return true;
        }
        return false;
    }

    const std::vector<Diagnostic>& getDiagnostics() const { return diagnostics_; }

    void clear() { diagnostics_.clear(); }

    std::string toString() const {
        std::ostringstream oss;
        for (const auto& d : diagnostics_) {
            oss << d.toString() << "\n";
        }
        return oss.str();
    }

    std::string toJson() const {
        std::ostringstream oss;
        oss << "[";
        for (size_t i = 0; i < diagnostics_.size(); ++i) {
            if (i > 0) oss << ",";
            oss << diagnostics_[i].toJson();
        }
        oss << "]";
        return oss.str();
    }
};

} // namespace edge

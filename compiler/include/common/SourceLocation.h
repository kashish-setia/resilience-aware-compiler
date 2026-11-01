#pragma once

#include <string>
#include <sstream>

namespace edge {

struct SourceLocation {
    size_t line = 1;
    size_t column = 1;
    std::string filename = "";

    SourceLocation() = default;
    SourceLocation(size_t l, size_t c, std::string fn = "")
        : line(l), column(c), filename(std::move(fn)) {}

    std::string toString() const {
        std::ostringstream oss;
        if (!filename.empty()) {
            oss << filename << ":";
        }
        oss << line << ":" << column;
        return oss.str();
    }
};

} // namespace edge

#pragma once

#include <string>
#include <cstdint>

namespace edge {

enum class TypeKind {
    VOID,
    INT,
    FLOAT,
    BOOL,
    STRING,
    UNKNOWN
};

inline std::string typeToString(TypeKind kind) {
    switch (kind) {
        case TypeKind::VOID: return "void";
        case TypeKind::INT: return "int";
        case TypeKind::FLOAT: return "float";
        case TypeKind::BOOL: return "bool";
        case TypeKind::STRING: return "string";
        case TypeKind::UNKNOWN: return "unknown";
    }
    return "unknown";
}

inline TypeKind stringToType(const std::string& str) {
    if (str == "int") return TypeKind::INT;
    if (str == "float") return TypeKind::FLOAT;
    if (str == "bool") return TypeKind::BOOL;
    if (str == "string") return TypeKind::STRING;
    if (str == "void") return TypeKind::VOID;
    return TypeKind::UNKNOWN;
}

inline size_t getTypeSize(TypeKind kind) {
    switch (kind) {
        case TypeKind::BOOL: return 1;
        case TypeKind::INT: return 4;
        case TypeKind::FLOAT: return 8;
        case TypeKind::STRING: return 8; // pointer or descriptor size on 64-bit / 32-bit edge
        default: return 4;
    }
}

} // namespace edge

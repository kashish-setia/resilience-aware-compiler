#pragma once

#include "Types.h"
#include <string>
#include <variant>
#include <sstream>
#include <iomanip>
#include <stdexcept>

namespace edge {

class Value {
public:
    using Storage = std::variant<std::monostate, int64_t, double, bool, std::string>;

private:
    TypeKind type_{TypeKind::VOID};
    Storage data_{std::monostate{}};

public:
    Value() : type_(TypeKind::VOID), data_(std::monostate{}) {}
    explicit Value(int64_t val) : type_(TypeKind::INT), data_(val) {}
    explicit Value(int val) : type_(TypeKind::INT), data_(static_cast<int64_t>(val)) {}
    explicit Value(double val) : type_(TypeKind::FLOAT), data_(val) {}
    explicit Value(bool val) : type_(TypeKind::BOOL), data_(val) {}
    explicit Value(std::string val) : type_(TypeKind::STRING), data_(std::move(val)) {}
    explicit Value(const char* val) : type_(TypeKind::STRING), data_(std::string(val)) {}

    TypeKind getType() const { return type_; }
    bool isVoid() const { return type_ == TypeKind::VOID; }
    bool isInt() const { return type_ == TypeKind::INT; }
    bool isFloat() const { return type_ == TypeKind::FLOAT; }
    bool isBool() const { return type_ == TypeKind::BOOL; }
    bool isString() const { return type_ == TypeKind::STRING; }

    int64_t asInt() const {
        if (isInt()) return std::get<int64_t>(data_);
        if (isFloat()) return static_cast<int64_t>(std::get<double>(data_));
        if (isBool()) return std::get<bool>(data_) ? 1 : 0;
        if (isString()) {
            try { return std::stoll(std::get<std::string>(data_)); } catch (...) { return 0; }
        }
        return 0;
    }

    double asFloat() const {
        if (isFloat()) return std::get<double>(data_);
        if (isInt()) return static_cast<double>(std::get<int64_t>(data_));
        if (isBool()) return std::get<bool>(data_) ? 1.0 : 0.0;
        if (isString()) {
            try { return std::stod(std::get<std::string>(data_)); } catch (...) { return 0.0; }
        }
        return 0.0;
    }

    bool asBool() const {
        if (isBool()) return std::get<bool>(data_);
        if (isInt()) return std::get<int64_t>(data_) != 0;
        if (isFloat()) return std::get<double>(data_) != 0.0;
        if (isString()) return !std::get<std::string>(data_).empty();
        return false;
    }

    std::string asString() const {
        if (isString()) return std::get<std::string>(data_);
        if (isInt()) return std::to_string(std::get<int64_t>(data_));
        if (isFloat()) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2) << std::get<double>(data_);
            return oss.str();
        }
        if (isBool()) return std::get<bool>(data_) ? "true" : "false";
        return "void";
    }

    size_t getSizeBytes() const {
        return getTypeSize(type_);
    }

    bool operator==(const Value& other) const {
        if (type_ != other.type_) {
            if ((isInt() || isFloat()) && (other.isInt() || other.isFloat())) {
                return asFloat() == other.asFloat();
            }
            return false;
        }
        return data_ == other.data_;
    }

    bool operator!=(const Value& other) const {
        return !(*this == other);
    }

    std::string toJson() const {
        std::ostringstream oss;
        if (isInt()) {
            oss << asInt();
        } else if (isFloat()) {
            oss << asFloat();
        } else if (isBool()) {
            oss << (asBool() ? "true" : "false");
        } else if (isString()) {
            oss << "\"" << asString() << "\"";
        } else {
            oss << "null";
        }
        return oss.str();
    }
};

} // namespace edge

#pragma once

#include <string>

namespace edge {

enum class AnnotationKind {
    NONE,
    CRITICAL,
    OPTIONAL,
    DISPOSABLE
};

inline std::string annotationToString(AnnotationKind kind) {
    switch (kind) {
        case AnnotationKind::NONE: return "none";
        case AnnotationKind::CRITICAL: return "@critical";
        case AnnotationKind::OPTIONAL: return "@optional";
        case AnnotationKind::DISPOSABLE: return "@disposable";
    }
    return "none";
}

inline AnnotationKind stringToAnnotation(const std::string& str) {
    if (str == "@critical" || str == "critical") return AnnotationKind::CRITICAL;
    if (str == "@optional" || str == "optional") return AnnotationKind::OPTIONAL;
    if (str == "@disposable" || str == "disposable") return AnnotationKind::DISPOSABLE;
    return AnnotationKind::NONE;
}

} // namespace edge

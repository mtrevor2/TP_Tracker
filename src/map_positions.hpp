#pragma once
#include "model.hpp"

namespace tracker {
// An explicit entrance is authoritative even when its interior uses an F_
// stage name (Iza's boat course). Never draw non-local placeholder coordinates.
inline const Json* worldMapAnchor(const Json& point) {
    if (point.contains("overworld") && point.at("overworld").is_object())
        return &point.at("overworld");
    if (point.value("local", true) && point.at("stage").get<std::string>().starts_with("F_"))
        return &point;
    return nullptr;
}
}

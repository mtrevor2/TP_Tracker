#pragma once
#include "model.hpp"
#include <mods/api.h>
namespace tracker {
ModResult initializeMap(Model* model);
void shutdownMap();
extern bool mapEnabled;
extern bool mapAvailable;
extern bool minimapEnabled, minimapAvailable;
constexpr int checkTypeCount = 8;
extern bool mapTypes[checkTypeCount], minimapTypes[checkTypeCount];
int checkType(const Json& check);
}

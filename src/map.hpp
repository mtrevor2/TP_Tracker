#pragma once
#include "model.hpp"
#include <mods/api.h>
namespace tracker {
ModResult initializeMap(Model* model);
void shutdownMap();
using MapCheckHandler = bool (*)(const std::string&);
void setMapCheckHandler(MapCheckHandler handler,bool (*closeHandler)());
bool mapInspectionActive();
extern bool mapEnabled;
extern bool showCheckInfoHint;
extern bool mapAccessibleOnly, minimapAccessibleOnly;
extern bool mapAvailable;
extern bool minimapEnabled, minimapAvailable;
constexpr int checkTypeCount = 10;
extern bool mapTypes[checkTypeCount], minimapTypes[checkTypeCount];
int checkType(const Json& check);
}

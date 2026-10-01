#pragma once
#include <string>
#include <mods/api.h>
union SDL_Event;
namespace tracker {
ModResult initializeInput();
void resetInput();
enum class MapAction { MouseClick, ControllerConfirm, Dismiss };
using MapActionHandler = bool (*)(MapAction,float,float);
void setMapActionHandler(MapActionHandler handler);
bool processInput(const SDL_Event& event);
bool readMousePosition(float& x,float& y);
bool readRightStick(int& x,int& y);
int pressedBinding(bool controller);
std::string bindingName(int binding,bool controller);
bool escapeBinding(int binding);
bool shortcutDown(int keyboard,int controller);
}


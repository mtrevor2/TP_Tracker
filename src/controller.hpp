#pragma once
#include <string>
#include <mods/api.h>
union SDL_Event;
namespace tracker {
ModResult initializeInput();
void resetInput();
void processInput(const SDL_Event& event);
bool readRightStick(int& x,int& y);
int pressedBinding(bool controller);
std::string bindingName(int binding,bool controller);
bool escapeBinding(int binding);
bool shortcutDown(int keyboard,int controller);
}


#include "controller.hpp"
#include <SDL3/SDL_events.h>
#include <stdexcept>
void require(bool value) { if(!value) throw std::runtime_error("input regression"); }
int main() {
 using namespace tracker;
 SDL_Event e{};
 e.type=SDL_EVENT_KEY_DOWN; e.key.scancode=SDL_SCANCODE_F5; processInput(e);
 require(pressedBinding(false)==512+SDL_SCANCODE_F5);
 require(shortcutDown(5,0) && shortcutDown(256+116,0));
 e.type=SDL_EVENT_KEY_UP; processInput(e); require(!pressedBinding(false));
 e.type=SDL_EVENT_GAMEPAD_BUTTON_DOWN; e.gbutton.which=7; e.gbutton.button=SDL_GAMEPAD_BUTTON_SOUTH; processInput(e);
 require(pressedBinding(true)==1 && shortcutDown(0,1));
 e.type=SDL_EVENT_GAMEPAD_AXIS_MOTION; e.gaxis.which=7; e.gaxis.axis=SDL_GAMEPAD_AXIS_RIGHTY; e.gaxis.value=-24000; processInput(e);
 int x=0,y=0; require(readRightStick(x,y) && x==0 && y==24000);
 e.type=SDL_EVENT_GAMEPAD_REMOVED; e.gdevice.which=7; processInput(e);
 require(!pressedBinding(true) && !readRightStick(x,y));
 e.type=SDL_EVENT_KEY_DOWN; e.key.scancode=SDL_SCANCODE_A; processInput(e);
 e.type=SDL_EVENT_WINDOW_FOCUS_LOST; processInput(e); require(!pressedBinding(false));
 e.type=SDL_EVENT_WINDOW_FOCUS_GAINED; processInput(e); require(!pressedBinding(false));
 require(escapeBinding(512+SDL_SCANCODE_ESCAPE) && escapeBinding(256+27));
}

#include "controller.hpp"
#include <SDL3/SDL_events.h>
#include <stdexcept>
#include "map_interaction.hpp"
#include <iostream>
void require(bool value) { if(!value) throw std::runtime_error("input regression"); }
namespace {
tracker::MapHitTargets targets;
tracker::MapScreenPoint controllerCursor{50,50};
bool details=false;
int opened=0,closed=0;
bool handle(tracker::MapAction action,float x,float y) {
 if(action==tracker::MapAction::Dismiss) { if(!details) return false; details=false; ++closed; return true; }
 if(details) return false;
 const auto p=action==tracker::MapAction::MouseClick ? tracker::MapScreenPoint{x,y} : controllerCursor;
 if(targets.hit(p).empty()) return false;
 details=true; ++opened; return true;
}
}
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

 resetInput(); setMapActionHandler(handle);
 targets.clear({0,0,100,100}); targets.add(50,50,12,12,"Chest");
 auto button=[&](unsigned id,unsigned b,bool down) { e={}; e.type=down ? SDL_EVENT_GAMEPAD_BUTTON_DOWN : SDL_EVENT_GAMEPAD_BUTTON_UP; e.gbutton.which=id; e.gbutton.button=b; return processInput(e); };
 auto trigger=[&](unsigned id,int value) { e={}; e.type=SDL_EVENT_GAMEPAD_AXIS_MOTION; e.gaxis.which=id; e.gaxis.axis=SDL_GAMEPAD_AXIS_RIGHT_TRIGGER; e.gaxis.value=value; return processInput(e); };
 require(!button(1,SDL_GAMEPAD_BUTTON_SOUTH,true)); // confirm alone is untouched
 button(1,SDL_GAMEPAD_BUTTON_SOUTH,false); trigger(2,25000);
 require(!button(1,SDL_GAMEPAD_BUTTON_SOUTH,true)); // cannot combine controllers
 button(1,SDL_GAMEPAD_BUTTON_SOUTH,false); trigger(1,25000);
 require(button(1,SDL_GAMEPAD_BUTTON_SOUTH,true) && opened==1);
 require(!button(1,SDL_GAMEPAD_BUTTON_SOUTH,true) && opened==1); // held: no repeats
 require(button(1,SDL_GAMEPAD_BUTTON_EAST,true) && closed==1 && !details);
 require(!button(1,SDL_GAMEPAD_BUTTON_EAST,true) && closed==1);
 button(1,SDL_GAMEPAD_BUTTON_SOUTH,false); button(1,SDL_GAMEPAD_BUTTON_EAST,false);
 controllerCursor={90,90}; require(!button(1,SDL_GAMEPAD_BUTTON_SOUTH,true)); // empty map
 controllerCursor={50,50}; require(!button(1,SDL_GAMEPAD_BUTTON_SOUTH,true)); // moving onto icon while held
 button(1,SDL_GAMEPAD_BUTTON_SOUTH,false); require(button(1,SDL_GAMEPAD_BUTTON_SOUTH,true));
 e={}; e.type=SDL_EVENT_KEY_DOWN; e.key.scancode=SDL_SCANCODE_ESCAPE;
 require(processInput(e) && closed==2); e.key.repeat=true; require(!processInput(e));
 resetInput(); require(!button(1,SDL_GAMEPAD_BUTTON_SOUTH,true)); // reset loses trigger
 button(1,SDL_GAMEPAD_BUTTON_SOUTH,false);
 auto click=[&](float x,float y,unsigned b,bool down) { e={}; e.type=down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP; e.button.x=x;e.button.y=y;e.button.button=b;return processInput(e); };
 require(!click(50,50,SDL_BUTTON_RIGHT,true));
 require(!click(90,90,SDL_BUTTON_LEFT,true)); click(90,90,SDL_BUTTON_LEFT,false);
 require(click(50,50,SDL_BUTTON_LEFT,true)); require(!click(50,50,SDL_BUTTON_LEFT,true));
 float mx=0,my=0; require(readMousePosition(mx,my) && mx==50 && my==50);
 require(button(1,SDL_GAMEPAD_BUTTON_EAST,true)); require(!readMousePosition(mx,my));
 click(50,50,SDL_BUTTON_LEFT,false);
 targets.clear(); // closing/zooming out/filtering removes rendered hit regions
 require(!click(50,50,SDL_BUTTON_LEFT,true));
 e={};e.type=SDL_EVENT_WINDOW_FOCUS_LOST;processInput(e);
 require(!readMousePosition(mx,my) && !shortcutDown(0,1));
 setMapActionHandler(nullptr);
 std::cout << "Input and map interaction regression scenarios passed\n";
}

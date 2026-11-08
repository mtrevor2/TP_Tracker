#include "controller.hpp"
#include <SDL3/SDL_events.h>
#include <stdexcept>
#include "map_interaction.hpp"
#include <iostream>
void require(bool value) { if(!value) throw std::runtime_error("input regression"); }
namespace {
tracker::MapInspectionFrame frame;
tracker::MapHitTargets& targets=frame.targets;
int owner=0,unrelated=0;
bool dungeon=false,zoomed=true;
tracker::MapScreenPoint controllerCursor{50,50};
bool details=false;
int opened=0,closed=0;
bool handle(tracker::MapAction action,float x,float y) {
 if(action==tracker::MapAction::Dismiss) { if(!details) return false; details=false; ++closed; return true; }
 if(details || !frame.active(&owner,dungeon,zoomed,"stage")) return false;
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
 frame.begin(&owner,&owner,dungeon,zoomed,"stage",{0,0,100,100},controllerCursor);
 targets.clear({0,0,100,100}); targets.add(50,50,12,12,"Chest");
 auto button=[&](unsigned id,unsigned b,bool down) { e={}; e.type=down ? SDL_EVENT_GAMEPAD_BUTTON_DOWN : SDL_EVENT_GAMEPAD_BUTTON_UP; e.gbutton.which=id; e.gbutton.button=b; return processInput(e); };
 auto trigger=[&](unsigned id,int value) { e={}; e.type=SDL_EVENT_GAMEPAD_AXIS_MOTION; e.gaxis.which=id; e.gaxis.axis=SDL_GAMEPAD_AXIS_RIGHT_TRIGGER; e.gaxis.value=value; return processInput(e); };
 require(checkBindingUses(1,true) && checkBindingUses(100,true) && !checkBindingUses(16,true));
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
 resetInput(); zoomed=false;
 require(!button(1,SDL_GAMEPAD_BUTTON_SOUTH,true)); // A still zooms the native map
 zoomed=true; require(!button(1,SDL_GAMEPAD_BUTTON_SOUTH,true)); // zooming in while held cannot open details
 button(1,SDL_GAMEPAD_BUTTON_SOUTH,false);
 frame.clear(); require(!button(1,SDL_GAMEPAD_BUTTON_SOUTH,true)); // normal gameplay/menu A passes through
 button(1,SDL_GAMEPAD_BUTTON_SOUTH,false);
 frame.begin(&owner,&owner,dungeon,zoomed,"stage",{0,0,100,100},controllerCursor);
 targets.add(50,50,12,12,"Chest");
 auto click=[&](float x,float y,unsigned b,bool down) { e={}; e.type=down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP; e.button.x=x;e.button.y=y;e.button.button=b;return processInput(e); };
 require(!click(50,50,SDL_BUTTON_RIGHT,true));
 require(!click(90,90,SDL_BUTTON_LEFT,true)); click(90,90,SDL_BUTTON_LEFT,false);
 require(click(50,50,SDL_BUTTON_LEFT,true)); require(!click(50,50,SDL_BUTTON_LEFT,true));
 float mx=0,my=0; require(readMousePosition(mx,my) && mx==50 && my==50);
 require(button(1,SDL_GAMEPAD_BUTTON_EAST,true)); require(!readMousePosition(mx,my));
 click(50,50,SDL_BUTTON_LEFT,false);
 // Overworld and temple renderers share these hit targets and the same
 // optional hint. A alone and mouse remain available
 // across visibility changes, including restoring a hidden hint at startup.
 for(bool temple:{false,true}) for(bool showHint:{false,true,false}) {
  dungeon=temple; zoomed=true;
  frame.begin(&owner,&owner,dungeon,zoomed,"stage",{0,0,100,100},{50,50});
  targets.add(50,50,12,12,"Chest");
  resetInput(); details=false; controllerCursor={50,50};
  require(!frame.begin(&unrelated,&owner,!dungeon,false,"other",{},{}));
  targets.setHint(showHint ? std::optional<MapHitRect>{{0,0,100,20}} : std::nullopt);
  require(targets.hit(controllerCursor)==std::vector<std::string>{"Chest"});
  const int before=opened;
  require(button(3,SDL_GAMEPAD_BUTTON_SOUTH,true) && opened==before+1); // no trigger needed
  require(!trigger(4,25000)); // another controller cannot repeat the action
  require(!trigger(3,25000)); // adding the old modifier cannot repeat it
  require(!trigger(3,26000) && opened==before+1); // held trigger motion cannot repeat
  require(button(3,SDL_GAMEPAD_BUTTON_EAST,true) && !details);
  require(!trigger(3,27000) && opened==before+1); // B dismiss does not rearm held A
  button(3,SDL_GAMEPAD_BUTTON_EAST,false); button(3,SDL_GAMEPAD_BUTTON_SOUTH,false);
  require(button(3,SDL_GAMEPAD_BUTTON_SOUTH,true)); // A still works with a trigger held
  require(button(3,SDL_GAMEPAD_BUTTON_EAST,true) && !details);
  button(3,SDL_GAMEPAD_BUTTON_EAST,false); button(3,SDL_GAMEPAD_BUTTON_SOUTH,false); trigger(3,0);
  require(click(50,50,SDL_BUTTON_LEFT,true));
  e={}; e.type=SDL_EVENT_KEY_DOWN; e.key.scancode=SDL_SCANCODE_ESCAPE;
  require(processInput(e) && !details);
  click(50,50,SDL_BUTTON_LEFT,false);
  controllerCursor={90,90}; button(3,SDL_GAMEPAD_BUTTON_SOUTH,true);
  require(!trigger(3,25000)); // no check under cursor
  controllerCursor={50,50}; require(!trigger(3,26000)); // moving held A onto a check is not a press
 }
 zoomed=false; resetInput();
 require(!click(50,50,SDL_BUTTON_LEFT,true));
 click(50,50,SDL_BUTTON_LEFT,false); zoomed=true;
 targets.clear(); // closing/zooming out/filtering removes rendered hit regions
 require(!click(50,50,SDL_BUTTON_LEFT,true));
 e={};e.type=SDL_EVENT_WINDOW_FOCUS_LOST;processInput(e);
 require(!readMousePosition(mx,my) && !shortcutDown(0,1));
 setMapActionHandler(nullptr);
 // Steam Input gyro-as-mouse must not replace a stationary map reticle.
 // Hover ownership also drives summary labels and the dungeon reticle, even
 // when the marker has no individual Check Details target.
 resetInput();
 e={}; e.type=SDL_EVENT_WINDOW_FOCUS_GAINED; processInput(e);
 auto motion=[&](float x,float y) {
  e={}; e.type=SDL_EVENT_MOUSE_MOTION; e.motion.x=x; e.motion.y=y;
  require(!processInput(e)); // gyro/mouse events still reach the host
 };
 auto axis=[&](unsigned id,unsigned which,int value) {
  e={}; e.type=SDL_EVENT_GAMEPAD_AXIS_MOTION; e.gaxis.which=id;
  e.gaxis.axis=which; e.gaxis.value=value; return processInput(e);
 };
 motion(50,50); require(readMousePosition(mx,my) && mx==50 && my==50);
 axis(9,SDL_GAMEPAD_AXIS_LEFTX,20000);
 motion(90,90); require(!readMousePosition(mx,my));
 axis(9,SDL_GAMEPAD_AXIS_LEFTX,0);
 for(int i=0;i<1000;++i) {
  motion(90+i,90-i); // neither persistent drift nor large motion steals hover
  require(!readMousePosition(mx,my));
 }
 require(!shortcutDown(0,1)); // no held button is needed to retain hover
 e={}; e.type=SDL_EVENT_WINDOW_MOUSE_LEAVE; processInput(e);
 motion(25,25); require(!readMousePosition(mx,my));
 // An actual mouse press is an explicit handoff, including direct check clicks.
 targets.clear({0,0,100,100}); targets.add(50,50,12,12,"Chest");
 details=false; setMapActionHandler(handle);
 require(click(50,50,SDL_BUTTON_LEFT,true) && details);
 require(readMousePosition(mx,my));
 click(50,50,SDL_BUTTON_LEFT,false);
 motion(60,60); require(readMousePosition(mx,my) && mx==60 && my==60);
 axis(9,SDL_GAMEPAD_AXIS_LEFTX,2000); require(readMousePosition(mx,my)); // stick jitter
 require(button(9,SDL_GAMEPAD_BUTTON_EAST,true) && !details);
 motion(60,60); require(!readMousePosition(mx,my));
 click(60,60,SDL_BUTTON_RIGHT,true); require(readMousePosition(mx,my));
 button(9,SDL_GAMEPAD_BUTTON_EAST,false); require(readMousePosition(mx,my)); // release cannot steal it back
 button(9,SDL_GAMEPAD_BUTTON_SOUTH,true); require(!readMousePosition(mx,my));
 click(60,60,SDL_BUTTON_RIGHT,false); require(!readMousePosition(mx,my));
 e={}; e.type=SDL_EVENT_GAMEPAD_REMOVED; e.gdevice.which=9; processInput(e);
 motion(40,40); require(readMousePosition(mx,my)); // unplugging restores mouse-only use
 axis(10,SDL_GAMEPAD_AXIS_RIGHTX,-20000); motion(40,40); require(!readMousePosition(mx,my));
 e={}; e.type=SDL_EVENT_WINDOW_FOCUS_LOST; processInput(e);
 motion(40,40); require(!readMousePosition(mx,my));
 e={}; e.type=SDL_EVENT_WINDOW_FOCUS_GAINED; processInput(e);
 motion(40,40); require(readMousePosition(mx,my)); // reset cannot strand mouse users
 setMapActionHandler(nullptr);
 // Rebinding replaces default A and preserves mouse inspection, including stacks.
 setMapActionHandler(handle); zoomed=true; dungeon=false; controllerCursor={50,50};
 frame.begin(&owner,&owner,false,true,"stage",{0,0,100,100},{50,50});
 targets.addGroup(50,50,12,12,{"First check","Second check"});
 for(int binding=1;binding<=16;++binding) {
  resetInput(); details=false; setCheckBindings(0,binding);
  constexpr int padButtons[]={-1,0,1,2,3,9,10,4,6,7,8,11,12,13,14};
  bool consumed=binding<15 ? button(12,padButtons[binding],true) : axis(12,binding==15 ? SDL_GAMEPAD_AXIS_LEFT_TRIGGER : SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,24000);
  require(consumed && details);
  require(!(binding<15 ? button(12,padButtons[binding],true) : axis(12,binding==15 ? SDL_GAMEPAD_AXIS_LEFT_TRIGGER : SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,25000)));
  require(checkBindingUses(binding,true));
 }
 resetInput(); details=false; setCheckBindings(0,100+SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1);
 require(button(13,SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1,true) && details);
 resetInput(); details=false; setCheckBindings(512+SDL_SCANCODE_K,0);
 require(!trigger(12,25000) && !button(12,SDL_GAMEPAD_BUTTON_SOUTH,true) && !details);
 e={}; e.type=SDL_EVENT_KEY_DOWN; e.key.scancode=SDL_SCANCODE_K;
 require(processInput(e) && details); require(!processInput(e));
 details=false; resetInput(); require(click(50,50,SDL_BUTTON_LEFT,true) && details);
 resetInput(); details=false; setCheckBindings(0,4);
 require(!trigger(12,25000) && !button(12,SDL_GAMEPAD_BUTTON_SOUTH,true) && !details);
 require(button(12,SDL_GAMEPAD_BUTTON_NORTH,true) && details);
 resetInput(); details=false; setCheckBindings(0,0);
 require(button(12,SDL_GAMEPAD_BUTTON_SOUTH,true) && details); // clearing restores A alone
 setMapActionHandler(nullptr);
 // Binding capture runs from UI presentation, with no game-simulation tick.
 // Capture a complete press/release between frames, including A and Back,
 // without letting those presses activate the prompt's Cancel button.
 constexpr int captureButtons[]={-1,0,1,2,3,9,10,4,6,7,8,11,12,13,14};
 for(int binding=1;binding<=16;++binding) {
  resetInput(); beginControllerBindingCapture();
  require(pollControllerBindingCapture()==0); // one UI frame arms idle input
  if(binding<15) {
   require(button(20,captureButtons[binding],true));
   require(!button(20,captureButtons[binding],false));
  } else {
   const auto which=binding==15 ? SDL_GAMEPAD_AXIS_LEFT_TRIGGER : SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
   require(axis(20,which,24000)); require(!axis(20,which,0));
  }
  require(!pressedBinding(true));
  require(pollControllerBindingCapture()==binding); // released taps remain captured
  require(!shortcutPressed(0,binding)); // assignment cannot also open the tracker
  cancelControllerBindingCapture();
 }
 resetInput(); button(20,SDL_GAMEPAD_BUTTON_SOUTH,true);
 beginControllerBindingCapture(); require(pollControllerBindingCapture()==0);
 require(button(20,SDL_GAMEPAD_BUTTON_SOUTH,true));
 require(pollControllerBindingCapture()==0); // opening A must first be released
 require(!button(20,SDL_GAMEPAD_BUTTON_SOUTH,false));
 require(axis(20,SDL_GAMEPAD_AXIS_LEFTX,24000));
 require(pollControllerBindingCapture()==0); // moving a stick is not a binding
 require(button(20,SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1,true));
 require(pollControllerBindingCapture()==100+SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1);
 cancelControllerBindingCapture(); resetInput();
 beginControllerBindingCapture(); pollControllerBindingCapture();
 e={};e.type=SDL_EVENT_WINDOW_FOCUS_LOST;processInput(e);
 require(!button(20,SDL_GAMEPAD_BUTTON_NORTH,true) && pollControllerBindingCapture()==0);
 e={};e.type=SDL_EVENT_WINDOW_FOCUS_GAINED;processInput(e);
 pollControllerBindingCapture();
 require(button(20,SDL_GAMEPAD_BUTTON_NORTH,true) && pollControllerBindingCapture()==4);
 cancelControllerBindingCapture(); resetInput();
 // Shortcut edges survive a tap between UI frames and clear after presentation.
 button(20,SDL_GAMEPAD_BUTTON_WEST,true); button(20,SDL_GAMEPAD_BUTTON_WEST,false);
 require(!shortcutDown(0,3) && shortcutPressed(0,3));
 pollInputFrame(); require(!shortcutPressed(0,3));
 axis(20,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,24000); axis(20,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,0);
 require(!shortcutDown(0,16) && shortcutPressed(0,16));
 pollInputFrame(); require(!shortcutPressed(0,16));
 e={};e.type=SDL_EVENT_KEY_DOWN;e.key.scancode=SDL_SCANCODE_F5;processInput(e);
 e.type=SDL_EVENT_KEY_UP;processInput(e);
 require(!shortcutDown(5,0) && shortcutPressed(5,0));
 pollInputFrame(); require(!shortcutPressed(5,0));
 std::cout << "Input and map interaction regression scenarios passed\n";
}

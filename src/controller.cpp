#include "controller.hpp"
#include <SDL3/SDL_events.h>
#ifndef TPTRACKER_INPUT_TESTS
#include "input_hook.hpp"
#endif
#include <array>
#include <map>
#include <cstdlib>
namespace tracker {
namespace {
std::array<bool,SDL_SCANCODE_COUNT> keys{},keyPresses{};
struct Pad {
 std::array<bool,SDL_GAMEPAD_BUTTON_COUNT> buttons{},buttonPresses{};
 std::array<int,SDL_GAMEPAD_AXIS_COUNT> axes{};
 std::array<bool,SDL_GAMEPAD_AXIS_COUNT> axisPresses{};
};
void (*inputFrameHandler)()=nullptr;
bool capturingController=false,captureReady=false;
int capturedController=0;
int checkKeyboard=0, checkController=0;
std::map<SDL_JoystickID,Pad> pads;
bool focused=true;
// Mouse motion can be Steam Input gyro, indistinguishable from a real mouse.
// Once a controller claims map hover, retain its reticle until a mouse press.
// This only chooses the tracker's hover source; motion still reaches the host.
bool mouseValid=false, mouseActive=true, mouseHeld=false;
float mouseX=0,mouseY=0;
MapActionHandler mapActionHandler=nullptr;
constexpr int buttons[]={-1,0,1,2,3,9,10,4,6,7,8,11,12,13,14};
int buttonBinding(int button) {
 for(int binding=1;binding<15;++binding) if(buttons[binding]==button) return binding;
 return 100+button;
}
void captureControllerPress(int binding,bool edge) {
 if(!captureReady) { if(!pressedBinding(true)) captureReady=true; }
 else if(edge && !capturedController) capturedController=binding;
}
int scan(int b) {
 if(b>=512) return b-512;
 if(b>=1 && b<=12) return SDL_SCANCODE_F1+b-1;
 if(b>=13 && b<=38) return SDL_SCANCODE_A+b-13;
 if(b>=39 && b<=48) return b==39 ? SDL_SCANCODE_0 : SDL_SCANCODE_1+b-40;
 constexpr int extra[]={SDL_SCANCODE_TAB,SDL_SCANCODE_SPACE,SDL_SCANCODE_INSERT,SDL_SCANCODE_HOME,SDL_SCANCODE_END,SDL_SCANCODE_PAGEUP,SDL_SCANCODE_PAGEDOWN,SDL_SCANCODE_DELETE};
 if(b>=49 && b<=56) return extra[b-49];
 int v=b-256;
 if(v>='A' && v<='Z') return SDL_SCANCODE_A+v-'A';
 if(v>='1' && v<='9') return SDL_SCANCODE_1+v-'1';
 if(v=='0') return SDL_SCANCODE_0;
 if(v>=112 && v<=123) return SDL_SCANCODE_F1+v-112;
 switch(v) {
 case 8:return SDL_SCANCODE_BACKSPACE; case 9:return SDL_SCANCODE_TAB; case 13:return SDL_SCANCODE_RETURN;
 case 27:return SDL_SCANCODE_ESCAPE; case 32:return SDL_SCANCODE_SPACE; case 33:return SDL_SCANCODE_PAGEUP;
 case 34:return SDL_SCANCODE_PAGEDOWN; case 35:return SDL_SCANCODE_END; case 36:return SDL_SCANCODE_HOME;
 case 37:return SDL_SCANCODE_LEFT; case 38:return SDL_SCANCODE_UP; case 39:return SDL_SCANCODE_RIGHT;
 case 40:return SDL_SCANCODE_DOWN; case 45:return SDL_SCANCODE_INSERT; case 46:return SDL_SCANCODE_DELETE;
 case 160:return SDL_SCANCODE_LSHIFT; case 161:return SDL_SCANCODE_RSHIFT;
 case 162:return SDL_SCANCODE_LCTRL; case 163:return SDL_SCANCODE_RCTRL;
 case 164:return SDL_SCANCODE_LALT; case 165:return SDL_SCANCODE_RALT;
 default:return 0;
 }
}
bool padBindingDown(const Pad& pad,int binding) {
 if(binding>0 && binding<15) return pad.buttons[buttons[binding]];
 if(binding==15) return pad.axes[SDL_GAMEPAD_AXIS_LEFT_TRIGGER]>3855;
 if(binding==16) return pad.axes[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER]>3855;
 return binding>=100 && binding<100+SDL_GAMEPAD_BUTTON_COUNT && pad.buttons[binding-100];
}
bool mapChordDown(const Pad& pad) {
 if(checkController) return padBindingDown(pad,checkController);
 return !checkKeyboard && pad.buttons[SDL_GAMEPAD_BUTTON_SOUTH] && pad.axes[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER]>12000;
}
}
void setMapActionHandler(MapActionHandler handler) { mapActionHandler=handler; }
void setCheckBindings(int keyboard,int controller) { checkKeyboard=keyboard; checkController=controller; }
bool checkBindingUses(int binding,bool controller) {
 if(!binding) return false;
 if(!controller) return checkKeyboard && scan(binding)==scan(checkKeyboard);
 if(!checkKeyboard && !checkController) return binding==1 || binding==16 || binding==100;
 auto button=[](int b) { return b>0 && b<15 ? buttons[b]+100 : b; };
 return checkController && button(binding)==button(checkController);
}
void resetInput() { keys.fill(false); keyPresses.fill(false); pads.clear(); mouseValid=mouseHeld=false; mouseActive=true; captureReady=false; capturedController=0; }
void setInputFrameHandler(void (*handler)()) { inputFrameHandler=handler; }
void pollInputFrame() {
 if(inputFrameHandler) inputFrameHandler();
 keyPresses.fill(false);
 for(auto& [id,pad]:pads) { pad.buttonPresses.fill(false); pad.axisPresses.fill(false); }
}
void beginControllerBindingCapture() { capturingController=true; captureReady=false; capturedController=0; }
int pollControllerBindingCapture() {
 if(capturingController && !captureReady && !pressedBinding(true)) captureReady=true;
 return capturedController;
}
void cancelControllerBindingCapture() { capturingController=false; captureReady=false; capturedController=0; }
bool readMousePosition(float& x,float& y) { x=mouseX; y=mouseY; return focused && mouseValid && mouseActive; }
bool processInput(const SDL_Event& e) {
 if(e.type==SDL_EVENT_WINDOW_FOCUS_LOST) { focused=false; resetInput(); }
 else if(e.type==SDL_EVENT_WINDOW_FOCUS_GAINED) focused=true;
 else if(e.type==SDL_EVENT_GAMEPAD_REMOVED) {
  pads.erase(e.gdevice.which);
  if(pads.empty()) mouseActive=true;
 }
 else if(focused) {
  if(e.type==SDL_EVENT_KEY_DOWN || e.type==SDL_EVENT_KEY_UP) {
   const bool edge=e.type==SDL_EVENT_KEY_DOWN && !e.key.repeat &&
       e.key.scancode>0 && e.key.scancode<SDL_SCANCODE_COUNT && !keys[e.key.scancode];
   if(e.key.scancode>0 && e.key.scancode<SDL_SCANCODE_COUNT) keys[e.key.scancode]=e.type==SDL_EVENT_KEY_DOWN;
   if(edge) keyPresses[e.key.scancode]=true;
   if(edge && mapActionHandler) {
    if(e.key.scancode==SDL_SCANCODE_ESCAPE && mapActionHandler(MapAction::Dismiss,0,0)) return true;
    if(checkKeyboard && e.key.scancode==scan(checkKeyboard)) return mapActionHandler(MapAction::KeyboardConfirm,0,0);
   }
  } else if(e.type==SDL_EVENT_GAMEPAD_BUTTON_DOWN || e.type==SDL_EVENT_GAMEPAD_BUTTON_UP) {
   if(e.gbutton.button<SDL_GAMEPAD_BUTTON_COUNT) {
    auto& pad=pads[e.gbutton.which];
    const bool pressed=e.type==SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    const bool edge=pressed && !pad.buttons[e.gbutton.button];
    if(edge) mouseActive=false;
    const bool chordWasDown=mapChordDown(pad);
    pad.buttons[e.gbutton.button]=pressed;
    if(capturingController) {
     captureControllerPress(buttonBinding(e.gbutton.button),edge);
     // Swallow assignment presses before A/Back can operate the Cancel action.
     // Forward releases so the native UI clears the button that opened the prompt.
     return pressed;
    }
    if(edge) pad.buttonPresses[e.gbutton.button]=true;
    if(edge && e.gbutton.button==SDL_GAMEPAD_BUTTON_EAST && mapActionHandler && mapActionHandler(MapAction::Dismiss,0,0)) return true;
    // Trigger and confirm may arrive in either order. Activate only on the
    // chord's rising edge, from one controller, never just by moving a held
    // chord over a marker. This input path does not depend on hint rendering.
    if(!chordWasDown && mapChordDown(pad) && mapActionHandler)
     return mapActionHandler(MapAction::ControllerConfirm,0,0);
   }
  } else if(e.type==SDL_EVENT_GAMEPAD_AXIS_MOTION && e.gaxis.axis<SDL_GAMEPAD_AXIS_COUNT) {
   auto& pad=pads[e.gaxis.which];
   const bool chordWasDown=mapChordDown(pad);
   const bool triggerEdge=e.gaxis.axis>=SDL_GAMEPAD_AXIS_LEFT_TRIGGER && pad.axes[e.gaxis.axis]<=3855 && e.gaxis.value>3855;
   pad.axes[e.gaxis.axis]=e.gaxis.value;
   if(capturingController) {
    captureControllerPress(e.gaxis.axis==SDL_GAMEPAD_AXIS_LEFT_TRIGGER ? 15 : 16,triggerEdge);
    return std::abs(e.gaxis.value)>3855;
   }
   if(triggerEdge) pad.axisPresses[e.gaxis.axis]=true;
   if(std::abs(e.gaxis.value)>12000) mouseActive=false;
   if(!chordWasDown && mapChordDown(pad) && mapActionHandler)
    return mapActionHandler(MapAction::ControllerConfirm,0,0);
  } else if(e.type==SDL_EVENT_MOUSE_MOTION) {
   mouseX=e.motion.x; mouseY=e.motion.y; mouseValid=true;
  } else if(e.type==SDL_EVENT_MOUSE_BUTTON_DOWN || e.type==SDL_EVENT_MOUSE_BUTTON_UP) {
   mouseX=e.button.x; mouseY=e.button.y; mouseValid=true;
   if(e.type==SDL_EVENT_MOUSE_BUTTON_DOWN) mouseActive=true;
   if(e.button.button==SDL_BUTTON_LEFT) {
    const bool pressed=e.type==SDL_EVENT_MOUSE_BUTTON_DOWN;
    const bool edge=pressed && !mouseHeld; mouseHeld=pressed;
    if(edge && mapActionHandler) return mapActionHandler(MapAction::MouseClick,mouseX,mouseY);
   }
  } else if(e.type==SDL_EVENT_WINDOW_MOUSE_LEAVE) mouseValid=false;
 }
 return false;
}
#ifndef TPTRACKER_INPUT_TESTS
ModResult initializeInput() {
 resetInput(); cancelControllerBindingCapture(); focused=true;
 auto result=mods::hook::add_post<detail::InputEventHook>(detail::inputEventHook);
 if(result!=MOD_OK) return result;
 return mods::hook::add_post<detail::InputFrameHook>(detail::inputFrameHook);
}
#endif
bool readRightStick(int& x,int& y) {
 x=y=0;
 for(const auto& [id,p]:pads) {
  if(std::abs(p.axes[SDL_GAMEPAD_AXIS_RIGHTX])>std::abs(x)) x=p.axes[SDL_GAMEPAD_AXIS_RIGHTX];
  if(std::abs(p.axes[SDL_GAMEPAD_AXIS_RIGHTY])>std::abs(y)) y=-p.axes[SDL_GAMEPAD_AXIS_RIGHTY];
 }
 return focused && !pads.empty();
}
bool shortcutDown(int keyboard,int controller) {
 if(!focused) return false;
 const int s=scan(keyboard);
 if(s>0 && s<SDL_SCANCODE_COUNT && keys[s]) return true;
 for(const auto& [id,p]:pads) {
  if(padBindingDown(p,controller)) return true;
 }
 return false;
}
bool shortcutPressed(int keyboard,int controller) {
 if(!focused) return false;
 const int s=scan(keyboard);
 if(s>0 && s<SDL_SCANCODE_COUNT && keyPresses[s]) return true;
 for(const auto& [id,pad]:pads) {
  if(controller>0 && controller<15 && pad.buttonPresses[buttons[controller]]) return true;
  if(controller==15 && pad.axisPresses[SDL_GAMEPAD_AXIS_LEFT_TRIGGER]) return true;
  if(controller==16 && pad.axisPresses[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER]) return true;
  if(controller>=100 && controller<100+SDL_GAMEPAD_BUTTON_COUNT && pad.buttonPresses[controller-100]) return true;
 }
 return false;
}
int pressedBinding(bool controller) {
 if(controller) {
  for(int i=1;i<=16;++i) if(shortcutDown(0,i)) return i;
  for(int i=0;i<SDL_GAMEPAD_BUTTON_COUNT;++i) if(shortcutDown(0,100+i)) return 100+i;
 } else for(int i=1;i<SDL_SCANCODE_COUNT;++i) if(shortcutDown(512+i,0)) return 512+i;
 return 0;
}
bool escapeBinding(int b) { return scan(b)==SDL_SCANCODE_ESCAPE; }
std::string bindingName(int b,bool controller) {
 const char* names[]={"Unassigned","A / Cross","B / Circle","X / Square","Y / Triangle","Left bumper","Right bumper","Back / View","Start / Menu","Left stick click","Right stick click","D-pad Up","D-pad Down","D-pad Left","D-pad Right","Left trigger","Right trigger"};
 if(controller) return b>=0 && b<=16 ? names[b] : "Controller button "+std::to_string(b-100);
 if(!b) return "Unassigned";
 const int s=scan(b);
 if(s>=SDL_SCANCODE_A && s<=SDL_SCANCODE_Z) return std::string(1,static_cast<char>('A'+s-SDL_SCANCODE_A));
 if(s>=SDL_SCANCODE_1 && s<=SDL_SCANCODE_9) return std::string(1,static_cast<char>('1'+s-SDL_SCANCODE_1));
 if(s==SDL_SCANCODE_0) return "0";
 if(s>=SDL_SCANCODE_F1 && s<=SDL_SCANCODE_F12) return "F"+std::to_string(s-SDL_SCANCODE_F1+1);
 switch(s) {
 case SDL_SCANCODE_SPACE:return "Space"; case SDL_SCANCODE_TAB:return "Tab";
 case SDL_SCANCODE_ESCAPE:return "Escape"; case SDL_SCANCODE_RETURN:return "Enter";
 default:return "Key "+std::to_string(s);
 }
}
}

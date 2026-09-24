#include "controller.hpp"
#include <SDL3/SDL_events.h>
#ifndef TPTRACKER_INPUT_TESTS
#include <mods/svc/hook.hpp>
#endif
#include <array>
#include <map>
#include <cstdlib>
#ifndef TPTRACKER_INPUT_TESTS
DEFINE_HOOK_SYMBOL("dusk::ui::handle_event", void(const SDL_Event*), TrackerInputEvent);
#endif
namespace tracker {
namespace {
std::array<bool,SDL_SCANCODE_COUNT> keys{};
struct Pad { std::array<bool,SDL_GAMEPAD_BUTTON_COUNT> buttons{}; std::array<int,SDL_GAMEPAD_AXIS_COUNT> axes{}; };
std::map<SDL_JoystickID,Pad> pads;
bool focused=true;
constexpr int buttons[]={-1,0,1,2,3,9,10,4,6,7,8,11,12,13,14};
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
#ifndef TPTRACKER_INPUT_TESTS
HookAction eventHook(ModContext*,void* args,void*,void*) {
 if(const auto* e=mods::arg<const SDL_Event*>(args,0)) processInput(*e);
 return HOOK_CONTINUE;
}
#endif
}
void resetInput() { keys.fill(false); pads.clear(); }
void processInput(const SDL_Event& e) {
 if(e.type==SDL_EVENT_WINDOW_FOCUS_LOST) { focused=false; resetInput(); }
 else if(e.type==SDL_EVENT_WINDOW_FOCUS_GAINED) focused=true;
 else if(e.type==SDL_EVENT_GAMEPAD_REMOVED) pads.erase(e.gdevice.which);
 else if(focused) {
  if(e.type==SDL_EVENT_KEY_DOWN || e.type==SDL_EVENT_KEY_UP) {
   if(e.key.scancode>0 && e.key.scancode<SDL_SCANCODE_COUNT) keys[e.key.scancode]=e.type==SDL_EVENT_KEY_DOWN;
  } else if(e.type==SDL_EVENT_GAMEPAD_BUTTON_DOWN || e.type==SDL_EVENT_GAMEPAD_BUTTON_UP) {
   if(e.gbutton.button<SDL_GAMEPAD_BUTTON_COUNT) pads[e.gbutton.which].buttons[e.gbutton.button]=e.type==SDL_EVENT_GAMEPAD_BUTTON_DOWN;
  } else if(e.type==SDL_EVENT_GAMEPAD_AXIS_MOTION && e.gaxis.axis<SDL_GAMEPAD_AXIS_COUNT)
   pads[e.gaxis.which].axes[e.gaxis.axis]=e.gaxis.value;
 }
}
#ifndef TPTRACKER_INPUT_TESTS
ModResult initializeInput() { resetInput(); focused=true; return mods::hook::add_pre<TrackerInputEvent>(eventHook); }
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
  if(controller>0 && controller<15 && p.buttons[buttons[controller]]) return true;
  if(controller==15 && p.axes[SDL_GAMEPAD_AXIS_LEFT_TRIGGER]>3855) return true;
  if(controller==16 && p.axes[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER]>3855) return true;
  if(controller>=100 && controller<100+SDL_GAMEPAD_BUTTON_COUNT && p.buttons[controller-100]) return true;
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

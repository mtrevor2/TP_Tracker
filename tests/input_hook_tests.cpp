#include "input_hook.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <vector>

extern "C" { ModContext* mod_ctx=nullptr; }
namespace {
using TrackerHook=tracker::detail::InputEventHook;
// A different mod using the SDK's reference trampoline. Mutable references
// have the same envelope as const references, without the SDK const-erasure gap.
struct OtherTag {};
using OtherHook=mods::HookImpl<OtherTag,void,SDL_Event&>;
static_assert(std::is_same_v<decltype(&TrackerHook::trampoline),void(*)(const SDL_Event&)>);
static_assert(std::is_same_v<decltype(TrackerHook::g_orig),void(*)(const SDL_Event&)>);
std::vector<HookPreFn> callbacks;
const SDL_Event* expected=nullptr;
int originals=0,posts=0,others=0,opens=0;
bool details=false;
void require(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
void original(const SDL_Event& event) {
    require(&event==expected,"original received a different event"); ++originals;
}
void otherOriginal(SDL_Event& event) { original(event); }
HookAction otherObserver(ModContext*,void* args,void*,void*) {
    require(&mods::arg<const SDL_Event&>(args,0)==expected,"shared callback received a pointer envelope");
    ++others; return HOOK_CONTINUE;
}
ModResult pre(ModContext*,void*,void* args,void*,int* skip) {
    require(args && static_cast<void**>(args)[0]==expected,"trampoline passed pointer storage instead of event");
    *skip=0;
    for(auto callback:callbacks) if(callback(nullptr,args,nullptr,nullptr)==HOOK_SKIP_ORIGINAL) {
        *skip=1; break;
    }
    return MOD_OK;
}
ModResult post(ModContext*,void*,void* args,void*) {
    require(&mods::arg<const SDL_Event&>(args,0)==expected,"post-hook received wrong event");
    ++posts; return MOD_OK;
}
bool mapAction(tracker::MapAction action,float,float) {
    if(action==tracker::MapAction::Dismiss) {
        if(!details) return false;
        details=false; return true;
    }
    if(details) return false;
    details=true; ++opens; return true;
}
void run(bool trackerOwnsHook,bool trackerFirst) {
    tracker::resetInput(); tracker::setMapActionHandler(nullptr);
    originals=posts=others=opens=0; details=false;
    callbacks={otherObserver};
    auto emit=[&](SDL_Event& event,bool consumed=false) {
        expected=&event;
        SDL_Event before{}; std::memcpy(&before,&event,sizeof(event));
        const int oldOriginals=originals,oldPosts=posts;
        if(trackerOwnsHook) TrackerHook::trampoline(event); else OtherHook::trampoline(event);
        require(originals==oldOriginals+(consumed ? 0 : 1),"original forwarding/consumption changed");
        require(posts==oldPosts+1,"post-hook missing after input consumption");
        require(std::memcmp(&before,&event,sizeof(event))==0,"tracker changed input event bytes");
    };
    SDL_Event event{};
    event.type=SDL_EVENT_WINDOW_FOCUS_GAINED; emit(event);
    event={}; event.type=SDL_EVENT_GAMEPAD_BUTTON_UP;
    static_assert(SDL_EVENT_GAMEPAD_BUTTON_UP==0x652);
    event.gbutton.which=7; event.gbutton.button=SDL_GAMEPAD_BUTTON_SOUTH;
    emit(event); // another mod is already running before TPTracker is enabled
    require(!tracker::shortcutDown(0,1),"disabled tracker observed input");
    callbacks=trackerFirst ? std::vector<HookPreFn>{tracker::detail::inputEventHook,otherObserver}
                           : std::vector<HookPreFn>{otherObserver,tracker::detail::inputEventHook};
    emit(event); // exact faulting event from the user's log, after hot enabling
    event.type=SDL_EVENT_GAMEPAD_BUTTON_DOWN; emit(event);
    require(tracker::shortcutDown(0,1),"shared controller press not tracked");
    event.type=SDL_EVENT_GAMEPAD_BUTTON_UP; emit(event);
    require(!tracker::shortcutDown(0,1),"controller release not tracked");
    event={}; event.type=SDL_EVENT_KEY_DOWN; event.key.scancode=SDL_SCANCODE_F5; emit(event);
    require(tracker::shortcutDown(5,0),"shared keyboard input not tracked");
    event.type=SDL_EVENT_KEY_UP; emit(event);
    require(!tracker::shortcutDown(5,0),"keyboard release not tracked");
    tracker::setMapActionHandler(mapAction);
    event={}; event.type=SDL_EVENT_GAMEPAD_AXIS_MOTION; event.gaxis.which=7;
    event.gaxis.axis=SDL_GAMEPAD_AXIS_RIGHT_TRIGGER; event.gaxis.value=25000; emit(event);
    event={}; event.type=SDL_EVENT_GAMEPAD_BUTTON_DOWN; event.gbutton.which=7;
    event.gbutton.button=SDL_GAMEPAD_BUTTON_SOUTH; emit(event,true);
    require(details && opens==1,"map combo did not reach check info");
    event.type=SDL_EVENT_GAMEPAD_BUTTON_UP; emit(event);
    event.type=SDL_EVENT_GAMEPAD_BUTTON_DOWN; event.gbutton.button=SDL_GAMEPAD_BUTTON_EAST; emit(event,true);
    require(!details,"B did not close check info");
    event={}; event.type=SDL_EVENT_MOUSE_BUTTON_DOWN; event.button.button=SDL_BUTTON_LEFT;
    event.button.x=50; event.button.y=50; emit(event,true);
    require(details && opens==2,"mouse click did not reach check info");
    event.type=SDL_EVENT_MOUSE_BUTTON_UP; emit(event);
    event={}; event.type=SDL_EVENT_KEY_DOWN; event.key.scancode=SDL_SCANCODE_ESCAPE; emit(event,true);
    require(!details,"Escape did not close check info");
    event={}; event.type=SDL_EVENT_WINDOW_FOCUS_LOST; emit(event);
    event.type=SDL_EVENT_WINDOW_FOCUS_GAINED; emit(event);
    callbacks={otherObserver}; tracker::resetInput(); // detach leaves the other hook usable
    event={}; event.type=SDL_EVENT_GAMEPAD_BUTTON_UP; emit(event);
    callbacks.push_back(tracker::detail::inputEventHook); // enable again
    emit(event);
    require(others>0,"other mod callback was not called");
    tracker::setMapActionHandler(nullptr);
}
}
int main() {
    try {
        HookService service{}; service.dispatch_pre=pre; service.dispatch_post=post;
        TrackerHook::hooks=&service; TrackerHook::g_orig=original;
        OtherHook::hooks=&service; OtherHook::g_orig=otherOriginal;
        require(tracker::detail::inputEventHook(nullptr,nullptr,nullptr,nullptr)==HOOK_CONTINUE,"null envelope was not ignored");
        void* empty[]={nullptr};
        require(tracker::detail::inputEventHook(nullptr,empty,nullptr,nullptr)==HOOK_CONTINUE,"empty envelope was not ignored");
        for(bool trackerOwnsHook:{false,true}) for(bool trackerFirst:{false,true}) run(trackerOwnsHook,trackerFirst);
        std::cout << "Shared input hook verified: both owners and callback orders, runtime enable/re-enable, 0x652 release, keyboard/mouse/controller, original and post dispatch.\n";
    } catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}

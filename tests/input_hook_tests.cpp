#include "input_hook.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <vector>

extern "C" { ModContext* mod_ctx=nullptr; }
namespace {
using TrackerHook=tracker::detail::InputEventHook;
struct OtherTag {};
using OtherHook=mods::HookImpl<OtherTag,bool,SDL_Event&>;
// Essentials 3.0.2 uses this pointer envelope on the separate UI target.
struct LegacyUiTag {};
using LegacyUiHook=mods::HookImpl<LegacyUiTag,void,SDL_Event*>;
static_assert(std::is_same_v<decltype(&TrackerHook::trampoline),bool(*)(const SDL_Event&)>);
static_assert(std::is_same_v<decltype(TrackerHook::g_orig),bool(*)(const SDL_Event&)>);
std::vector<HookPostFn> callbacks;
const SDL_Event* expected=nullptr;
int originals=0,posts=0,others=0,opens=0,legacyEvents=0;
bool details=false,auxiliary=false,preConsumed=false;
void require(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
bool original(const SDL_Event& event) {
    require(&event==expected,"original received a different event"); ++originals;
    return auxiliary;
}
bool otherOriginal(SDL_Event& event) { return original(event); }
void otherObserver(ModContext*,void* args,void*,void*) {
    require(&mods::arg<const SDL_Event&>(args,0)==expected,"shared callback received a pointer envelope");
    ++others;
}
ModResult pre(ModContext*,void*,void* args,void* result,int* skip) {
    require(args && static_cast<void**>(args)[0]==expected,"trampoline passed pointer storage instead of event");
    require(result!=nullptr,"missing router return value");
    *skip=preConsumed;
    if(preConsumed) *static_cast<bool*>(result)=true;
    return MOD_OK;
}
ModResult post(ModContext*,void*,void* args,void* result) {
    require(&mods::arg<const SDL_Event&>(args,0)==expected,"post-hook received wrong event");
    for(auto callback:callbacks) callback(nullptr,args,result,nullptr);
    ++posts; return MOD_OK;
}
ModResult legacyPre(ModContext*,void*,void* args,void*,int* skip) {
    // This would read 0x305 as a pointer with the old shared reference target.
    auto* event=mods::arg<SDL_Event*>(args,0);
    require(event==expected,"legacy UI mod received incompatible reference envelope");
    require(event->type==expected->type,"legacy UI callback could not read event");
    ++legacyEvents; *skip=0; return MOD_OK;
}
ModResult legacyPost(ModContext*,void*,void*,void*) { return MOD_OK; }
void legacyOriginal(SDL_Event* event) { require(event==expected,"legacy UI original lost event"); }
bool mapAction(tracker::MapAction action,float,float) {
    if(action==tracker::MapAction::Dismiss) {
        if(!details) return false;
        details=false; return true;
    }
    if(details) return false;
    details=true; ++opens; return true;
}
void run(bool trackerOwnsHook,bool trackerFirst) {
    tracker::resetInput(); tracker::setCheckBindings(0,0); tracker::setMapActionHandler(nullptr);
    originals=posts=others=opens=legacyEvents=0; details=auxiliary=preConsumed=false;
    callbacks={otherObserver};
    auto emit=[&](SDL_Event& event,bool consumed=false) {
        expected=&event;
        SDL_Event before{}; std::memcpy(&before,&event,sizeof(event));
        const int oldOriginals=originals,oldPosts=posts,oldLegacy=legacyEvents;
        const bool handled=trackerOwnsHook ? TrackerHook::trampoline(event) : OtherHook::trampoline(event);
        require(handled==consumed,"router consumption changed");
        require(originals==oldOriginals+(preConsumed ? 0 : 1),"auxiliary routing was skipped or repeated");
        require(posts==oldPosts+1,"post-hook missing after input consumption");
        // Match main01/launchUILoop: only unhandled events reach the UI hook.
        if(!handled) LegacyUiHook::trampoline(&event);
        require(legacyEvents==oldLegacy+(handled ? 0 : 1),"legacy UI forwarding changed");
        require(std::memcmp(&before,&event,sizeof(event))==0,"tracker changed input event bytes");
    };
    SDL_Event event{};
    event.type=SDL_EVENT_WINDOW_FOCUS_GAINED; emit(event);
    event={}; event.type=SDL_EVENT_GAMEPAD_BUTTON_UP;
    static_assert(SDL_EVENT_GAMEPAD_BUTTON_UP==0x652);
    event.gbutton.which=7; event.gbutton.button=SDL_GAMEPAD_BUTTON_SOUTH;
    emit(event);
    require(!tracker::shortcutDown(0,1),"disabled tracker observed input");
    callbacks=trackerFirst ? std::vector<HookPostFn>{tracker::detail::inputEventHook,otherObserver}
                           : std::vector<HookPostFn>{otherObserver,tracker::detail::inputEventHook};
    event={}; event.type=SDL_EVENT_KEYBOARD_ADDED;
    static_assert(SDL_EVENT_KEYBOARD_ADDED==0x305);
    emit(event); // exact startup event from both supplied crash logs
    event={}; event.type=SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    event.gbutton.which=7; event.gbutton.button=SDL_GAMEPAD_BUTTON_SOUTH; emit(event);
    require(tracker::shortcutDown(0,1),"shared controller press not tracked");
    event.type=SDL_EVENT_GAMEPAD_BUTTON_UP; emit(event);
    require(!tracker::shortcutDown(0,1),"controller release not tracked");
    event={}; event.type=SDL_EVENT_KEY_DOWN; event.key.scancode=SDL_SCANCODE_F5;
    auxiliary=true; emit(event,true);
    require(!tracker::shortcutDown(5,0),"auxiliary-window input leaked into tracker");
    auxiliary=false; preConsumed=true; emit(event,true);
    require(!tracker::shortcutDown(5,0),"another mod's consumed input leaked into tracker");
    preConsumed=false; emit(event);
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
    callbacks={otherObserver}; tracker::resetInput();
    event={}; event.type=SDL_EVENT_KEYBOARD_ADDED; emit(event);
    callbacks.push_back(tracker::detail::inputEventHook); emit(event);
    require(others>0 && legacyEvents>0,"other mod callbacks were not called");
    tracker::setMapActionHandler(nullptr);
}
}
int main() {
    try {
        HookService service{}; service.dispatch_pre=pre; service.dispatch_post=post;
        TrackerHook::hooks=&service; TrackerHook::g_orig=original;
        OtherHook::hooks=&service; OtherHook::g_orig=otherOriginal;
        HookService legacy{}; legacy.dispatch_pre=legacyPre; legacy.dispatch_post=legacyPost;
        LegacyUiHook::hooks=&legacy; LegacyUiHook::g_orig=legacyOriginal;
        bool handled=false;
        tracker::detail::inputEventHook(nullptr,nullptr,&handled,nullptr);
        require(!handled,"null envelope was not ignored");
        void* empty[]={nullptr};
        tracker::detail::inputEventHook(nullptr,empty,&handled,nullptr);
        require(!handled,"empty envelope was not ignored");
        tracker::detail::inputEventHook(nullptr,empty,nullptr,nullptr);
        for(bool trackerOwnsHook:{false,true}) for(bool trackerFirst:{false,true}) run(trackerOwnsHook,trackerFirst);
        std::cout << "Input router verified: legacy UI hooks, 0x305 startup, both owners and callback orders, auxiliary/consumed events, reload, keyboard/mouse/controller.\n";
    } catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}

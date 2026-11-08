#pragma once
#include "controller.hpp"
#include <SDL3/SDL_events.h>
#include <mods/svc/hook.hpp>

namespace tracker::detail {
// Observe the host's window router, before main-window events reach the UI.
// Some UI mods declare dusk::ui::handle_event's reference as a pointer. Joining
// that shared target makes their callbacks dereference the event type (0x305
// at startup) as an address. Keep our correctly typed envelope on this separate
// target, and let the host handle auxiliary windows before inspecting input.
DEFINE_HOOK_SYMBOL("dusk::mods::svc::window_dispatch_event", bool(const SDL_Event&), InputEventHookBase);
struct InputEventHook : InputEventHookBase {
    static bool trampoline(const SDL_Event& event) {
        // The pinned SDK's generic trampoline cannot erase const references to
        // void*. Erase const only for the hook envelope; decoding stays const.
        // Preserve the SDK's original-call and post-hook behavior, including skip.
        void* args[]={const_cast<SDL_Event*>(std::addressof(event))};
        bool handled=false;
        if(!dispatch_pre(args,&handled)) handled=g_orig(event);
        dispatch_post(args,&handled);
        return handled;
    }
};
inline void inputEventHook(ModContext*,void* args,void* result,void*) {
    if(!args || !static_cast<void**>(args)[0] || !result || *static_cast<bool*>(result)) return;
    // true tells the main loop that this event is consumed. Unhandled events
    // continue to mouse, UI and other mods normally; event bytes stay intact.
    if(processInput(mods::arg<const SDL_Event&>(args,0))) *static_cast<bool*>(result)=true;
}
}

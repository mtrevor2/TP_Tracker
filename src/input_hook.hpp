#pragma once
#include "controller.hpp"
#include <SDL3/SDL_events.h>
#include <mods/svc/hook.hpp>

namespace tracker::detail {
// Match dusk::ui::handle_event(const SDL_Event&) exactly. The host shares one
// argument array between all mods, regardless of which mod installed first.
DEFINE_HOOK_SYMBOL("dusk::ui::handle_event", void(const SDL_Event&), InputEventHookBase);
struct InputEventHook : InputEventHookBase {
    static void trampoline(const SDL_Event& event) {
        // The pinned SDK's generic trampoline cannot erase const references to
        // void*. Erase const only for the hook envelope; decoding stays const.
        // Preserve the SDK's original-call and post-hook behavior, including skip.
        void* args[]={const_cast<SDL_Event*>(std::addressof(event))};
        if(!dispatch_pre(args,nullptr)) g_orig(event);
        dispatch_post(args,nullptr);
    }
};
inline HookAction inputEventHook(ModContext*,void* args,void*,void*) {
    if(!args || !static_cast<void**>(args)[0]) return HOOK_CONTINUE;
    return processInput(mods::arg<const SDL_Event&>(args,0)) ? HOOK_SKIP_ORIGINAL : HOOK_CONTINUE;
}
}

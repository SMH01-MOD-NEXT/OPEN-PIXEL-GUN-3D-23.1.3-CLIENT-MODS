#pragma once

#include <atomic>
#include <cstdint>

#include "hook.h"
#include "log.h"

// Passive diagnostics for the PG3D 23.1.3 authorization scene.
//
// Measured on device: the boot reaches AuthSceneController.Awake and stops
// there. The scene state setter is never called, no state is ever applied and
// MainMenuController.Awake never runs, so the scene waits for a condition that
// never becomes true. These hooks name the step that never completes. Nothing
// here changes client behavior: every hook logs and then calls the stock body.
namespace auth_scene_trace_2313 {
namespace detail {

using MethodInfo = void;
using InstanceVoidFn = void (*)(void* self, const MethodInfo* method);
using InstanceStateFn = void (*)(void* self, int32_t state,
                                 const MethodInfo* method);
using InstanceIteratorFn = void* (*)(void* self, const MethodInfo* method);
using StaticStateReaderFn = int32_t (*)(const MethodInfo* method);

inline InstanceVoidFn g_start = nullptr;
inline InstanceStateFn g_apply_state = nullptr;
inline InstanceVoidFn g_first_step = nullptr;
inline InstanceVoidFn g_second_step = nullptr;
inline InstanceIteratorFn g_coroutine = nullptr;
inline StaticStateReaderFn g_state_reader = nullptr;
inline std::atomic<int32_t> g_last_observed{INT32_MIN};

const char* state_name(int32_t state) {
    switch (state) {
        case -1: return "None";
        case 0: return "Initial";
        case 1: return "Authorizing";
        case 2: return "Authorized";
        case 3: return "FullySynchronized";
        case 4: return "Empty";
        case 5: return "CheckBindedId";
        case 6: return "ChooseId";
        case 7: return "ChooseProgress";
        case 8: return "SendCachedCommands";
        case 9: return "SynchronizeProgress";
        case 10: return "CheckAppVersion";
        case 11: return "Easy";
        case 12: return "CheckConnection";
        case 13: return "SendProgress";
        case 14: return "WaitAsync";
        case 15: return "TechnicalWorks";
        case 16: return "Login";
        default: return "Unknown";
    }
}

template <typename Fn>
void* replacement(Fn fn) {
    return reinterpret_cast<void*>(fn);
}

template <typename Fn>
void** original_slot(Fn* fn) {
    return reinterpret_cast<void**>(fn);
}

void hook_start(void* self, const MethodInfo* method) {
    LOGI("23.1.3-authscene: Start ENTER self=%p", self);
    if (g_start != nullptr) g_start(self, method);
    LOGI("23.1.3-authscene: Start RETURN");
}

void hook_apply_state(void* self, int32_t state, const MethodInfo* method) {
    LOGI("23.1.3-authscene: state dispatch ENTER %d (%s)", state,
         state_name(state));
    if (g_apply_state != nullptr) g_apply_state(self, state, method);
    LOGI("23.1.3-authscene: state dispatch RETURN %d (%s)", state,
         state_name(state));
}

void hook_first_step(void* self, const MethodInfo* method) {
    LOGI("23.1.3-authscene: first startup step ENTER self=%p", self);
    if (g_first_step != nullptr) g_first_step(self, method);
    LOGI("23.1.3-authscene: first startup step RETURN");
}

void hook_second_step(void* self, const MethodInfo* method) {
    LOGI("23.1.3-authscene: second startup step ENTER self=%p", self);
    if (g_second_step != nullptr) g_second_step(self, method);
    LOGI("23.1.3-authscene: second startup step RETURN");
}

void* hook_coroutine(void* self, const MethodInfo* method) {
    LOGI("23.1.3-authscene: scene coroutine requested self=%p", self);
    return g_coroutine != nullptr ? g_coroutine(self, method) : nullptr;
}

int32_t hook_state_reader(const MethodInfo* method) {
    const int32_t state =
        g_state_reader != nullptr ? g_state_reader(method) : -1;
    const int32_t previous =
        g_last_observed.exchange(state, std::memory_order_relaxed);
    if (state != previous) {
        LOGI("23.1.3-authscene: observed state %d (%s)", state,
             state_name(state));
    }
    return state;
}

} // namespace detail

inline bool install_hooks() {
    int armed = 0;
    if (hook::install({"", "AuthSceneController", "Start", 0},
                      detail::replacement(&detail::hook_start),
                      detail::original_slot(&detail::g_start), false)) {
        ++armed;
    }
    if (hook::install({"", "AuthSceneController", u8"\u4e0e\u4e1b\u4e00\u4e02\u4e0d\u4e04\u4e0a\u4e0a\u4e07", 1},
                      detail::replacement(&detail::hook_apply_state),
                      detail::original_slot(&detail::g_apply_state), false)) {
        ++armed;
    }
    if (hook::install({"", "AuthSceneController", u8"\u4e06\u4e08\u4e09\u4e0d\u4e13\u4e18\u4e1e\u4e16\u4e0e", 0},
                      detail::replacement(&detail::hook_first_step),
                      detail::original_slot(&detail::g_first_step), false)) {
        ++armed;
    }
    if (hook::install({"", "AuthSceneController", u8"\u4e0d\u4e10\u4e04\u4e1d\u4e07\u4e19\u4e15\u4e17\u4e01", 0},
                      detail::replacement(&detail::hook_second_step),
                      detail::original_slot(&detail::g_second_step), false)) {
        ++armed;
    }
    if (hook::install({"", "AuthSceneController", u8"\u4e1b\u4e07\u4e14\u4e0b\u4e05\u4e18\u4e13\u4e0c\u4e08", 0},
                      detail::replacement(&detail::hook_coroutine),
                      detail::original_slot(&detail::g_coroutine), false)) {
        ++armed;
    }
    if (hook::install({"", "AuthSceneController", u8"\u4e12\u4e0e\u4e14\u4e19\u4e1a\u4e01\u4e0f\u4e01\u4e09", 0},
                      detail::replacement(&detail::hook_state_reader),
                      detail::original_slot(&detail::g_state_reader), false)) {
        ++armed;
    }
    LOGI("23.1.3-authscene: auth scene trace armed on %d/6 entry point(s) "
         "(Start, state dispatch, two startup steps, scene coroutine, state "
         "reader); passive passthrough, the stock body always runs",
         armed);
    return armed > 0;
}

} // namespace auth_scene_trace_2313

#pragma once

#include <atomic>
#include <cstdint>

#include "hook.h"
#include "log.h"

// Passive diagnostics for the one step the 23.1.3 authorization scene never
// finishes.
//
// Measured with auth_scene_trace_2313: Start runs, the state dispatcher is
// entered exactly once with 2 (Authorized), returns, and the scene then waits
// forever -- no further state, no scene coroutine, no main menu. Disassembly of
// the dispatcher shows the branch is gated on a static bool and, when it
// passes, starts an iterator whose MoveNext is followed here step by step, plus
// the two collaborators it calls. Nothing here changes client behavior: every
// hook logs and then calls the stock body.
namespace auth_state_trace_2313 {
namespace detail {

using MethodInfo = void;
using StaticBoolFn = bool (*)(const MethodInfo* method);
using InstanceIteratorFn = void* (*)(void* self, const MethodInfo* method);
using InstanceBoolFn = bool (*)(void* self, const MethodInfo* method);
using InstanceVoidFn = void (*)(void* self, const MethodInfo* method);
using InstanceIntFn = void (*)(void* self, int32_t value,
                               const MethodInfo* method);

// Compiler-generated iterators keep their step counter in the first field of
// the state machine object, the same offset the loading stall guard reads.
constexpr int kIteratorStateOffset = 0x10;

inline StaticBoolFn g_gate = nullptr;
inline InstanceIteratorFn g_coroutine = nullptr;
inline InstanceBoolFn g_move_next = nullptr;
inline InstanceVoidFn g_scene_step = nullptr;
inline InstanceIntFn g_interface = nullptr;
inline std::atomic<int32_t> g_last_gate{-1};
inline std::atomic<int32_t> g_steps{0};

template <typename Fn>
void* replacement(Fn fn) {
    return reinterpret_cast<void*>(fn);
}

template <typename Fn>
void** original_slot(Fn* fn) {
    return reinterpret_cast<void**>(fn);
}

int32_t iterator_state(void* self) {
    if (self == nullptr) {
        return INT32_MIN;
    }
    return *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(self) +
                                      kIteratorStateOffset);
}

bool hook_gate(const MethodInfo* method) {
    const bool value = g_gate != nullptr ? g_gate(method) : false;
    const int32_t as_int = value ? 1 : 0;
    // The gate belongs to a shared flag helper called from all over the game,
    // so report the value only when it changes.
    if (g_last_gate.exchange(as_int, std::memory_order_relaxed) != as_int) {
        LOGI("23.1.3-authstate: dispatcher gate now returns %s",
             value ? "true" : "false");
    }
    return value;
}

void* hook_coroutine(void* self, const MethodInfo* method) {
    LOGI("23.1.3-authstate: the Authorized branch requested its coroutine "
         "self=%p",
         self);
    return g_coroutine != nullptr ? g_coroutine(self, method) : nullptr;
}

bool hook_move_next(void* self, const MethodInfo* method) {
    const int32_t before = iterator_state(self);
    const bool result = g_move_next != nullptr ? g_move_next(self, method) : false;
    const int32_t after = iterator_state(self);
    const int32_t step = g_steps.fetch_add(1, std::memory_order_relaxed) + 1;
    // A coroutine parked on a yield keeps returning true without moving, which
    // would flood the log: report the opening steps and every state change.
    if (step <= 8 || before != after) {
        LOGI("23.1.3-authstate: coroutine step #%d state %d -> %d result=%d",
             step, before, after, result ? 1 : 0);
    }
    return result;
}

void hook_scene_step(void* self, const MethodInfo* method) {
    LOGI("23.1.3-authstate: scene collaborator step ENTER self=%p", self);
    if (g_scene_step != nullptr) g_scene_step(self, method);
    LOGI("23.1.3-authstate: scene collaborator step RETURN");
}

void hook_interface(void* self, int32_t value, const MethodInfo* method) {
    LOGI("23.1.3-authstate: auth interface asked for screen %d", value);
    if (g_interface != nullptr) g_interface(self, value, method);
    LOGI("23.1.3-authstate: auth interface screen %d handled", value);
}

} // namespace detail

inline bool install_hooks() {
    int armed = 0;
    if (hook::install({"", u8"\u4e19\u4e00\u4e1a\u4e09\u4e05\u4e07\u4e02\u4e19\u4e1e", u8"\u4e14\u4e1c\u4e19\u4e13\u4e11\u4e09\u4e19\u4e06\u4e15", 0},
                      detail::replacement(&detail::hook_gate),
                      detail::original_slot(&detail::g_gate), false)) {
        ++armed;
    }
    if (hook::install({"", "AuthSceneController", u8"\u4e07\u4e11\u4e0c\u4e1b\u4e0b\u4e0b\u4e0f\u4e08\u4e0a", 0},
                      detail::replacement(&detail::hook_coroutine),
                      detail::original_slot(&detail::g_coroutine), false)) {
        ++armed;
    }
    if (hook::install({"", u8"AuthSceneController.\u4e0a\u4e04\u4e0d\u4e13\u4e19\u4e14\u4e19\u4e1a\u4e03", "MoveNext", 0},
                      detail::replacement(&detail::hook_move_next),
                      detail::original_slot(&detail::g_move_next), false)) {
        ++armed;
    }
    if (hook::install({"", u8"\u4e00\u4e01\u4e16\u4e0e\u4e1c\u4e0f\u4e1a\u4e0c\u4e19", u8"\u4e1d\u4e10\u4e0b\u4e02\u4e0b\u4e04\u4e01\u4e0f\u4e0b", 0},
                      detail::replacement(&detail::hook_scene_step),
                      detail::original_slot(&detail::g_scene_step), false)) {
        ++armed;
    }
    if (hook::install({"PGCompany.AuthorizationScene", "AuthInterfaceController", u8"\u4e07\u4e19\u4e08\u4e02\u4e12\u4e0c\u4e16\u4e0d\u4e05", 1},
                      detail::replacement(&detail::hook_interface),
                      detail::original_slot(&detail::g_interface), false)) {
        ++armed;
    }
    LOGI("23.1.3-authstate: auth state trace armed on %d/5 entry point(s) "
         "(dispatcher gate, Authorized coroutine, its MoveNext, scene "
         "collaborator, auth interface); passive passthrough",
         armed);
    return armed > 0;
}

} // namespace auth_state_trace_2313

#pragma once

// Observational loading-coroutine diagnostics for PG3D 23.1.3.
// Every wrapper reads fields, calls stock MoveNext exactly once with the
// original arguments, logs its unmodified result, and returns that result.
// No iterator state, game field, yield decision, or control-flow edge is changed.

#include <atomic>
#include <cinttypes>
#include <cstdint>
#include <cstring>

#include "hook.h"
#include "il2cpp.h"
#include "log.h"

namespace startup_trace_2313 {
namespace detail {

using MethodInfo = void;
using MoveNextFn = bool (*)(void* self, const MethodInfo* method);

// Filled by hook::install at runtime.
inline MoveNextFn g_sw_start_next = nullptr;  // Switcher.Start iterator
inline MoveNextFn g_sw_init_next  = nullptr;  // Switcher.InitializeSwitcher
inline MoveNextFn g_sw_menu_next  = nullptr;  // Switcher.LoadMainMenu
inline MoveNextFn g_am_start_next = nullptr;  // AppsMenu.Start iterator

// Per-hook throttle: suppress identical back-to-back log lines.
struct Throttle {
    void*    last_self   = nullptr;
    int32_t  last_state  = INT32_MIN;
    bool     last_result = true;
    float    last_value  = -1000.0f;
    uint64_t last_log_ms = 0u;
};

inline Throttle g_th_sw_start;
inline Throttle g_th_sw_init;
inline Throttle g_th_sw_menu;
inline Throttle g_th_am_start;

// Safe field read by byte offset from a managed IL2CPP object pointer.
template <typename T>
static T read_at(const void* base, size_t offset, T fallback) noexcept {
    if (base == nullptr) return fallback;
    T v = fallback;
    std::memcpy(&v, static_cast<const uint8_t*>(base) + offset, sizeof(T));
    return v;
}

// Throttling: emit at most once per 2 s unless state/value/result changed.
static constexpr uint64_t kIntervalMs = 2000u;

static bool need_emit(Throttle& t, void* self,
                      int32_t state, float value, bool result) noexcept {
    const uint64_t now = opg3d_log::monotonic_ms();
    const float    d   = value > t.last_value ? value - t.last_value
                                              : t.last_value - value;
    const bool emit =
        self     != t.last_self      ||
        state    != t.last_state     ||
        result   != t.last_result    ||
        d        >= 0.005f           ||
        t.last_log_ms == 0u          ||
        now < t.last_log_ms          ||
        now - t.last_log_ms >= kIntervalMs;
    if (emit) {
        t.last_self   = self;
        t.last_state  = state;
        t.last_result = result;
        t.last_value  = value;
        t.last_log_ms = now;
    }
    return emit;
}

// -- hook bodies ------------------------------------------------------------

// Switcher.\u4e0b\u4e09\u4e0a\u4e1c\u4e13\u4e1a\u4e15\u4e09\u4e0e.MoveNext()
// Drives the top-level loading coroutine (0 -> 100% bar).
bool hook_sw_start(void* self, const MethodInfo* method) {
    const int32_t state    = read_at<int32_t>(self,     0x10, INT32_MIN);
    void*   const switcher = read_at<void*>  (self,     0x20, nullptr);
    const float   min_sec  = read_at<float>  (switcher, 0x128, -1.0f);
    const float   bar      = read_at<float>  (switcher, 0x138, -1.0f);
    void*   const ns_raw   = read_at<void*>  (switcher, 0x150, nullptr);

    const bool result = g_sw_start_next != nullptr
                            ? g_sw_start_next(self, method)
                            : false;

    if (need_emit(g_th_sw_start, self, state, bar, result)) {
        LOGI("23.1.3-swt: Start state=%d bar=%.3f minSec=%.3f "
             "next='%s' result=%d",
             state, bar, min_sec,
             il2cpp::to_utf8(ns_raw, 128).c_str(),
             result ? 1 : 0);
    }
    return result;
}

// Switcher.\u4e1c\u4e03\u4e17\u4e0c\u4e11\u4e05\u4e1e\u4e07\u4e01.MoveNext()
// Yields float sub-steps during InitializeSwitcher().
bool hook_sw_init(void* self, const MethodInfo* method) {
    const int32_t state    = read_at<int32_t>(self,     0x10, INT32_MIN);
    void*   const switcher = read_at<void*>  (self,     0x20, nullptr);
    const uint8_t rancho   = read_at<uint8_t>(self,     0x28, 0);
    const float   min_sec  = read_at<float>  (switcher, 0x128, -1.0f);
    const float   bar      = read_at<float>  (switcher, 0x138, -1.0f);

    const bool result = g_sw_init_next != nullptr
                            ? g_sw_init_next(self, method)
                            : false;

    if (need_emit(g_th_sw_init, self, state, bar, result)) {
        LOGI("23.1.3-swt: InitSwitcher state=%d bar=%.3f minSec=%.3f "
             "ranchoOk=%d result=%d",
             state, bar, min_sec, rancho ? 1 : 0, result ? 1 : 0);
    }
    return result;
}

// Switcher.\u4e1d\u4e01\u4e09\u4e1f\u4e10\u4e15\u4e01\u4e0e\u4e0f.MoveNext()
// Final step: waits out the minimum loading time, then runs the tail that
// requests the main menu scene. That scene load destroys the Switcher, so the
// last call of this hook is expected to be the one that advances to state 2.
bool hook_sw_menu(void* self, const MethodInfo* method) {
    const int32_t state    = read_at<int32_t>(self,     0x10, INT32_MIN);
    void*   const switcher = read_at<void*>  (self,     0x20, nullptr);
    const float   min_sec  = read_at<float>  (switcher, 0x128, -1.0f);
    const float   bar      = read_at<float>  (switcher, 0x138, -1.0f);

    const bool result = g_sw_menu_next != nullptr
                            ? g_sw_menu_next(self, method)
                            : false;

    if (need_emit(g_th_sw_menu, self, state, bar, result)) {
        LOGI("23.1.3-swt: LoadMainMenu state=%d bar=%.3f minSec=%.3f result=%d",
             state, bar, min_sec, result ? 1 : 0);
    }
    return result;
}

// AppsMenu.\u4e19\u4e0b\u4e19\u4e07\u4e09\u4e06\u4e11\u4e01\u4e1a.MoveNext()
// Traces the AppsMenu coroutine flow after both String.Compare gates are
// patched by install_early_signature_patch(). Confirms the managed code
// actually advances past the gates that were patched in the native binary.
bool hook_am_start(void* self, const MethodInfo* method) {
    const int32_t state = read_at<int32_t>(self, 0x10, INT32_MIN);

    const bool result = g_am_start_next != nullptr
                            ? g_am_start_next(self, method)
                            : false;

    if (need_emit(g_th_am_start, self, state, -1.0f, result)) {
        LOGI("23.1.3-swt: AppsMenu.Start state=%d result=%d",
             state, result ? 1 : 0);
    }
    return result;
}

template <typename Fn>
static void* repl(Fn fn)  { return reinterpret_cast<void*>(fn); }
template <typename Fn>
static void** orig(Fn* p) { return reinterpret_cast<void**>(p); }

} // namespace detail

// Call once after Assembly-CSharp.dll is ready (from main.cpp init_thread).
inline bool install_hooks() {
    int installed = 0;

    // Switcher.Start iterator -- drives the 0-100% loading progress bar.
    // Nested class separator is '/' so il2cpp::find_class splits on it.
    if (hook::install(
            {"" , u8"Switcher/\u4e0b\u4e09\u4e0a\u4e1c\u4e13\u4e1a\u4e15\u4e09\u4e0e",
             "MoveNext", 0},
            detail::repl(&detail::hook_sw_start),
            detail::orig(&detail::g_sw_start_next), false)) {
        ++installed;
        LOGI("23.1.3-swt: Switcher.Start heartbeat armed");
    } else {
        LOGE("23.1.3-swt: Switcher.Start MoveNext hook failed "
             "-- 90%% stall attribution will be impaired");
    }

    // Switcher.InitializeSwitcher iterator.
    if (hook::install(
            {"", u8"Switcher/\u4e1c\u4e03\u4e17\u4e0c\u4e11\u4e05\u4e1e\u4e07\u4e01",
             "MoveNext", 0},
            detail::repl(&detail::hook_sw_init),
            detail::orig(&detail::g_sw_init_next), false)) {
        ++installed;
        LOGI("23.1.3-swt: Switcher.InitializeSwitcher heartbeat armed");
    } else {
        LOGW("23.1.3-swt: Switcher.InitializeSwitcher MoveNext trace unavailable");
    }

    // Switcher.LoadMainMenu iterator -- the last coroutine before the menu
    // scene, and the one that dies if InitializeSwitcher was cut short.
    if (hook::install(
            {"", u8"Switcher/\u4e1d\u4e01\u4e09\u4e1f\u4e10\u4e15\u4e01\u4e0e\u4e0f",
             "MoveNext", 0},
            detail::repl(&detail::hook_sw_menu),
            detail::orig(&detail::g_sw_menu_next), false)) {
        ++installed;
        LOGI("23.1.3-swt: Switcher.LoadMainMenu heartbeat + abort detection "
             "armed");
    } else {
        LOGE("23.1.3-swt: Switcher.LoadMainMenu MoveNext hook failed "
             "-- a dying menu coroutine will not be reported");
    }

    // AppsMenu.Start iterator -- confirms native patch carries through managed.
    if (hook::install(
            {"", u8"AppsMenu/\u4e19\u4e0b\u4e19\u4e07\u4e09\u4e06\u4e11\u4e01\u4e1a",
             "MoveNext", 0},
            detail::repl(&detail::hook_am_start),
            detail::orig(&detail::g_am_start_next), false)) {
        ++installed;
        LOGI("23.1.3-swt: AppsMenu.Start heartbeat armed");
    }

    LOGI("23.1.3-swt: %d/4 coroutine heartbeats installed "
         "(Switcher.Start + InitializeSwitcher + LoadMainMenu + "
         "AppsMenu.Start; state/value logged every 2 s or on change)",
         installed);

    return installed > 0;
}

} // namespace startup_trace_2313

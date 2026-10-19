#pragma once

#include "hook.h"
#include "il2cpp.h"
#include "log.h"

// 23.1.3 AppsMenu skips Stopwatch.Start when the persistent
// MainMenuControllerOnceEnabled key exists. A Switcher instantiated on that
// path nevertheless waits for ElapsedMilliseconds >= minSec at state 23.
// Resume only that clock at Switcher entry; do not skip the wait, change auth
// states, clear preferences, or touch any progress model.
namespace startup_clock_2313 {
namespace detail {
using IteratorFn = void* (*)(void*, const void*);
using RunningFn = bool (*)(void*, const void*);
using StartFn = void (*)(void*, const void*);
inline IteratorFn original_initialize = nullptr;
inline void* clock_field = nullptr;
inline const void* running_info = nullptr;
inline const void* start_info = nullptr;
inline RunningFn is_running = nullptr;
inline StartFn start = nullptr;

inline bool resume_clock(void* clock, RunningFn running, StartFn resume,
                         const void* running_method, const void* start_method) {
    if (!clock || !running || !resume) return false;
    if (running(clock, running_method)) return false;
    resume(clock, start_method); // Start preserves elapsed; Reset/Restart would not.
    return true;
}

inline void* initialize(void* self, const void* method) {
    if (!original_initialize) return nullptr;
    void* iterator = original_initialize(self, method);
    // Read after the stock factory, on Unity's calling thread. Metadata lookup
    // at install time never evaluates a static field or runs a constructor.
    if (iterator && clock_field && il2cpp::field_static_get_value) {
        void* clock = nullptr;
        il2cpp::field_static_get_value(clock_field, &clock);
        if (resume_clock(clock, is_running, start, running_info, start_info)) {
            LOGI("23.1.3-startup-clock: resumed stopped Switcher timer; "
                 "stock loading delay and progress unchanged");
        }
    }
    return iterator;
}
} // namespace detail

inline bool install() {
    using namespace detail;
    if (!il2cpp::field_static_get_value) return false;
    clock_field = il2cpp::find_field("", "Switcher", u8"世丆业丞上丘三业丄");
    auto* running = il2cpp::find_method_info("System.Diagnostics", "Stopwatch", "get_IsRunning", 0);
    auto* starting = il2cpp::find_method_info("System.Diagnostics", "Stopwatch", "Start", 0);
    is_running = reinterpret_cast<RunningFn>(il2cpp::method_pointer(running));
    start = reinterpret_cast<StartFn>(il2cpp::method_pointer(starting));
    running_info = running;
    start_info = starting;
    if (!clock_field || !is_running || !start) {
        LOGE("23.1.3-startup-clock: metadata unavailable; left stock");
        return false;
    }
    return hook::install({"", "Switcher", "InitializeSwitcher", 0},
        reinterpret_cast<void*>(&initialize),
        reinterpret_cast<void**>(&original_initialize), true);
}
} // namespace startup_clock_2313

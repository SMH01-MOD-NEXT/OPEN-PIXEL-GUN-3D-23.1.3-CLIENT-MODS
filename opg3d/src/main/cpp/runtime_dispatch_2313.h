#pragma once

#include <atomic>
#include "hook.h"
#include "log.h"

// Game metadata setup must not compete with Unity's startup generic inflation.
// The worker only installs this engine-level entry. The actual game hooks are
// resolved/installed once on the next UnitySynchronizationContext.Exec call.
// No sleeps on Unity's thread, synthetic frames, managed invocation from the
// worker, signal suppression or MethodInfo pointer rewriting are involved.
namespace runtime_dispatch_2313 {
namespace detail {
using Job = void (*)();
using Exec = void (*)(void*, const void*);
inline std::atomic<Job> pending{nullptr};
inline std::atomic<bool> scheduled{false};
inline Exec original = nullptr;

inline bool run_pending() {
    Job job = pending.exchange(nullptr, std::memory_order_acq_rel);
    if (!job) return false;
    // Remove BEFORE running: nested Exec calls cannot recursively install.
    job();
    return true;
}

inline void exec(void* self, const void* method) {
    if (run_pending()) LOGI("23.1.3-runtime: game hooks initialized on Unity thread");
    original(self, method); // Every stock Exec still runs, with original args.
}
} // namespace detail

inline bool schedule(detail::Job job) {
    using namespace detail;
    if (!job) return false;
    bool expected = false;
    if (!scheduled.compare_exchange_strong(expected, true)) return false;
    // Publish the callback and its bootstrap state before the hook can fire.
    pending.store(job, std::memory_order_release);
    if (!hook::install({"UnityEngine", "UnitySynchronizationContext", "Exec", 0},
            reinterpret_cast<void*>(&exec), reinterpret_cast<void**>(&original), true)) {
        pending.store(nullptr, std::memory_order_release);
        scheduled.store(false);
        LOGE("23.1.3-runtime: Unity dispatch hook failed; game hooks not initialized");
        return false;
    }
    LOGI("23.1.3-runtime: game hook setup queued for Unity thread");
    return true;
}
} // namespace runtime_dispatch_2313

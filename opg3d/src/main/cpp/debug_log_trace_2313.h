#pragma once

#include <atomic>
#include <cstdint>
#include <cstring>
#include <string>

#include "hook.h"
#include "il2cpp.h"
#include "log.h"

// Names the managed component behind one specific Unity log line.
//
// Run 28 proved the boot config walk in BalanceController is embedded-only: the
// per-id request 0x471C074 is never called during loading, and the static gate
// 0x471CAC0 is driven by a fixed list of Balance documents from the Start()
// iterator at 0x486FC98. So whatever logs `Start wait config response` is not
// that pipeline, and the managed string is the only handle left on it.
//
// UnityEngine.Debug.Log(object) is hooked and filtered down to the few messages
// that matter. The identification comes from the log line's own pc= field:
// inside an inline-hook proxy that is the return address into libil2cpp.so,
// which is the managed caller, and the enclosing-method scan over dump2313.cs
// turns it into a class and method name.
//
// The stack is deliberately not unwound. log.h records why: this library and
// PG3D use different ARM EHABI unwinders and mixing their contexts has crashed
// the process before.
namespace debug_log_trace_2313 {
namespace detail {

using MethodInfo = void;

// Static method: the boxed argument, then MethodInfo*.
using LogFn = void (*)(void*, const MethodInfo*);

constexpr const char* kUnityNamespace = "UnityEngine";
constexpr const char* kDebugClass = "Debug";
constexpr const char* kLog = "Log";
constexpr const char* kLogError = "LogError";

inline LogFn g_log = nullptr;
inline LogFn g_log_error = nullptr;
inline std::atomic<uint32_t> g_hits{0u};
inline std::atomic<uint32_t> g_seen{0u};

// Runs up to 87 filtered this trace down to a handful of needles, and that is
// why the message after `Start wait info about Game Center` never appeared:
// the filter was silent, not the client. The boot is clean enough now to
// afford the whole stream, so every distinct managed log line is printed once
// and repeats are suppressed. The needles below keep their old behaviour and
// print on every occurrence; the `----` needle is the prefix separator the
// config consumers use, so it names all of them without listing each one.
inline bool interesting(const std::string& text) {
    static const char* kNeedles[] = {"wait config", "config response",
                                     "is null or empty", "ClanCommon",
                                     "Start wait", "End wait", "wait socket",
                                     "----", "parse error"};
    for (const char* needle : kNeedles) {
        if (text.find(needle) != std::string::npos) return true;
    }
    return false;
}

// Open-addressed table of message hashes, so a repeated line costs one probe
// and prints nothing. A trace can tolerate a lost race, so the slots are plain
// atomics and the probe gives up instead of blocking the game thread.
constexpr uint32_t kSeenSlots = 8192u;
constexpr uint32_t kDistinctBudget = 4000u;
inline std::atomic<uint64_t> g_seen_hash[kSeenSlots];
inline std::atomic<uint32_t> g_distinct{0u};

inline uint64_t hash_of(const std::string& text) {
    uint64_t h = 1469598103934665603ull;
    for (const char c : text) {
        h ^= static_cast<uint8_t>(c);
        h *= 1099511628211ull;
    }
    // Zero marks an empty slot, so it can never be a live hash.
    return h | 1ull;
}

// True the first time this exact message is seen, false for every repeat.
inline bool first_time(const std::string& text) {
    if (g_distinct.load(std::memory_order_relaxed) >= kDistinctBudget) {
        return false;
    }
    const uint64_t h = hash_of(text);
    uint32_t slot = static_cast<uint32_t>(h) & (kSeenSlots - 1u);
    for (uint32_t probe = 0u; probe < 32u; ++probe) {
        const uint64_t held = g_seen_hash[slot].load(std::memory_order_relaxed);
        if (held == h) return false;
        if (held == 0ull) {
            uint64_t expected = 0ull;
            if (g_seen_hash[slot].compare_exchange_strong(
                    expected, h, std::memory_order_relaxed)) {
                g_distinct.fetch_add(1u, std::memory_order_relaxed);
                return true;
            }
            if (expected == h) return false;
        }
        slot = (slot + 1u) & (kSeenSlots - 1u);
    }
    return false;
}

inline const char* class_name_of(void* object) {
    if (object == nullptr) return "null";
    if (il2cpp::object_get_class == nullptr ||
        il2cpp::class_get_name == nullptr) {
        return "?";
    }
    void* klass = il2cpp::object_get_class(object);
    if (klass == nullptr) return "?";
    const char* name = il2cpp::class_get_name(klass);
    return name != nullptr ? name : "?";
}

void hook_log(void* message, const MethodInfo* method) {
    g_seen.fetch_add(1u);
    if (message != nullptr &&
        std::strcmp(class_name_of(message), "String") == 0) {
        const std::string text = il2cpp::to_utf8(message, 512u);
        if (interesting(text) || first_time(text)) {
            const uint32_t n = g_hits.fetch_add(1u) + 1u;
            LOGI("23.1.3-dbglog: #%u managed caller of Debug.Log text='%s'", n,
                 text.c_str());
        }
    }
    if (g_log != nullptr) {
        g_log(message, method);
    }
}

// The five configs the boot stops on report themselves through
// Debug.LogError, which is a different managed method, so the Debug.Log hook
// above never sees them. Same filter, same pc= field, same passthrough.
void hook_log_error(void* message, const MethodInfo* method) {
    g_seen.fetch_add(1u);
    if (message != nullptr &&
        std::strcmp(class_name_of(message), "String") == 0) {
        const std::string text = il2cpp::to_utf8(message, 512u);
        if (interesting(text) || first_time(text)) {
            const uint32_t n = g_hits.fetch_add(1u) + 1u;
            LOGI("23.1.3-dbglog: #%u managed caller of Debug.LogError "
                 "text='%s'",
                 n, text.c_str());
        }
    }
    if (g_log_error != nullptr) {
        g_log_error(message, method);
    }
}

} // namespace detail

inline bool install_hooks() {
    const bool armed = hook::install(
        {detail::kUnityNamespace, detail::kDebugClass, detail::kLog, 1},
        reinterpret_cast<void*>(&detail::hook_log),
        reinterpret_cast<void**>(&detail::g_log), false);
    const bool armed_error = hook::install(
        {detail::kUnityNamespace, detail::kDebugClass, detail::kLogError, 1},
        reinterpret_cast<void*>(&detail::hook_log_error),
        reinterpret_cast<void**>(&detail::g_log_error), false);
    LOGI("23.1.3-dbglog: Debug.Log(object) trace %s, Debug.LogError(object) "
         "trace %s; for every matching line the pc= field is the managed "
         "caller to resolve against dump2313.cs",
         armed ? "armed" : "refused", armed_error ? "armed" : "refused");
    return armed || armed_error;
}

} // namespace debug_log_trace_2313

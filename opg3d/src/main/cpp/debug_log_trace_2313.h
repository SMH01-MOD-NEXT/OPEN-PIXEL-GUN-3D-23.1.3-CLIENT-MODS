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

inline LogFn g_log = nullptr;
inline std::atomic<uint32_t> g_hits{0u};
inline std::atomic<uint32_t> g_seen{0u};

// This client logs thousands of lines per boot, so only the messages that bear
// on the current stop are printed.
inline bool interesting(const std::string& text) {
    static const char* kNeedles[] = {"wait config", "config response",
                                     "is null or empty", "ClanCommon",
                                     "Start wait", "wait socket"};
    for (const char* needle : kNeedles) {
        if (text.find(needle) != std::string::npos) return true;
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
        const std::string text = il2cpp::to_utf8(message, 256u);
        if (interesting(text)) {
            const uint32_t n = g_hits.fetch_add(1u) + 1u;
            LOGI("23.1.3-dbglog: #%u managed caller of Debug.Log text='%s'", n,
                 text.c_str());
        }
    }
    if (g_log != nullptr) {
        g_log(message, method);
    }
}

} // namespace detail

inline bool install_hooks() {
    const bool armed = hook::install(
        {detail::kUnityNamespace, detail::kDebugClass, detail::kLog, 1},
        reinterpret_cast<void*>(&detail::hook_log),
        reinterpret_cast<void**>(&detail::g_log), false);
    LOGI("23.1.3-dbglog: Debug.Log(object) trace %s; for every matching line "
         "the pc= field is the managed caller to resolve against dump2313.cs",
         armed ? "armed" : "refused");
    return armed;
}

} // namespace debug_log_trace_2313

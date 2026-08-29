#pragma once

#include <atomic>
#include <cstdint>

#include "hook.h"
#include "il2cpp.h"
#include "log.h"

// Run 14 proved both coroutines started by the Authorized branch take exactly
// one step (state 0 -> 1, more=1) and are never pumped again, while the server
// message window is only instantiated and never shown. So the scene is parked
// on whatever those first segments yielded. This trace names it: it reads the
// object each state machine hands to Unity through Current and reports its
// managed class, and it follows the five cached Func<bool> lambdas of the
// closure class, which is where a WaitUntil predicate would live. Passive.
namespace await_trace_2313 {
namespace detail {

using MethodInfo = void;

constexpr int32_t kMaxReports = 8;
constexpr int32_t kPollReportEvery = 240;

inline const char* class_name(void* object) {
    if (object == nullptr) {
        return "null";
    }
    if (il2cpp::object_get_class == nullptr || il2cpp::class_get_name == nullptr) {
        return "unresolved";
    }
    void* klass = il2cpp::object_get_class(object);
    const char* name = klass != nullptr ? il2cpp::class_get_name(klass) : nullptr;
    return name != nullptr ? name : "unnamed";
}

using CurrentFn0 = void* (*)(void* self, const MethodInfo* method);
inline CurrentFn0 g_current0 = nullptr;
inline std::atomic<int32_t> g_current0_calls;
void* current_0(void* self, const MethodInfo* method) {
    void* value = g_current0 != nullptr ? g_current0(self, method) : nullptr;
    const int32_t call = g_current0_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    if (call <= kMaxReports) {
        LOGI("23.1.3-await: coroutine 0x3DC4758 waits on class=%s object=%p (via 0x3DC7C4C) call#%d",
             class_name(value), value, call);
    }
    return value;
}

using CurrentFn1 = void* (*)(void* self, const MethodInfo* method);
inline CurrentFn1 g_current1 = nullptr;
inline std::atomic<int32_t> g_current1_calls;
void* current_1(void* self, const MethodInfo* method) {
    void* value = g_current1 != nullptr ? g_current1(self, method) : nullptr;
    const int32_t call = g_current1_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    if (call <= kMaxReports) {
        LOGI("23.1.3-await: coroutine 0x3DC4758 waits on class=%s object=%p (via 0x3DC7C94) call#%d",
             class_name(value), value, call);
    }
    return value;
}

using CurrentFn2 = void* (*)(void* self, const MethodInfo* method);
inline CurrentFn2 g_current2 = nullptr;
inline std::atomic<int32_t> g_current2_calls;
void* current_2(void* self, const MethodInfo* method) {
    void* value = g_current2 != nullptr ? g_current2(self, method) : nullptr;
    const int32_t call = g_current2_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    if (call <= kMaxReports) {
        LOGI("23.1.3-await: coroutine 0x3DC6CD0 waits on class=%s object=%p (via 0x3DCB0B4) call#%d",
             class_name(value), value, call);
    }
    return value;
}

using CurrentFn3 = void* (*)(void* self, const MethodInfo* method);
inline CurrentFn3 g_current3 = nullptr;
inline std::atomic<int32_t> g_current3_calls;
void* current_3(void* self, const MethodInfo* method) {
    void* value = g_current3 != nullptr ? g_current3(self, method) : nullptr;
    const int32_t call = g_current3_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    if (call <= kMaxReports) {
        LOGI("23.1.3-await: coroutine 0x3DC6CD0 waits on class=%s object=%p (via 0x3DCB0FC) call#%d",
             class_name(value), value, call);
    }
    return value;
}

using PredicateFn0 = bool (*)(void* self, const MethodInfo* method);
inline PredicateFn0 g_predicate0 = nullptr;
inline std::atomic<int32_t> g_predicate0_calls;
inline std::atomic<int32_t> g_predicate0_true;
bool predicate_0(void* self, const MethodInfo* method) {
    const bool value = g_predicate0 != nullptr ? g_predicate0(self, method) : false;
    const int32_t call = g_predicate0_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    const bool first_true =
        value && g_predicate0_true.fetch_add(1, std::memory_order_relaxed) == 0;
    if (call <= 3 || first_true || call % kPollReportEvery == 0) {
        LOGI("23.1.3-await: predicate 0x3DC739C polled #%d returned %s", call,
             value ? "true" : "false");
    }
    return value;
}

using PredicateFn1 = bool (*)(void* self, const MethodInfo* method);
inline PredicateFn1 g_predicate1 = nullptr;
inline std::atomic<int32_t> g_predicate1_calls;
inline std::atomic<int32_t> g_predicate1_true;
bool predicate_1(void* self, const MethodInfo* method) {
    const bool value = g_predicate1 != nullptr ? g_predicate1(self, method) : false;
    const int32_t call = g_predicate1_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    const bool first_true =
        value && g_predicate1_true.fetch_add(1, std::memory_order_relaxed) == 0;
    if (call <= 3 || first_true || call % kPollReportEvery == 0) {
        LOGI("23.1.3-await: predicate 0x3DC742C polled #%d returned %s", call,
             value ? "true" : "false");
    }
    return value;
}

using PredicateFn2 = bool (*)(void* self, const MethodInfo* method);
inline PredicateFn2 g_predicate2 = nullptr;
inline std::atomic<int32_t> g_predicate2_calls;
inline std::atomic<int32_t> g_predicate2_true;
bool predicate_2(void* self, const MethodInfo* method) {
    const bool value = g_predicate2 != nullptr ? g_predicate2(self, method) : false;
    const int32_t call = g_predicate2_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    const bool first_true =
        value && g_predicate2_true.fetch_add(1, std::memory_order_relaxed) == 0;
    if (call <= 3 || first_true || call % kPollReportEvery == 0) {
        LOGI("23.1.3-await: predicate 0x3DC7584 polled #%d returned %s", call,
             value ? "true" : "false");
    }
    return value;
}

using PredicateFn3 = bool (*)(void* self, const MethodInfo* method);
inline PredicateFn3 g_predicate3 = nullptr;
inline std::atomic<int32_t> g_predicate3_calls;
inline std::atomic<int32_t> g_predicate3_true;
bool predicate_3(void* self, const MethodInfo* method) {
    const bool value = g_predicate3 != nullptr ? g_predicate3(self, method) : false;
    const int32_t call = g_predicate3_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    const bool first_true =
        value && g_predicate3_true.fetch_add(1, std::memory_order_relaxed) == 0;
    if (call <= 3 || first_true || call % kPollReportEvery == 0) {
        LOGI("23.1.3-await: predicate 0x3DC75EC polled #%d returned %s", call,
             value ? "true" : "false");
    }
    return value;
}

using PredicateFn4 = bool (*)(void* self, const MethodInfo* method);
inline PredicateFn4 g_predicate4 = nullptr;
inline std::atomic<int32_t> g_predicate4_calls;
inline std::atomic<int32_t> g_predicate4_true;
bool predicate_4(void* self, const MethodInfo* method) {
    const bool value = g_predicate4 != nullptr ? g_predicate4(self, method) : false;
    const int32_t call = g_predicate4_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    const bool first_true =
        value && g_predicate4_true.fetch_add(1, std::memory_order_relaxed) == 0;
    if (call <= 3 || first_true || call % kPollReportEvery == 0) {
        LOGI("23.1.3-await: predicate 0x3DC7688 polled #%d returned %s", call,
             value ? "true" : "false");
    }
    return value;
}

} // namespace detail

inline bool install_hooks() {
    int armed = 0;
    armed += hook::install({"", "AuthSceneController.丆丞业且世丌丟专丕", u8"System.Collections.Generic.IEnumerator<System.Object>.get_Current", 0},
                           reinterpret_cast<void*>(&detail::current_0),
                           reinterpret_cast<void**>(&detail::g_current0), false) ? 1 : 0;  // 0x3DC7C4C
    armed += hook::install({"", "AuthSceneController.丆丞业且世丌丟专丕", u8"System.Collections.IEnumerator.get_Current", 0},
                           reinterpret_cast<void*>(&detail::current_1),
                           reinterpret_cast<void**>(&detail::g_current1), false) ? 1 : 0;  // 0x3DC7C94
    armed += hook::install({"", "AuthSceneController.且下七丟上三丌世专", u8"System.Collections.Generic.IEnumerator<System.Object>.get_Current", 0},
                           reinterpret_cast<void*>(&detail::current_2),
                           reinterpret_cast<void**>(&detail::g_current2), false) ? 1 : 0;  // 0x3DCB0B4
    armed += hook::install({"", "AuthSceneController.且下七丟上三丌世专", u8"System.Collections.IEnumerator.get_Current", 0},
                           reinterpret_cast<void*>(&detail::current_3),
                           reinterpret_cast<void**>(&detail::g_current3), false) ? 1 : 0;  // 0x3DCB0FC
    armed += hook::install({"", "AuthSceneController.<>c", u8"万丑业东业万丁七丗", 0},
                           reinterpret_cast<void*>(&detail::predicate_0),
                           reinterpret_cast<void**>(&detail::g_predicate0), false) ? 1 : 0;  // 0x3DC739C
    armed += hook::install({"", "AuthSceneController.<>c", u8"业丅丟丘丙丗丞业专", 0},
                           reinterpret_cast<void*>(&detail::predicate_1),
                           reinterpret_cast<void**>(&detail::g_predicate1), false) ? 1 : 0;  // 0x3DC742C
    armed += hook::install({"", "AuthSceneController.<>c", u8"世丏丂上丗丁丆丘下", 0},
                           reinterpret_cast<void*>(&detail::predicate_2),
                           reinterpret_cast<void**>(&detail::g_predicate2), false) ? 1 : 0;  // 0x3DC7584
    armed += hook::install({"", "AuthSceneController.<>c", u8"丈丌丅丟丁万丄三七", 0},
                           reinterpret_cast<void*>(&detail::predicate_3),
                           reinterpret_cast<void**>(&detail::g_predicate3), false) ? 1 : 0;  // 0x3DC75EC
    armed += hook::install({"", "AuthSceneController.<>c", u8"七世专七丏丟丁一丟", 0},
                           reinterpret_cast<void*>(&detail::predicate_4),
                           reinterpret_cast<void**>(&detail::g_predicate4), false) ? 1 : 0;  // 0x3DC7688
    LOGI("23.1.3-await: coroutine wait trace armed on %d/9 entry point(s); "
         "passive passthrough", armed);
    return armed > 0;
}

} // namespace await_trace_2313

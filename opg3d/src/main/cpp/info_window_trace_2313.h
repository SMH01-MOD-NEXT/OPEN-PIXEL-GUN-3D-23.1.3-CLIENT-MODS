#pragma once

#include <atomic>
#include <cstdint>
#include <string>

#include "hook.h"
#include "il2cpp.h"
#include "log.h"

// The 23.1.3 auth scene stops in state 2 (Authorized). The sweep in
// auth_scene_sweep_2313 showed the live path: the dispatcher reaches
// 0x3DC2410, which starts two coroutines and returns, and a Unity warning
// proves the dispatcher also instantiates InfoWindowController, the server
// message window. This trace follows both: the two coroutine bodies, so a
// yield that never resumes becomes visible, and the message window, so a
// modal that never closes becomes visible. Everything is passthrough.
namespace info_window_trace_2313 {
namespace detail {

using MethodInfo = void;
constexpr int32_t kIteratorStateOffset = 0x10;
constexpr int32_t kMaxReports = 12;

inline int32_t iterator_state(void* self) {
    if (self == nullptr) {
        return -0x7fffffff;
    }
    return *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(self) + kIteratorStateOffset);
}

using MoveNextFn0 = bool (*)(void* self, const MethodInfo* method);
inline MoveNextFn0 g_move0 = nullptr;
inline std::atomic<int32_t> g_move0_calls;
bool move_next_0(void* self, const MethodInfo* method) {
    const int32_t before = iterator_state(self);
    const bool more = g_move0 != nullptr ? g_move0(self, method) : false;
    const int32_t after = iterator_state(self);
    const int32_t call = g_move0_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    if (call <= kMaxReports || before != after) {
        LOGI("23.1.3-infowin: coroutine 0x3DC4758 step #%d state %d -> %d more=%d",
             call, before, after, more ? 1 : 0);
    }
    return more;
}

using MoveNextFn1 = bool (*)(void* self, const MethodInfo* method);
inline MoveNextFn1 g_move1 = nullptr;
inline std::atomic<int32_t> g_move1_calls;
bool move_next_1(void* self, const MethodInfo* method) {
    const int32_t before = iterator_state(self);
    const bool more = g_move1 != nullptr ? g_move1(self, method) : false;
    const int32_t after = iterator_state(self);
    const int32_t call = g_move1_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    if (call <= kMaxReports || before != after) {
        LOGI("23.1.3-infowin: coroutine 0x3DC6CD0 step #%d state %d -> %d more=%d",
             call, before, after, more ? 1 : 0);
    }
    return more;
}

using Fn0 = bool (*)(void* self, const MethodInfo* method);
inline Fn0 g_fn0 = nullptr;
inline std::atomic<int32_t> g_fn0_calls;
bool fn_0(void* self, const MethodInfo* method) {
    const int32_t call = g_fn0_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    const bool report = call <= kMaxReports;
    const bool value = g_fn0 != nullptr ? g_fn0(self, method) : false;
    if (report) {
        LOGI("23.1.3-infowin: 0x28F855C query returned %s call#%d",
             value ? "true" : "false", call);
    }
    return value;
}

using Fn1 = void (*)(void* self, void* text, const MethodInfo* method);
inline Fn1 g_fn1 = nullptr;
inline std::atomic<int32_t> g_fn1_calls;
void fn_1(void* self, void* text, const MethodInfo* method) {
    const int32_t call = g_fn1_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    const bool report = call <= kMaxReports;
    const std::string as_text = text != nullptr ? il2cpp::to_utf8(text) : std::string();
    if (report) {
        LOGI("23.1.3-infowin: 0x28F862C ENTER self=%p text='%s' call#%d", self, as_text.c_str(), call);
    }
    if (g_fn1 != nullptr) {
        g_fn1(self, text, method);
    }
    if (report) {
        LOGI("23.1.3-infowin: 0x28F862C RETURN");
    }
}

using Fn2 = void (*)(void* self, bool flag, const MethodInfo* method);
inline Fn2 g_fn2 = nullptr;
inline std::atomic<int32_t> g_fn2_calls;
void fn_2(void* self, bool flag, const MethodInfo* method) {
    const int32_t call = g_fn2_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    const bool report = call <= kMaxReports;
    if (report) {
        LOGI("23.1.3-infowin: 0x28F8860 ENTER self=%p flag=%d call#%d", self, flag ? 1 : 0, call);
    }
    if (g_fn2 != nullptr) {
        g_fn2(self, flag, method);
    }
    if (report) {
        LOGI("23.1.3-infowin: 0x28F8860 RETURN");
    }
}

using Fn3 = void (*)(void* self, void* callback, const MethodInfo* method);
inline Fn3 g_fn3 = nullptr;
inline std::atomic<int32_t> g_fn3_calls;
void fn_3(void* self, void* callback, const MethodInfo* method) {
    const int32_t call = g_fn3_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    const bool report = call <= kMaxReports;
    if (report) {
        LOGI("23.1.3-infowin: 0x28F8ED4 ENTER self=%p callback=%p call#%d", self, callback, call);
    }
    if (g_fn3 != nullptr) {
        g_fn3(self, callback, method);
    }
    if (report) {
        LOGI("23.1.3-infowin: 0x28F8ED4 RETURN");
    }
}

using Fn4 = void (*)(void* self, void* text, void* accept, void* cancel, const MethodInfo* method);
inline Fn4 g_fn4 = nullptr;
inline std::atomic<int32_t> g_fn4_calls;
void fn_4(void* self, void* text, void* accept, void* cancel, const MethodInfo* method) {
    const int32_t call = g_fn4_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    const bool report = call <= kMaxReports;
    const std::string as_text = text != nullptr ? il2cpp::to_utf8(text) : std::string();
    if (report) {
        LOGI("23.1.3-infowin: 0x28F8C64 ENTER self=%p text='%s' accept=%p cancel=%p call#%d", self, as_text.c_str(), accept, cancel, call);
    }
    if (g_fn4 != nullptr) {
        g_fn4(self, text, accept, cancel, method);
    }
    if (report) {
        LOGI("23.1.3-infowin: 0x28F8C64 RETURN");
    }
}

using Fn5 = void (*)(void* self, const MethodInfo* method);
inline Fn5 g_fn5 = nullptr;
inline std::atomic<int32_t> g_fn5_calls;
void fn_5(void* self, const MethodInfo* method) {
    const int32_t call = g_fn5_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    const bool report = call <= kMaxReports;
    if (report) {
        LOGI("23.1.3-infowin: 0x28F89E0 ENTER self=%p call#%d", self, call);
    }
    if (g_fn5 != nullptr) {
        g_fn5(self, method);
    }
    if (report) {
        LOGI("23.1.3-infowin: 0x28F89E0 RETURN");
    }
}

using Fn6 = void* (*)(void* self, const MethodInfo* method);
inline Fn6 g_fn6 = nullptr;
inline std::atomic<int32_t> g_fn6_calls;
void* fn_6(void* self, const MethodInfo* method) {
    const int32_t call = g_fn6_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    const bool report = call <= kMaxReports;
    void* iterator = g_fn6 != nullptr ? g_fn6(self, method) : nullptr;
    if (report) {
        LOGI("23.1.3-infowin: 0x28F84E8 iterator requested iterator=%p call#%d",
             iterator, call);
    }
    return iterator;
}

using Fn7 = void (*)(void* self, const MethodInfo* method);
inline Fn7 g_fn7 = nullptr;
inline std::atomic<int32_t> g_fn7_calls;
void fn_7(void* self, const MethodInfo* method) {
    const int32_t call = g_fn7_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    const bool report = call <= kMaxReports;
    if (report) {
        LOGI("23.1.3-infowin: 0x28F8498 ENTER self=%p call#%d", self, call);
    }
    if (g_fn7 != nullptr) {
        g_fn7(self, method);
    }
    if (report) {
        LOGI("23.1.3-infowin: 0x28F8498 RETURN");
    }
}

using Fn8 = void (*)(void* self, const MethodInfo* method);
inline Fn8 g_fn8 = nullptr;
inline std::atomic<int32_t> g_fn8_calls;
void fn_8(void* self, const MethodInfo* method) {
    const int32_t call = g_fn8_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    const bool report = call <= kMaxReports;
    if (report) {
        LOGI("23.1.3-infowin: 0x28F8628 ENTER self=%p call#%d", self, call);
    }
    if (g_fn8 != nullptr) {
        g_fn8(self, method);
    }
    if (report) {
        LOGI("23.1.3-infowin: 0x28F8628 RETURN");
    }
}

using Fn9 = void (*)(void* self, const MethodInfo* method);
inline Fn9 g_fn9 = nullptr;
inline std::atomic<int32_t> g_fn9_calls;
void fn_9(void* self, const MethodInfo* method) {
    const int32_t call = g_fn9_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    const bool report = call <= kMaxReports;
    if (report) {
        LOGI("23.1.3-infowin: 0x28F85FC ENTER self=%p call#%d", self, call);
    }
    if (g_fn9 != nullptr) {
        g_fn9(self, method);
    }
    if (report) {
        LOGI("23.1.3-infowin: 0x28F85FC RETURN");
    }
}

using Fn10 = void (*)(void* self, const MethodInfo* method);
inline Fn10 g_fn10 = nullptr;
inline std::atomic<int32_t> g_fn10_calls;
void fn_10(void* self, const MethodInfo* method) {
    const int32_t call = g_fn10_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    const bool report = call <= kMaxReports;
    if (report) {
        LOGI("23.1.3-infowin: 0x28F9218 ENTER self=%p call#%d", self, call);
    }
    if (g_fn10 != nullptr) {
        g_fn10(self, method);
    }
    if (report) {
        LOGI("23.1.3-infowin: 0x28F9218 RETURN");
    }
}

using Fn11 = void (*)(void* self, const MethodInfo* method);
inline Fn11 g_fn11 = nullptr;
inline std::atomic<int32_t> g_fn11_calls;
void fn_11(void* self, const MethodInfo* method) {
    const int32_t call = g_fn11_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    const bool report = call <= kMaxReports;
    if (report) {
        LOGI("23.1.3-infowin: 0x28F8110 ENTER self=%p call#%d", self, call);
    }
    if (g_fn11 != nullptr) {
        g_fn11(self, method);
    }
    if (report) {
        LOGI("23.1.3-infowin: 0x28F8110 RETURN");
    }
}

using Fn12 = void (*)(void* self, const MethodInfo* method);
inline Fn12 g_fn12 = nullptr;
inline std::atomic<int32_t> g_fn12_calls;
void fn_12(void* self, const MethodInfo* method) {
    const int32_t call = g_fn12_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    const bool report = call <= kMaxReports;
    if (report) {
        LOGI("23.1.3-infowin: 0x28F806C ENTER self=%p call#%d", self, call);
    }
    if (g_fn12 != nullptr) {
        g_fn12(self, method);
    }
    if (report) {
        LOGI("23.1.3-infowin: 0x28F806C RETURN");
    }
}

} // namespace detail

inline bool install_hooks() {
    int armed = 0;
    armed += hook::install({"", "AuthSceneController.丆丞业且世丌丟专丕", "MoveNext", 0},
                           reinterpret_cast<void*>(&detail::move_next_0),
                           reinterpret_cast<void**>(&detail::g_move0), false) ? 1 : 0;  // 0x3DC4758
    armed += hook::install({"", "AuthSceneController.且下七丟上三丌世专", "MoveNext", 0},
                           reinterpret_cast<void*>(&detail::move_next_1),
                           reinterpret_cast<void**>(&detail::g_move1), false) ? 1 : 0;  // 0x3DC6CD0
    armed += hook::install({"", "InfoWindowController", u8"三丅丙丞上不丂专丕", 0},
                           reinterpret_cast<void*>(&detail::fn_0),
                           reinterpret_cast<void**>(&detail::g_fn0), false) ? 1 : 0;  // 0x28F855C
    armed += hook::install({"", "InfoWindowController", u8"不丙丛三世不丕万丄", 1},
                           reinterpret_cast<void*>(&detail::fn_1),
                           reinterpret_cast<void**>(&detail::g_fn1), false) ? 1 : 0;  // 0x28F862C
    armed += hook::install({"", "InfoWindowController", u8"丂且丕上丁三万一下", 1},
                           reinterpret_cast<void*>(&detail::fn_2),
                           reinterpret_cast<void**>(&detail::g_fn2), false) ? 1 : 0;  // 0x28F8860
    armed += hook::install({"", "InfoWindowController", u8"丟业丁丘一与丈丐七", 1},
                           reinterpret_cast<void*>(&detail::fn_3),
                           reinterpret_cast<void**>(&detail::g_fn3), false) ? 1 : 0;  // 0x28F8ED4
    armed += hook::install({"", "InfoWindowController", u8"丏不上丙下且丁丁七", 3},
                           reinterpret_cast<void*>(&detail::fn_4),
                           reinterpret_cast<void**>(&detail::g_fn4), false) ? 1 : 0;  // 0x28F8C64
    armed += hook::install({"", "InfoWindowController", u8"OnClickOkDialog", 0},
                           reinterpret_cast<void*>(&detail::fn_5),
                           reinterpret_cast<void**>(&detail::g_fn5), false) ? 1 : 0;  // 0x28F89E0
    armed += hook::install({"", "InfoWindowController", u8"丌与丞万下丅一丕且", 0},
                           reinterpret_cast<void*>(&detail::fn_6),
                           reinterpret_cast<void**>(&detail::g_fn6), false) ? 1 : 0;  // 0x28F84E8
    armed += hook::install({"", "InfoWindowController", u8"万丁三下三丙丂一东", 0},
                           reinterpret_cast<void*>(&detail::fn_7),
                           reinterpret_cast<void**>(&detail::g_fn7), false) ? 1 : 0;  // 0x28F8498
    armed += hook::install({"", "InfoWindowController", u8"丌丂丙上与世且丂丙", 0},
                           reinterpret_cast<void*>(&detail::fn_8),
                           reinterpret_cast<void**>(&detail::g_fn8), false) ? 1 : 0;  // 0x28F8628
    armed += hook::install({"", "InfoWindowController", u8"与丗下万丅丟丝业丙", 0},
                           reinterpret_cast<void*>(&detail::fn_9),
                           reinterpret_cast<void**>(&detail::g_fn9), false) ? 1 : 0;  // 0x28F85FC
    armed += hook::install({"", "InfoWindowController", u8"丒三丙东丐丂丙一丌", 0},
                           reinterpret_cast<void*>(&detail::fn_10),
                           reinterpret_cast<void**>(&detail::g_fn10), false) ? 1 : 0;  // 0x28F9218
    armed += hook::install({"", "InfoWindowController", u8"Start", 0},
                           reinterpret_cast<void*>(&detail::fn_11),
                           reinterpret_cast<void**>(&detail::g_fn11), false) ? 1 : 0;  // 0x28F8110
    armed += hook::install({"", "InfoWindowController", u8"Awake", 0},
                           reinterpret_cast<void*>(&detail::fn_12),
                           reinterpret_cast<void**>(&detail::g_fn12), false) ? 1 : 0;  // 0x28F806C
    LOGI("23.1.3-infowin: coroutine and message window trace armed on %d/15 "
         "entry point(s); passive passthrough", armed);
    return armed > 0;
}

} // namespace info_window_trace_2313

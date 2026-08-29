#pragma once

#include <atomic>
#include <cstdint>
#include <string>

#include "hook.h"
#include "il2cpp.h"
#include "log.h"

// Passive sweep over every instance method of the 23.1.3 AuthSceneController.
//
// auth_scene_trace_2313 and auth_state_trace_2313 narrowed the stall down to
// this: Start runs, the dispatcher handles state 2 (Authorized) once as a
// no-op, and the scene then rests forever. The state property setter is dead
// code across the whole image, so the field is written inline by whichever
// method advances the machine. Rather than guess which one that is, every
// instance method of the class is instrumented at once so the log can show
// which part of the scene is still alive after Start returns. Each entry point
// reports only its first few calls, so frame-polled predicates cannot flood
// the log, and every thunk calls the stock body: behaviour is unchanged.
namespace auth_scene_sweep_2313 {
namespace detail {

using MethodInfo = void;
using VoidFn = void (*)(void* self, const MethodInfo* method);
using IterFn = void* (*)(void* self, const MethodInfo* method);
using BoolFn = bool (*)(void* self, const MethodInfo* method);
using StateFn = void (*)(void* self, int32_t state, const MethodInfo* method);
using ReplyFn = void (*)(void* self, void* payload, bool flag, void* text,
                         void* keys, const MethodInfo* method);
using StringFn = void (*)(void* self, void* text, const MethodInfo* method);

constexpr int32_t kMaxReports = 4;

constexpr int kVoidCount = 35;
inline VoidFn g_void[kVoidCount] = {};
inline std::atomic<int32_t> g_void_calls[kVoidCount];
inline const char* const g_void_label[kVoidCount] = {
    "0x3DC2C04",
    "0x3DC2CC4",
    "0x3DC2F3C",
    "0x3DC3094",
    "0x3DC3100",
    "0x3DC3180",
    "0x3DC3690",
    "0x3DC39D4",
    "0x3DC3DD4",
    "0x3DC3F54",
    "0x3DC46C8",
    "0x3DC2410",
    "0x3DC4890",
    "0x3DC4A30",
    "0x3DC4B98",
    "0x3DC4EB4",
    "0x3DC5108",
    "0x3DC5A5C",
    "0x3DC5C44",
    "0x3DC5CB8",
    "0x3DC61A0",
    "0x3DC62D8",
    "0x3DC21B4",
    "0x3DC4964",
    "0x3DC39A8",
    "0x3DC18D4",
    "0x3DC1DB8",
    "0x3DC6D58",
    "0x3DC20E8",
    "0x3DC2110",
    "0x3DC2438",
    "0x3DC1758",
    "0x3DC6E9C",
    "0x3DC6EBC",
    "0x3DC70F8",
};

constexpr int kIterCount = 10;
inline IterFn g_iter[kIterCount] = {};
inline std::atomic<int32_t> g_iter_calls[kIterCount];
inline const char* const g_iter_label[kIterCount] = {
    "0x3DC4758",
    "0x3DC47F4",
    "0x3DC5CD8",
    "0x3DC5EB8",
    "0x3DC59E8",
    "0x3DC6214",
    "0x3DC3EE0",
    "0x3DC6BAC",
    "0x3DC6C48",
    "0x3DC6CD0",
};

constexpr int kBoolCount = 1;
inline BoolFn g_bool[kBoolCount] = {};
inline std::atomic<int32_t> g_bool_calls[kBoolCount];
inline const char* const g_bool_label[kBoolCount] = {
    "0x3DC5F54",
};

constexpr int kStateCount = 1;
inline StateFn g_state[kStateCount] = {};
inline std::atomic<int32_t> g_state_calls[kStateCount];
inline const char* const g_state_label[kStateCount] = {
    "0x3DC2458",
};

constexpr int kReplyCount = 1;
inline ReplyFn g_reply[kReplyCount] = {};
inline std::atomic<int32_t> g_reply_calls[kReplyCount];
inline const char* const g_reply_label[kReplyCount] = {
    "0x3DC418C",
};

constexpr int kStringCount = 3;
inline StringFn g_string[kStringCount] = {};
inline std::atomic<int32_t> g_string_calls[kStringCount];
inline const char* const g_string_label[kStringCount] = {
    "0x3DC4C18",
    "0x3DC3988",
    "0x3DC6AD4",
};

template <int N>
void void_thunk(void* self, const MethodInfo* method) {
    const int32_t call = g_void_calls[N].fetch_add(1, std::memory_order_relaxed) + 1;
    const bool report = call <= kMaxReports;
    if (report) {
        LOGI("23.1.3-sweep: %s ENTER self=%p call#%d", g_void_label[N], self, call);
    }
    if (g_void[N] != nullptr) {
        g_void[N](self, method);
    }
    if (report) {
        LOGI("23.1.3-sweep: %s RETURN", g_void_label[N]);
    }
}

template <int N>
void* iter_thunk(void* self, const MethodInfo* method) {
    const int32_t call = g_iter_calls[N].fetch_add(1, std::memory_order_relaxed) + 1;
    void* iterator = g_iter[N] != nullptr ? g_iter[N](self, method) : nullptr;
    if (call <= kMaxReports) {
        LOGI("23.1.3-sweep: %s iterator requested self=%p iterator=%p call#%d",
             g_iter_label[N], self, iterator, call);
    }
    return iterator;
}

template <int N>
bool bool_thunk(void* self, const MethodInfo* method) {
    const bool value = g_bool[N] != nullptr ? g_bool[N](self, method) : false;
    const int32_t call = g_bool_calls[N].fetch_add(1, std::memory_order_relaxed) + 1;
    // A wait predicate is polled every frame; report the opening calls only.
    if (call <= kMaxReports) {
        LOGI("23.1.3-sweep: %s predicate returned %s call#%d", g_bool_label[N],
             value ? "true" : "false", call);
    }
    return value;
}

template <int N>
void state_thunk(void* self, int32_t state, const MethodInfo* method) {
    LOGI("23.1.3-sweep: %s called with state %d", g_state_label[N], state);
    if (g_state[N] != nullptr) {
        g_state[N](self, state, method);
    }
    LOGI("23.1.3-sweep: %s returned for state %d", g_state_label[N], state);
}

template <int N>
void reply_thunk(void* self, void* payload, bool flag, void* text, void* keys,
                 const MethodInfo* method) {
    const std::string as_text = text != nullptr ? il2cpp::to_utf8(text) : std::string();
    LOGI("23.1.3-sweep: %s reply handler ENTER payload=%p flag=%d text='%s' keys=%p",
         g_reply_label[N], payload, flag ? 1 : 0, as_text.c_str(), keys);
    if (g_reply[N] != nullptr) {
        g_reply[N](self, payload, flag, text, keys, method);
    }
    LOGI("23.1.3-sweep: %s reply handler RETURN", g_reply_label[N]);
}

template <int N>
void string_thunk(void* self, void* text, const MethodInfo* method) {
    const std::string as_text = text != nullptr ? il2cpp::to_utf8(text) : std::string();
    const int32_t call = g_string_calls[N].fetch_add(1, std::memory_order_relaxed) + 1;
    if (call <= kMaxReports) {
        LOGI("23.1.3-sweep: %s called with '%s' call#%d", g_string_label[N],
             as_text.c_str(), call);
    }
    if (g_string[N] != nullptr) {
        g_string[N](self, text, method);
    }
}

template <int N>
bool install_void(const char* name) {
    return hook::install({"", "AuthSceneController", name, 0},
                         reinterpret_cast<void*>(&void_thunk<N>),
                         reinterpret_cast<void**>(&g_void[N]), false);
}

template <int N>
bool install_iter(const char* name) {
    return hook::install({"", "AuthSceneController", name, 0},
                         reinterpret_cast<void*>(&iter_thunk<N>),
                         reinterpret_cast<void**>(&g_iter[N]), false);
}

template <int N>
bool install_bool(const char* name) {
    return hook::install({"", "AuthSceneController", name, 0},
                         reinterpret_cast<void*>(&bool_thunk<N>),
                         reinterpret_cast<void**>(&g_bool[N]), false);
}

template <int N>
bool install_state(const char* name) {
    return hook::install({"", "AuthSceneController", name, 1},
                         reinterpret_cast<void*>(&state_thunk<N>),
                         reinterpret_cast<void**>(&g_state[N]), false);
}

template <int N>
bool install_reply(const char* name) {
    return hook::install({"", "AuthSceneController", name, 4},
                         reinterpret_cast<void*>(&reply_thunk<N>),
                         reinterpret_cast<void**>(&g_reply[N]), false);
}

template <int N>
bool install_string(const char* name) {
    return hook::install({"", "AuthSceneController", name, 1},
                         reinterpret_cast<void*>(&string_thunk<N>),
                         reinterpret_cast<void**>(&g_string[N]), false);
}

} // namespace detail

inline bool install_hooks() {
    int armed = 0;
    armed += detail::install_void<0>(u8"丏丞七三丕一丄丆丞") ? 1 : 0;  // 0x3DC2C04
    armed += detail::install_void<1>(u8"且丌万丘业且丐丅不") ? 1 : 0;  // 0x3DC2CC4
    armed += detail::install_void<2>(u8"丘丂丞东丟与丟丆不") ? 1 : 0;  // 0x3DC2F3C
    armed += detail::install_void<3>(u8"专丄业不丛丑丙一不") ? 1 : 0;  // 0x3DC3094
    armed += detail::install_void<4>(u8"丝与丟下丑丏丑丟丑") ? 1 : 0;  // 0x3DC3100
    armed += detail::install_void<5>(u8"丅上不丏丙丅三专一") ? 1 : 0;  // 0x3DC3180
    armed += detail::install_void<6>(u8"丅丙丞一东下丟丌且") ? 1 : 0;  // 0x3DC3690
    armed += detail::install_void<7>(u8"丟丏丐丕世上一与万") ? 1 : 0;  // 0x3DC39D4
    armed += detail::install_void<8>(u8"万上丒丑丑七丝丂丞") ? 1 : 0;  // 0x3DC3DD4
    armed += detail::install_void<9>(u8"丑丅与丘业与丙丞万") ? 1 : 0;  // 0x3DC3F54
    armed += detail::install_void<10>(u8"一丅丂丙上丞丈丅丏") ? 1 : 0;  // 0x3DC46C8
    armed += detail::install_void<11>(u8"丌丟且丐专丈丘一丌") ? 1 : 0;  // 0x3DC2410
    armed += detail::install_void<12>(u8"专丌丑丘丌七丞东业") ? 1 : 0;  // 0x3DC4890
    armed += detail::install_void<13>(u8"上丌东丐丅东丏丏七") ? 1 : 0;  // 0x3DC4A30
    armed += detail::install_void<14>(u8"丟丕万丝七专万丄丙") ? 1 : 0;  // 0x3DC4B98
    armed += detail::install_void<15>(u8"业世下上上丞专三丞") ? 1 : 0;  // 0x3DC4EB4
    armed += detail::install_void<16>(u8"丝丛丑三丕丟七下不") ? 1 : 0;  // 0x3DC5108
    armed += detail::install_void<17>(u8"丏一业一丈丅不下上") ? 1 : 0;  // 0x3DC5A5C
    armed += detail::install_void<18>(u8"丆丙丁丌业丗与东丒") ? 1 : 0;  // 0x3DC5C44
    armed += detail::install_void<19>(u8"丅丐丘下丟丂丗丄丄") ? 1 : 0;  // 0x3DC5CB8
    armed += detail::install_void<20>(u8"丁万东东丑与丞丆丑") ? 1 : 0;  // 0x3DC61A0
    armed += detail::install_void<21>(u8"丘不七丌丑且东一丞") ? 1 : 0;  // 0x3DC62D8
    armed += detail::install_void<22>(u8"丁丂丝丝下丘丑专丗") ? 1 : 0;  // 0x3DC21B4
    armed += detail::install_void<23>(u8"丂且丒东丗丈丌丄丑") ? 1 : 0;  // 0x3DC4964
    armed += detail::install_void<24>(u8"上丙丟七丛业丈与与") ? 1 : 0;  // 0x3DC39A8
    armed += detail::install_void<25>(u8"丙三丗上东丈丟下丌") ? 1 : 0;  // 0x3DC18D4
    armed += detail::install_void<26>(u8"万丐丈丏与不丆丘丄") ? 1 : 0;  // 0x3DC1DB8
    armed += detail::install_void<27>(u8"万丁丝上丞丏丏丂上") ? 1 : 0;  // 0x3DC6D58
    armed += detail::install_void<28>(u8"丙丈丙世丅专丌丈丕") ? 1 : 0;  // 0x3DC20E8
    armed += detail::install_void<29>(u8"上丗三世丞丂丑专丕") ? 1 : 0;  // 0x3DC2110
    armed += detail::install_void<30>(u8"丝丒东丌丐一世与丛") ? 1 : 0;  // 0x3DC2438
    armed += detail::install_void<31>(u8"三专丆专丘七丛东丌") ? 1 : 0;  // 0x3DC1758
    armed += detail::install_void<32>(u8"丂丁丌丙丕丑丏丄且") ? 1 : 0;  // 0x3DC6E9C
    armed += detail::install_void<33>(u8"丟丛七专不不且三万") ? 1 : 0;  // 0x3DC6EBC
    armed += detail::install_void<34>(u8"且三不业与三万东丞") ? 1 : 0;  // 0x3DC70F8
    armed += detail::install_iter<0>(u8"丑专丞丗丒上丛丕万") ? 1 : 0;  // 0x3DC4758
    armed += detail::install_iter<1>(u8"丟丈丕丌丗东不丙丄") ? 1 : 0;  // 0x3DC47F4
    armed += detail::install_iter<2>(u8"丗丈世与丌丂三丐丌") ? 1 : 0;  // 0x3DC5CD8
    armed += detail::install_iter<3>(u8"丛七丈丆丂三丁且丈") ? 1 : 0;  // 0x3DC5EB8
    armed += detail::install_iter<4>(u8"不丆丄三丅丌且东丄") ? 1 : 0;  // 0x3DC59E8
    armed += detail::install_iter<5>(u8"丂不丈丌丅上业丘丘") ? 1 : 0;  // 0x3DC6214
    armed += detail::install_iter<6>(u8"万丕丂丑丄世丈丁丌") ? 1 : 0;  // 0x3DC3EE0
    armed += detail::install_iter<7>(u8"三七丟丐丁一丟丘丑") ? 1 : 0;  // 0x3DC6BAC
    armed += detail::install_iter<8>(u8"丘不世丝三丗丁丌丘") ? 1 : 0;  // 0x3DC6C48
    armed += detail::install_iter<9>(u8"三丁丕丙丐丈专丕专") ? 1 : 0;  // 0x3DC6CD0
    armed += detail::install_bool<0>(u8"丑七丏三丌丕丞不丙") ? 1 : 0;  // 0x3DC5F54
    armed += detail::install_state<0>(u8"丙丟不丗丑下丌丁专") ? 1 : 0;  // 0x3DC2458
    armed += detail::install_reply<0>(u8"丄上丗丑丙东丈丘丁") ? 1 : 0;  // 0x3DC418C
    armed += detail::install_string<0>(u8"丘丟丂丈丝丂丅丆东") ? 1 : 0;  // 0x3DC4C18
    armed += detail::install_string<1>(u8"丆与专且丑丙丑丘丘") ? 1 : 0;  // 0x3DC3988
    armed += detail::install_string<2>(u8"且丅世与上丕丕丁东") ? 1 : 0;  // 0x3DC6AD4
    LOGI("23.1.3-sweep: auth scene sweep armed on %d/51 method(s) of "
         "AuthSceneController; passive passthrough, first %d call(s) each",
         armed, detail::kMaxReports);
    return armed > 0;
}

} // namespace auth_scene_sweep_2313

#pragma once

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <string>

#include "elf_sym.h"
#include "hook.h"
#include "il2cpp.h"
#include "log.h"

// Run 38 parked the auth scene on predicate 0x3DC75EC, which reads the same
// static bool as QuietAuthorization's own WaitUntil (0x3DD1218). That
// predicate is (progress singleton has a non-null id string at +0x18) AND
// (the static flag is set), so this trace prints both halves on every poll
// sample, follows the coroutine state machine, and names the value that the
// id setter receives -- the exact server data the boot is waiting for.
namespace quiet_auth_trace_2313 {
namespace detail {

using MethodInfo = void;

constexpr int32_t kMaxReports = 8;

inline bool valid_ptr(const void* pointer) {
    const uintptr_t value = reinterpret_cast<uintptr_t>(pointer);
    return value >= 0x100000u && value < 0x0000800000000000ull && (value & 0x7u) == 0;
}

inline void* slot_class(uintptr_t offset) {
    uintptr_t base = 0;
    if (!elfsym::find_library("libil2cpp.so", &base) || base == 0) {
        return nullptr;
    }
    void** cell = *reinterpret_cast<void***>(base + offset);
    if (!valid_ptr(cell)) {
        return nullptr;
    }
    void* klass = *cell;
    return valid_ptr(klass) ? klass : nullptr;
}

inline void* class_statics(void* klass) {
    if (!valid_ptr(klass)) {
        return nullptr;
    }
    void* statics = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(klass) + 0xb8);
    return valid_ptr(statics) ? statics : nullptr;
}

// Flag the two predicates read: statics+0 of the class in slot 0x6778288
// (Nullable<bool> value at +0x0, hasValue at +0x1, plain bool at +0x2,
// Nullable<int> at +0x4/+0x8). Progress singleton: statics+0x20 of slot
// 0x67918F0. Every dereference is pointer-checked because run 39 crashed
// (SIGSEGV at 0x20016451) reading these slots before il2cpp had filled them.
inline std::string nullable_state() {
    void* statics = class_statics(slot_class(0x6778288));
    if (statics == nullptr) {
        return std::string("<no statics>");
    }
    const uintptr_t address = reinterpret_cast<uintptr_t>(statics);
    char buffer[128];
    snprintf(buffer, sizeof(buffer),
             "bool=%d has=%d plain=%d int=%d intHas=%d",
             static_cast<int>(*reinterpret_cast<uint8_t*>(address + 0x0)),
             static_cast<int>(*reinterpret_cast<uint8_t*>(address + 0x1)),
             static_cast<int>(*reinterpret_cast<uint8_t*>(address + 0x2)),
             *reinterpret_cast<int32_t*>(address + 0x4),
             static_cast<int>(*reinterpret_cast<uint8_t*>(address + 0x8)));
    return std::string(buffer);
}

inline int flag_value() {
    void* statics = class_statics(slot_class(0x6778288));
    if (statics == nullptr) {
        return -1;
    }
    return static_cast<int>(*reinterpret_cast<uint8_t*>(statics));
}

inline void* progress_object() {
    void* statics = class_statics(slot_class(0x67918F0));
    if (statics == nullptr) {
        return nullptr;
    }
    void* object = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(statics) + 0x20);
    return valid_ptr(object) ? object : nullptr;
}

inline std::string field_string(void* object, uintptr_t offset) {
    if (!valid_ptr(object)) {
        return std::string("<no object>");
    }
    void* value = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(object) + offset);
    if (value == nullptr) {
        return std::string("<null>");
    }
    if (!valid_ptr(value)) {
        return std::string("<bad ptr>");
    }
    const int32_t length = *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(value) + 0x10);
    if (length < 0 || length > 4096) {
        return std::string("<bad string>");
    }
    return il2cpp::to_utf8(value, 128);
}

inline void report(const char* where) {
    void* progress = progress_object();
    LOGI("23.1.3-quiet: %s statics[%s] progress=%p id@0x18=\"%s\" device@0x20=\"%s\"",
         where, nullable_state().c_str(), progress,
         field_string(progress, 0x18).c_str(),
         field_string(progress, 0x20).c_str());
}

using Predicate = bool (*)(void*, const MethodInfo*);
inline Predicate g_predicate = nullptr;
inline std::atomic<int32_t> g_predicate_calls;
bool predicate(void* self, const MethodInfo* method) {
    const bool result = g_predicate != nullptr ? g_predicate(self, method) : false;
    const int32_t call = g_predicate_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    if (call <= kMaxReports || call % 600 == 0 || result) {
        void* progress = progress_object();
        LOGI("23.1.3-quiet: predicate #%d returned %s; statics[%s] progress=%p id@0x18=\"%s\"",
             call, result ? "true" : "false", nullable_state().c_str(), progress,
             field_string(progress, 0x18).c_str());
    }
    return result;
}

using MoveNext = bool (*)(void*, const MethodInfo*);
inline MoveNext g_move_next = nullptr;
inline std::atomic<int32_t> g_move_next_calls;
bool move_next(void* self, const MethodInfo* method) {
    const int32_t call = g_move_next_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    const uintptr_t address = reinterpret_cast<uintptr_t>(self);
    const int32_t before = self != nullptr ? *reinterpret_cast<int32_t*>(address + 0x10) : -1;
    const bool more = g_move_next != nullptr ? g_move_next(self, method) : false;
    const int32_t after = self != nullptr ? *reinterpret_cast<int32_t*>(address + 0x10) : -1;
    if (call <= kMaxReports || before != after) {
        LOGI("23.1.3-quiet: coroutine step #%d state %d -> %d more=%d", call, before, after,
             more ? 1 : 0);
        report("coroutine step");
    }
    return more;
}

using SetId = void (*)(void*, void*, const MethodInfo*);
inline SetId g_set_id = nullptr;
void set_id(void* self, void* value, const MethodInfo* method) {
    LOGI("23.1.3-quiet: progress id setter called with \"%s\"",
         value != nullptr ? il2cpp::to_utf8(value, 128).c_str() : "<null>");
    if (g_set_id != nullptr) {
        g_set_id(self, value, method);
    }
    report("progress id setter");
}

using Awake = void (*)(void*, const MethodInfo*);
inline Awake g_awake = nullptr;
void awake(void* self, const MethodInfo* method) {
    LOGI("23.1.3-quiet: QuietAuthorization.Awake");
    if (g_awake != nullptr) {
        g_awake(self, method);
    }
    report("Awake");
}

} // namespace detail

inline bool install_hooks() {
    int armed = 0;
    armed += hook::install({"", "QuietAuthorization.<>c", u8"丛七丅丌专丄丏丏丗", 0},
                           reinterpret_cast<void*>(&detail::predicate),
                           reinterpret_cast<void**>(&detail::g_predicate), false) ? 1 : 0;
    armed += hook::install({"", "QuietAuthorization.且一丈丄丈上丟三三", "MoveNext", 0},
                           reinterpret_cast<void*>(&detail::move_next),
                           reinterpret_cast<void**>(&detail::g_move_next), false) ? 1 : 0;
    armed += hook::install({"Progress", u8"东丝丂丄业丕且丙丑", u8"一丞丏丛丝丝丟丞万", 1},
                           reinterpret_cast<void*>(&detail::set_id),
                           reinterpret_cast<void**>(&detail::g_set_id), false) ? 1 : 0;
    armed += hook::install({"", "QuietAuthorization", "Awake", 0},
                           reinterpret_cast<void*>(&detail::awake),
                           reinterpret_cast<void**>(&detail::g_awake), false) ? 1 : 0;
    LOGI("23.1.3-quiet: quiet-authorization trace armed on %d/4 entry point(s)", armed);
    return armed > 0;
}

} // namespace quiet_auth_trace_2313

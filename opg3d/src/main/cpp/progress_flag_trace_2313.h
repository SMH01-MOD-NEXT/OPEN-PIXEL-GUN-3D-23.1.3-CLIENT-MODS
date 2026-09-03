#pragma once

#include <atomic>
#include <cstdint>

#include "elf_sym.h"
#include "hook.h"
#include "il2cpp.h"
#include "log.h"

// Observational diagnostics for the static Progress Nullable<bool> consumed by
// the authorization flow. Earlier investigation assigned this field a causal
// meaning that later runs disproved; this trace now reports raw hasValue/value
// bytes only and makes no control-flow claim. Every hooked stock body is called
// exactly once with its original arguments and its result is preserved.
namespace progress_flag_trace_2313 {
namespace detail {

using MethodInfo = void;

// Progress.<TypeDefIndex 6640>, the static holder of the flag.
constexpr uintptr_t kFlagClassSlot = 0x6778288u;
constexpr uintptr_t kStaticFieldsOffset = 0xB8u;
constexpr uintptr_t kClassInitOffset = 0xE0u;
constexpr uintptr_t kMethodInfoKlassOffset = 0x18u;

// Unresolved metadata slots hold small tokens; real il2cpp pointers on arm64
// live far above 4 GiB and are aligned.
constexpr uintptr_t kMinPointer = 0x100000000ull;

// Static field layout of that class, from dump2313.cs line 283267:
//   0x0  Nullable<bool>: byte 0 hasValue, byte 1 value
//   0x2  bool            <..>k__BackingField
//   0x4  Nullable<int>   <..>k__BackingField
//   0x10 Action, 0x18 Action
constexpr uintptr_t kGetterRva = 0x17A8F38u;   // static Nullable<bool> get()
constexpr uintptr_t kSetterRva = 0x17A8F90u;   // static void set(Nullable<bool>)
constexpr uintptr_t kServerPayloadRva = 0x17A9980u;  // server event callback
constexpr uintptr_t kInternalWriterRva = 0x17A9F90u; // called from 0x1B41E54
constexpr uintptr_t kNotifyRva = 0x17A9880u;
constexpr uintptr_t kCctorWriterRva = 0x17A9784u;
constexpr uintptr_t kResetWriterRva = 0x17A9934u;
constexpr uintptr_t kContinuationFactoryRva = 0x3DC6214u;

constexpr int32_t kGetterBudget = 24;

using VoidFn = void (*)(const MethodInfo*);
using GetterFn = uint16_t (*)(const MethodInfo*);
using SetterFn = void (*)(uint16_t value, const MethodInfo*);
using ServerPayloadFn = void (*)(void* data, void* attachments, void* binaries,
                                 const MethodInfo*);
using FactoryFn = void* (*)(void* self, const MethodInfo*);

inline uintptr_t g_il2cpp_base = 0;
inline GetterFn g_getter = nullptr;
inline SetterFn g_setter = nullptr;
inline ServerPayloadFn g_server_payload = nullptr;
inline VoidFn g_internal_writer = nullptr;
inline VoidFn g_notify = nullptr;
inline VoidFn g_cctor_writer = nullptr;
inline VoidFn g_reset_writer = nullptr;
inline FactoryFn g_factory = nullptr;

inline std::atomic<int64_t> g_last_snapshot{-1};
inline std::atomic<int32_t> g_getter_budget{kGetterBudget};
inline std::atomic<void*> g_klass{nullptr};
inline std::atomic<bool> g_named{false};

// __builtin_return_address must be expanded inside the hook itself, so the
// caller lookup is a macro rather than a helper function.
#define PFLAG_CALLER(out)                                                      \
    do {                                                                       \
        const void* raw =                                                      \
            __builtin_extract_return_addr(__builtin_return_address(0));        \
        const uintptr_t pc = reinterpret_cast<uintptr_t>(raw) & ~uintptr_t{1}; \
        (out) = (detail::g_il2cpp_base != 0 && pc >= detail::g_il2cpp_base)    \
                    ? pc - detail::g_il2cpp_base                               \
                    : 0;                                                       \
    } while (0)

inline bool plausible(const void* pointer, uintptr_t alignment_mask) {
    const uintptr_t value = reinterpret_cast<uintptr_t>(pointer);
    return value >= kMinPointer && (value & alignment_mask) == 0u;
}

// The hooked methods are static members of the class that owns the flag, so
// their MethodInfo carries the exact Il2CppClass the auth scene reads.
inline void remember_klass(const MethodInfo* method) {
    if (g_klass.load(std::memory_order_relaxed) != nullptr) {
        return;
    }
    if (!plausible(method, 0x7u)) {
        return;
    }
    void* klass = *reinterpret_cast<void* const*>(
        reinterpret_cast<const uint8_t*>(method) + kMethodInfoKlassOffset);
    if (!plausible(klass, 0x7u)) {
        return;
    }
    g_klass.store(klass, std::memory_order_relaxed);
}

inline void* resolve_klass() {
    void* klass = g_klass.load(std::memory_order_relaxed);
    if (klass != nullptr) {
        return klass;
    }
    if (g_il2cpp_base == 0) {
        return nullptr;
    }
    // The scene does `ldr x20, [xN, #0x288]` and then `ldr x0, [x20]`, so the
    // slot holds a pointer to the cell that holds the class pointer.
    void** cell = *reinterpret_cast<void***>(g_il2cpp_base + kFlagClassSlot);
    if (!plausible(cell, 0x7u)) {
        return nullptr;
    }
    void* candidate = *cell;
    return plausible(candidate, 0x7u) ? candidate : nullptr;
}

// Reads the static fields the same way the auth scene reads them, and reports
// only when the picture changes. Every dereference is guarded.
inline void snapshot(const char* when) {
    void* klass = resolve_klass();
    if (klass == nullptr) {
        return;
    }
    uint8_t* raw_class = reinterpret_cast<uint8_t*>(klass);
    if (*reinterpret_cast<uint32_t*>(raw_class + kClassInitOffset) == 0) {
        return;  // the class has not finished initialising
    }
    uint8_t* statics =
        *reinterpret_cast<uint8_t**>(raw_class + kStaticFieldsOffset);
    if (!plausible(statics, 0x3u)) {
        return;
    }

    if (!g_named.exchange(true, std::memory_order_relaxed)) {
        const char* name = il2cpp::class_get_name != nullptr
                               ? il2cpp::class_get_name(klass)
                               : "<unnamed>";
        LOGI("23.1.3-pflag: flag owner resolved: %s statics=%p", name,
             static_cast<void*>(statics));
    }

    const uint8_t flag_has = statics[0];
    const uint8_t flag_value = statics[1];
    const uint8_t second = statics[2];
    int32_t number = 0;
    __builtin_memcpy(&number, statics + 4, sizeof(number));
    const uint8_t number_has = statics[8];

    const int64_t packed = (static_cast<int64_t>(flag_value)) |
                           (static_cast<int64_t>(flag_has) << 8) |
                           (static_cast<int64_t>(second) << 16) |
                           (static_cast<int64_t>(number_has) << 24) |
                           (static_cast<int64_t>(number) << 32);
    if (g_last_snapshot.exchange(packed, std::memory_order_relaxed) == packed) {
        return;
    }

    LOGI("23.1.3-pflag: nullable hasValue=%d value=%d second=%d number=%d "
         "numberHasValue=%d after %s",
         static_cast<int>(flag_has), static_cast<int>(flag_value),
         static_cast<int>(second), number, static_cast<int>(number_has), when);
}

inline uint16_t hook_getter(const MethodInfo* method) {
    const uint16_t value = g_getter(method);
    remember_klass(method);
    if (g_getter_budget.fetch_sub(1, std::memory_order_relaxed) > 0) {
        uintptr_t caller = 0;
        PFLAG_CALLER(caller);
        LOGI("23.1.3-pflag: read hasValue=%d value=%d caller=libil2cpp.so+0x%llx",
             static_cast<int>(value & 0xFF),
             static_cast<int>((value >> 8) & 0xFF),
             static_cast<unsigned long long>(caller));
    }
    snapshot("read");
    return value;
}

inline void hook_setter(uint16_t value, const MethodInfo* method) {
    uintptr_t caller = 0;
    PFLAG_CALLER(caller);
    LOGI("23.1.3-pflag: WRITE hasValue=%d value=%d caller=libil2cpp.so+0x%llx",
         static_cast<int>(value & 0xFF), static_cast<int>((value >> 8) & 0xFF),
         static_cast<unsigned long long>(caller));
    g_setter(value, method);
    remember_klass(method);
    snapshot("write");
}

inline void hook_server_payload(void* data, void* attachments, void* binaries,
                                const MethodInfo* method) {
    uintptr_t caller = 0;
    PFLAG_CALLER(caller);
    remember_klass(method);
    snapshot("before server payload");
    LOGI("23.1.3-pflag: server payload handler ENTER data=%p attachments=%p "
         "binaries=%p caller=libil2cpp.so+0x%llx",
         data, attachments, binaries,
         static_cast<unsigned long long>(caller));
    g_server_payload(data, attachments, binaries, method);
    snapshot("after server payload");
}

inline void hook_internal_writer(const MethodInfo* method) {
    uintptr_t caller = 0;
    PFLAG_CALLER(caller);
    LOGI("23.1.3-pflag: internal writer 0x17A9F90 ENTER "
         "caller=libil2cpp.so+0x%llx",
         static_cast<unsigned long long>(caller));
    g_internal_writer(method);
    remember_klass(method);
    snapshot("internal writer");
}

inline void hook_notify(const MethodInfo* method) {
    uintptr_t caller = 0;
    PFLAG_CALLER(caller);
    LOGI("23.1.3-pflag: notify 0x17A9880 ENTER caller=libil2cpp.so+0x%llx",
         static_cast<unsigned long long>(caller));
    g_notify(method);
    remember_klass(method);
    snapshot("notify");
}

inline void hook_cctor_writer(const MethodInfo* method) {
    LOGI("23.1.3-pflag: static-init writer 0x17A9784 ENTER");
    g_cctor_writer(method);
    remember_klass(method);
    snapshot("static-init writer");
}

inline void hook_reset_writer(const MethodInfo* method) {
    uintptr_t caller = 0;
    PFLAG_CALLER(caller);
    LOGI("23.1.3-pflag: reset writer 0x17A9934 ENTER caller=libil2cpp.so+0x%llx",
         static_cast<unsigned long long>(caller));
    g_reset_writer(method);
    remember_klass(method);
    snapshot("reset writer");
}

inline void* hook_factory(void* self, const MethodInfo* method) {
    LOGI("23.1.3-pflag: observed continuation factory self=%p", self);
    return g_factory(self, method);
}

} // namespace detail

inline bool install_hooks() {
    if (!elfsym::find_library("libil2cpp.so", &detail::g_il2cpp_base) ||
        detail::g_il2cpp_base == 0) {
        LOGW("23.1.3-pflag: libil2cpp.so base not found; the progress flag stays "
             "unwatched");
        return false;
    }

    int armed = 0;
    struct Target {
        uintptr_t rva;
        const char* label;
        void* replacement;
        void** original;
    };
    const Target targets[] = {
        {detail::kGetterRva, "Progress.flag.get",
         reinterpret_cast<void*>(&detail::hook_getter),
         reinterpret_cast<void**>(&detail::g_getter)},
        {detail::kSetterRva, "Progress.flag.set",
         reinterpret_cast<void*>(&detail::hook_setter),
         reinterpret_cast<void**>(&detail::g_setter)},
        {detail::kServerPayloadRva, "Progress.serverPayload",
         reinterpret_cast<void*>(&detail::hook_server_payload),
         reinterpret_cast<void**>(&detail::g_server_payload)},
        {detail::kInternalWriterRva, "Progress.internalWriter",
         reinterpret_cast<void*>(&detail::hook_internal_writer),
         reinterpret_cast<void**>(&detail::g_internal_writer)},
        {detail::kNotifyRva, "Progress.notify",
         reinterpret_cast<void*>(&detail::hook_notify),
         reinterpret_cast<void**>(&detail::g_notify)},
        {detail::kCctorWriterRva, "Progress.staticInitWriter",
         reinterpret_cast<void*>(&detail::hook_cctor_writer),
         reinterpret_cast<void**>(&detail::g_cctor_writer)},
        {detail::kResetWriterRva, "Progress.resetWriter",
         reinterpret_cast<void*>(&detail::hook_reset_writer),
         reinterpret_cast<void**>(&detail::g_reset_writer)},
        {detail::kContinuationFactoryRva, "AuthScene.continuationFactory",
         reinterpret_cast<void*>(&detail::hook_factory),
         reinterpret_cast<void**>(&detail::g_factory)},
    };

    for (const Target& target : targets) {
        if (hook::install_address(
                reinterpret_cast<void*>(detail::g_il2cpp_base + target.rva),
                target.label, target.replacement, target.original, false)) {
            ++armed;
        }
    }

    LOGI("23.1.3-pflag: %d/8 raw progress-status observation hooks installed",
         armed);
    return armed > 0;
}

} // namespace progress_flag_trace_2313

#pragma once

#include <atomic>
#include <cstdint>

#include <pthread.h>
#include <unistd.h>

#include "elf_sym.h"
#include "hook.h"
#include "il2cpp.h"
#include "log.h"

// Observational diagnostics for AuthSceneState. The two wrappers call stock
// exactly once with unchanged arguments. A bounded read-only sampler catches
// inlined stores that do not pass through those methods: 150 reads over 30 s,
// logging only transitions and never writing managed or static memory.
namespace auth_state_watch_2313 {
namespace detail {

using MethodInfo = void;

// AuthSceneController, TypeDefIndex 2047.
constexpr uintptr_t kClassSlot = 0x6756500u;  // the slot Start itself uses
constexpr uintptr_t kStaticFieldsOffset = 0xB8u;
constexpr uintptr_t kClassInitOffset = 0xE0u;
constexpr uintptr_t kMethodInfoKlassOffset = 0x18u;
constexpr uintptr_t kStateFieldOffset = 0x10u;  // AuthSceneState backing field
constexpr uintptr_t kMinPointer = 0x100000000ull;

constexpr uintptr_t kApplierRva = 0x3DC2458u;      // instance, stores + mirrors
constexpr uintptr_t kStaticSetterRva = 0x3DC0968u; // generated, inlined away

constexpr useconds_t kPollIntervalUs = 200000;  // 200 ms
constexpr int kPollRounds = 150;                 // bounded to 30 seconds

using ApplierFn = void (*)(void* self, int32_t state, const MethodInfo*);
using StaticSetterFn = void (*)(int32_t state, const MethodInfo*);

inline uintptr_t g_il2cpp_base = 0;
inline ApplierFn g_applier = nullptr;
inline StaticSetterFn g_static_setter = nullptr;
inline std::atomic<void*> g_klass{nullptr};
inline std::atomic<int32_t> g_last_state{-999};
inline std::atomic<bool> g_named{false};

// __builtin_return_address must be expanded inside the hook itself, so the
// caller lookup is a macro rather than a helper function.
#define AUTHSTATE_CALLER(out)                                                  \
    do {                                                                       \
        const void* raw =                                                      \
            __builtin_extract_return_addr(__builtin_return_address(0));        \
        const uintptr_t pc = reinterpret_cast<uintptr_t>(raw) & ~uintptr_t{1}; \
        (out) = (detail::g_il2cpp_base != 0 && pc >= detail::g_il2cpp_base)    \
                    ? pc - detail::g_il2cpp_base                               \
                    : 0;                                                       \
    } while (0)

inline const char* state_name(int32_t state) {
    switch (state) {
        case -1: return "None";
        case 0: return "Initial";
        case 1: return "Authorizing";
        case 2: return "Authorized";
        case 3: return "FullySynchronized";
        case 4: return "Empty";
        case 5: return "CheckBindedId";
        case 6: return "ChooseId";
        case 7: return "ChooseProgress";
        case 8: return "SendCachedCommands";
        case 9: return "SynchronizeProgress";
        case 10: return "CheckAppVersion";
        case 11: return "Easy";
        case 12: return "CheckConnection";
        case 13: return "SendProgress";
        case 14: return "WaitAsync";
        case 15: return "TechnicalWorks";
        case 16: return "Login";
        default: return "<unknown>";
    }
}

inline bool plausible(const void* pointer, uintptr_t alignment_mask) {
    const uintptr_t value = reinterpret_cast<uintptr_t>(pointer);
    return value >= kMinPointer && (value & alignment_mask) == 0u;
}

// The applier is an instance member of AuthSceneController, so its MethodInfo
// carries exactly the class whose static the scene reads.
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
    // Start does `ldr x8, [x8, #0x500]` and only then `ldr x8, [x8]`, so the
    // slot holds a pointer to the cell that holds the class pointer.
    void** cell = *reinterpret_cast<void***>(g_il2cpp_base + kClassSlot);
    if (!plausible(cell, 0x7u)) {
        return nullptr;
    }
    void* candidate = *cell;
    if (!plausible(candidate, 0x7u)) {
        return nullptr;
    }
    g_klass.store(candidate, std::memory_order_relaxed);
    return candidate;
}

// Reads the state the same way Start reads it. Returns false while the class is
// still uninitialised or any pointer looks wrong.
inline bool read_state(int32_t* out) {
    void* klass = resolve_klass();
    if (klass == nullptr) {
        return false;
    }
    uint8_t* raw_class = reinterpret_cast<uint8_t*>(klass);
    if (*reinterpret_cast<uint32_t*>(raw_class + kClassInitOffset) == 0) {
        return false;
    }
    uint8_t* statics =
        *reinterpret_cast<uint8_t**>(raw_class + kStaticFieldsOffset);
    if (!plausible(statics, 0x3u)) {
        return false;
    }
    __builtin_memcpy(out, statics + kStateFieldOffset, sizeof(*out));
    return true;
}

// Reports only when the state actually changes, so the poller stays quiet.
inline void report(const char* when, uintptr_t caller) {
    int32_t state = 0;
    if (!read_state(&state)) {
        return;
    }
    if (g_last_state.exchange(state, std::memory_order_relaxed) == state) {
        return;
    }
    if (caller != 0) {
        LOGI("23.1.3-statewatch: state -> %d (%s) after %s "
             "caller=libil2cpp.so+0x%llx",
             state, state_name(state), when,
             static_cast<unsigned long long>(caller));
    } else {
        LOGI("23.1.3-statewatch: state -> %d (%s) after %s", state,
             state_name(state), when);
    }
}

inline void hook_applier(void* self, int32_t state, const MethodInfo* method) {
    uintptr_t caller = 0;
    AUTHSTATE_CALLER(caller);
    remember_klass(method);
    if (!g_named.exchange(true, std::memory_order_relaxed)) {
        void* klass = resolve_klass();
        const char* name = (klass != nullptr && il2cpp::class_get_name != nullptr)
                               ? il2cpp::class_get_name(klass)
                               : "<unnamed>";
        LOGI("23.1.3-statewatch: state owner resolved: %s", name);
    }
    LOGI("23.1.3-statewatch: APPLY %d (%s) self=%p caller=libil2cpp.so+0x%llx",
         state, state_name(state), self,
         static_cast<unsigned long long>(caller));
    g_applier(self, state, method);
    report("apply", caller);
}

inline void hook_static_setter(int32_t state, const MethodInfo* method) {
    uintptr_t caller = 0;
    AUTHSTATE_CALLER(caller);
    LOGI("23.1.3-statewatch: SET %d (%s) caller=libil2cpp.so+0x%llx", state,
         state_name(state), static_cast<unsigned long long>(caller));
    g_static_setter(state, method);
    remember_klass(method);
    report("set", caller);
}

// Catches the writers static analysis cannot see: the inlined stores that reach
// the class through metadata slots other than the one Start uses.
inline void* poll_thread(void*) {
    for (int round = 0; round < kPollRounds; ++round) {
        report("poll", 0);
        usleep(kPollIntervalUs);
    }
    LOGI("23.1.3-statewatch: poller finished; last state %d",
         g_last_state.load(std::memory_order_relaxed));
    return nullptr;
}

} // namespace detail

inline bool install_hooks() {
    if (!elfsym::find_library("libil2cpp.so", &detail::g_il2cpp_base) ||
        detail::g_il2cpp_base == 0) {
        LOGW("23.1.3-statewatch: libil2cpp.so base not found; the auth scene "
             "state stays unwatched");
        return false;
    }

    int armed = 0;
    if (hook::install_address(
            reinterpret_cast<void*>(detail::g_il2cpp_base + detail::kApplierRva),
            "AuthScene.applyState",
            reinterpret_cast<void*>(&detail::hook_applier),
            reinterpret_cast<void**>(&detail::g_applier), false)) {
        ++armed;
    }
    if (hook::install_address(
            reinterpret_cast<void*>(detail::g_il2cpp_base +
                                    detail::kStaticSetterRva),
            "AuthScene.state.set",
            reinterpret_cast<void*>(&detail::hook_static_setter),
            reinterpret_cast<void**>(&detail::g_static_setter), false)) {
        ++armed;
    }

    pthread_t poller;
    const bool polling =
        pthread_create(&poller, nullptr, &detail::poll_thread, nullptr) == 0;
    if (polling) {
        pthread_detach(poller);
    } else {
        LOGW("23.1.3-statewatch: poller thread refused to start");
    }

    LOGI("23.1.3-statewatch: %d/2 hooks installed, bounded sampler %s "
         "(150 read-only samples over 30 seconds)",
         armed, polling ? "running" : "absent");
    return armed > 0 || polling;
}

} // namespace auth_state_watch_2313

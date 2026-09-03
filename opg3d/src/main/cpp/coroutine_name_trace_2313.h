#pragma once

#include <atomic>
#include <cstdint>

#include "elf_sym.h"
#include "hook.h"
#include "il2cpp.h"
#include "log.h"

// PG3D 23.1.3 ARM64 -- names the coroutine the boot dies waiting on, then
// follows that coroutine step by step.
//
// Runs 76-83 established that the loading sequence itself completes. Both
// Switcher iterators are declared IEnumerable<float>, the floats they yield are
// the progress values, and the highest either yields is 0.750:
// InitializeSwitcher runs to its end (state=45, MoveNext returns false),
// LoadMainMenu then runs and its tail requests the main-menu scene. The dump
// confirms neither can block - Switcher.<LoadMainMenu> (TypeDefIndex 6034)
// holds only <>1__state, <>2__current, <>l__initialThreadId and <>4__this, so
// it owns no enumerator, no async operation and no coroutine.
//
// Exactly one await in the whole boot never finishes. The await trace reports
//
//   coroutine 0x3DC4758 waits on class=Coroutine object=0x... call#3
//
// from AuthSceneController.<iterator> (TypeDefIndex 2028), whose first two
// waits complete and whose call#4 is never logged.
//
// Picking an entry point that could name that handle took two runs.
// MonoBehaviour has two single-argument StartCoroutine overloads, and resolving
// by name and argument count picks the string one, which logged a single
// UICamera start and nothing else (run 82). StartCoroutineManaged2 has a unique
// name and hooks cleanly, but run 83 armed it and it was never called once. So
// the enumerator overload is hooked by address, the same way
// auth_scene_sweep_2313 reaches bodies metadata cannot name unambiguously.
//
// Run 84 then closed the identification: 132 starts were recorded and the dead
// handle matched start #82 -
//
//   AuthSceneController.\u4e18\u4e0d\u4e16\u4e1d\u4e09\u4e17\u4e01\u4e0c\u4e18() at RVA 0x3DC6C48,
//   state machine TypeDefIndex 2044, MoveNext at RVA 0x3DCAB8C.
//
// That machine holds only <>1__state and <>2__current - it captures no `this`,
// so it cannot be blocked on owner state; it is parked on whatever it yielded.
// The last two hooks below follow it directly: every MoveNext reports the state
// it entered and left and whether it asked to continue, and every get_Current
// reports the managed class of the object it handed Unity. Between them they
// name the exact wait that never ends. Passive passthrough throughout.
namespace coroutine_name_trace_2313 {
namespace detail {

using MethodInfo = void;

// public Coroutine StartCoroutine(IEnumerator routine)
// dump2313.cs line 1076144: RVA 0x4436094. Shared with the deprecated
// StartCoroutine_Auto alias, which is why it cannot be named through metadata.
constexpr uintptr_t kStartCoroutineEnumeratorRva = 0x4436094;

// The coroutine identified in run 84 as the one that never completes.
constexpr uintptr_t kHungMoveNextRva = 0x3DCAB8C;
constexpr uintptr_t kHungCurrentRva = 0x3DCAD24;

// Iterator state machines keep <>1__state at 0x10 (dump2313.cs TypeDefIndex
// 2044) and <>2__current at 0x18.
constexpr uintptr_t kStateOffset = 0x10;

// The boot starts a few hundred coroutines. The cap keeps the log bounded while
// staying far above the count reached by the time the boot parks at 90%.
constexpr int32_t kMaxReports = 400;

inline const char* class_name(void* object) {
    if (object == nullptr) return "null";
    if (il2cpp::object_get_class == nullptr || il2cpp::class_get_name == nullptr) {
        return "unresolved";
    }
    void* klass = il2cpp::object_get_class(object);
    const char* name = klass != nullptr ? il2cpp::class_get_name(klass) : nullptr;
    return name != nullptr ? name : "unnamed";
}

inline int32_t machine_state(void* self) {
    if (self == nullptr) return -9999;
    return *reinterpret_cast<int32_t*>(reinterpret_cast<char*>(self) +
                                       kStateOffset);
}

using StartFn = void* (*)(void* self, void* routine, const MethodInfo* method);
inline StartFn g_start = nullptr;
inline std::atomic<int32_t> g_start_calls;

void* start_enumerator(void* self, void* routine, const MethodInfo* method) {
    void* const handle =
        g_start != nullptr ? g_start(self, routine, method) : nullptr;
    const int32_t call =
        g_start_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    if (call <= kMaxReports) {
        LOGI("23.1.3-coro: #%d %s started %s -> Coroutine=%p", call,
             class_name(self), class_name(routine), handle);
    }
    return handle;
}

// public Coroutine StartCoroutine(string methodName). A coroutine started by
// name never passes an enumerator, so without this a by-name start would be
// invisible in the correlation.
using ByNameFn = void* (*)(void* self, void* name, const MethodInfo* method);
inline ByNameFn g_by_name = nullptr;
inline std::atomic<int32_t> g_by_name_calls;

void* start_by_name(void* self, void* name, const MethodInfo* method) {
    void* const handle =
        g_by_name != nullptr ? g_by_name(self, name, method) : nullptr;
    const int32_t call =
        g_by_name_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    if (call <= kMaxReports) {
        LOGI("23.1.3-coro: byname#%d %s started a coroutine -> Coroutine=%p",
             call, class_name(self), handle);
    }
    return handle;
}

// bool MoveNext() on the hung machine.
using MoveNextFn = bool (*)(void* self, const MethodInfo* method);
inline MoveNextFn g_hung_move_next = nullptr;
inline std::atomic<int32_t> g_hung_calls;

bool hung_move_next(void* self, const MethodInfo* method) {
    const int32_t entered = machine_state(self);
    const int32_t call = g_hung_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    const bool more =
        g_hung_move_next != nullptr ? g_hung_move_next(self, method) : false;
    const int32_t left = machine_state(self);
    if (call <= kMaxReports) {
        LOGI("23.1.3-coro[hung]: MoveNext #%d entered state=%d left state=%d "
             "more=%d",
             call, entered, left, more ? 1 : 0);
    }
    return more;
}

// object IEnumerator.get_Current() on the hung machine: the object Unity is
// told to wait for.
using CurrentFn = void* (*)(void* self, const MethodInfo* method);
inline CurrentFn g_hung_current = nullptr;
inline std::atomic<int32_t> g_hung_current_calls;

void* hung_current(void* self, const MethodInfo* method) {
    void* const value =
        g_hung_current != nullptr ? g_hung_current(self, method) : nullptr;
    const int32_t call =
        g_hung_current_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    if (call <= kMaxReports) {
        LOGI("23.1.3-coro[hung]: yielded class=%s object=%p at state=%d call#%d",
             class_name(value), value, machine_state(self), call);
    }
    return value;
}

} // namespace detail

inline bool install_hooks() {
    int armed = 0;

    uintptr_t il2cpp_base = 0;
    const bool found_il2cpp =
        elfsym::find_library("libil2cpp.so", &il2cpp_base) && il2cpp_base != 0;
    if (found_il2cpp) {
        armed += hook::install_address(
                     reinterpret_cast<void*>(
                         il2cpp_base + detail::kStartCoroutineEnumeratorRva),
                     "23.1.3 MonoBehaviour.StartCoroutine(IEnumerator) trace",
                     reinterpret_cast<void*>(&detail::start_enumerator),
                     reinterpret_cast<void**>(&detail::g_start), false)
                     ? 1
                     : 0;
        armed += hook::install_address(
                     reinterpret_cast<void*>(il2cpp_base +
                                             detail::kHungMoveNextRva),
                     "23.1.3 hung AuthScene coroutine MoveNext trace",
                     reinterpret_cast<void*>(&detail::hung_move_next),
                     reinterpret_cast<void**>(&detail::g_hung_move_next), false)
                     ? 1
                     : 0;
        armed += hook::install_address(
                     reinterpret_cast<void*>(il2cpp_base +
                                             detail::kHungCurrentRva),
                     "23.1.3 hung AuthScene coroutine get_Current trace",
                     reinterpret_cast<void*>(&detail::hung_current),
                     reinterpret_cast<void**>(&detail::g_hung_current), false)
                     ? 1
                     : 0;
    } else {
        LOGW("23.1.3-coro: libil2cpp.so base not found; the enumerator "
             "overload and the hung coroutine cannot be hooked by address");
    }

    armed += hook::install({"UnityEngine", "MonoBehaviour", "StartCoroutine", 1},
                           reinterpret_cast<void*>(&detail::start_by_name),
                           reinterpret_cast<void**>(&detail::g_by_name), false)
                 ? 1
                 : 0;

    LOGI("23.1.3-coro: coroutine name trace armed on %d/4 entry point(s); "
         "every started coroutine is reported with its IEnumerator type, and "
         "the coroutine that never completes is followed step by step",
         armed);
    return armed > 0;
}

} // namespace coroutine_name_trace_2313

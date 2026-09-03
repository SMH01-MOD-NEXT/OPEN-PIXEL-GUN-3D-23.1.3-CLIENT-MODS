#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "elf_sym.h"
#include "hook.h"
#include "log.h"

// Passive trace of the stock progress-command barrier used by AuthScene.
//
// The last startup iterator records ProgressUpdateType.AfterProgressSync (274),
// registers AuthSceneController.0x3DC70F8 as an Action, flushes pending progress
// commands, then releases one barrier reference. That Action starts the stock
// Login coroutine. Run 156 reached the later localMultyKey fallback instead,
// which means an earlier progress command still owned a barrier reference.
// These hooks report every acquire/release and callback registration/drain so
// the outstanding operation can be matched to its Socket.IO request. Nothing
// here changes an argument, return value, counter, queue, or callback.
namespace progress_sync_balance_trace_2313 {
namespace detail {

using MethodInfo = void;
using AcquireFn = void (*)(void* self, int32_t event_type,
                           const MethodInfo* method);
using ReleaseFn = void (*)(void* self, void* response,
                           const MethodInfo* method);
using RegisterFn = uint32_t (*)(void* self, void* action, int32_t mode,
                                uint32_t token, const MethodInfo* method);
using VoidFn = void (*)(void* self, const MethodInfo* method);

constexpr uintptr_t kAcquireRva = 0x1B44114u;
constexpr uintptr_t kReleaseRva = 0x1B44230u;
constexpr uintptr_t kRegisterRva = 0x1B5DAC8u;
constexpr uintptr_t kDrainRva = 0x1B5D778u;
constexpr uintptr_t kFlushRva = 0x1B4648Cu;

constexpr uintptr_t kBarrierCountOffset = 0xD0u;
constexpr uintptr_t kSequenceOffset = 0xD4u;
constexpr uintptr_t kWaitersOffset = 0xE0u;
constexpr uintptr_t kListSizeOffset = 0x18u;
constexpr uintptr_t kDelegateMethodOffset = 0x10u;
constexpr uintptr_t kDelegateTargetOffset = 0x20u;
constexpr int32_t kAfterProgressSync = 274;

inline uintptr_t g_il2cpp_base = 0;
inline AcquireFn g_acquire = nullptr;
inline ReleaseFn g_release = nullptr;
inline RegisterFn g_register = nullptr;
inline VoidFn g_drain = nullptr;
inline VoidFn g_flush = nullptr;
inline std::atomic<uint64_t> g_sequence{0};

inline int32_t read_i32(void* object, uintptr_t offset) {
    if (object == nullptr) return INT32_MIN;
    return *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(object) +
                                      offset);
}

inline void* read_ref(void* object, uintptr_t offset) {
    if (object == nullptr) return nullptr;
    return *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(object) +
                                     offset);
}

inline int32_t waiters(void* self) {
    void* list = read_ref(self, kWaitersOffset);
    return list != nullptr ? read_i32(list, kListSizeOffset) : 0;
}

inline unsigned long long rva(void* pointer) {
    const uintptr_t address = reinterpret_cast<uintptr_t>(pointer);
    if (g_il2cpp_base == 0 || address < g_il2cpp_base) return 0ull;
    return static_cast<unsigned long long>(address - g_il2cpp_base);
}

#define PSYNC_CALLER(out)                                                       \
    do {                                                                        \
        const void* raw =                                                       \
            __builtin_extract_return_addr(__builtin_return_address(0));         \
        const uintptr_t pc = reinterpret_cast<uintptr_t>(raw) & ~uintptr_t{1};  \
        (out) = (detail::g_il2cpp_base != 0 && pc >= detail::g_il2cpp_base)     \
                    ? pc - detail::g_il2cpp_base                                \
                    : 0;                                                        \
    } while (0)

inline void hook_acquire(void* self, int32_t event_type,
                         const MethodInfo* method) {
    uintptr_t caller = 0;
    PSYNC_CALLER(caller);
    const int32_t before = read_i32(self, kBarrierCountOffset);
    const int32_t before_waiters = waiters(self);
    if (g_acquire != nullptr) g_acquire(self, event_type, method);
    const int32_t after = read_i32(self, kBarrierCountOffset);
    const uint64_t seq = g_sequence.fetch_add(1, std::memory_order_relaxed) + 1;
    LOGI("23.1.3-psync: #%llu ACQUIRE type=%d%s count=%d->%d waiters=%d->%d "
         "caller=libil2cpp.so+0x%llx self=%p",
         static_cast<unsigned long long>(seq), event_type,
         event_type == kAfterProgressSync ? "(AfterProgressSync)" : "", before,
         after, before_waiters, waiters(self),
         static_cast<unsigned long long>(caller), self);
}

inline void hook_release(void* self, void* response, const MethodInfo* method) {
    uintptr_t caller = 0;
    PSYNC_CALLER(caller);
    const int32_t before = read_i32(self, kBarrierCountOffset);
    const int32_t before_waiters = waiters(self);
    if (g_release != nullptr) g_release(self, response, method);
    const int32_t after = read_i32(self, kBarrierCountOffset);
    const uint64_t seq = g_sequence.fetch_add(1, std::memory_order_relaxed) + 1;
    LOGI("23.1.3-psync: #%llu RELEASE count=%d->%d waiters=%d->%d "
         "response=%p caller=libil2cpp.so+0x%llx self=%p",
         static_cast<unsigned long long>(seq), before, after, before_waiters,
         waiters(self), response, static_cast<unsigned long long>(caller), self);
}

inline uint32_t hook_register(void* self, void* action, int32_t mode,
                              uint32_t token, const MethodInfo* method) {
    uintptr_t caller = 0;
    PSYNC_CALLER(caller);
    const int32_t before = read_i32(self, kBarrierCountOffset);
    void* target = read_ref(action, kDelegateTargetOffset);
    void* function = read_ref(action, kDelegateMethodOffset);
    const uint32_t id = g_register != nullptr
                            ? g_register(self, action, mode, token, method)
                            : 0u;
    const uint64_t seq = g_sequence.fetch_add(1, std::memory_order_relaxed) + 1;
    LOGI("23.1.3-psync: #%llu REGISTER mode=%d token=%u id=%u count=%d->%d "
         "waiters=%d action=%p target=%p method=libil2cpp.so+0x%llx "
         "caller=libil2cpp.so+0x%llx self=%p",
         static_cast<unsigned long long>(seq), mode, token, id, before,
         read_i32(self, kBarrierCountOffset), waiters(self), action, target,
         rva(function), static_cast<unsigned long long>(caller), self);
    return id;
}

inline void hook_drain(void* self, const MethodInfo* method) {
    uintptr_t caller = 0;
    PSYNC_CALLER(caller);
    const int32_t before = read_i32(self, kBarrierCountOffset);
    const int32_t before_waiters = waiters(self);
    LOGI("23.1.3-psync: DRAIN ENTER count=%d waiters=%d sequence=%d "
         "caller=libil2cpp.so+0x%llx self=%p",
         before, before_waiters, read_i32(self, kSequenceOffset),
         static_cast<unsigned long long>(caller), self);
    if (g_drain != nullptr) g_drain(self, method);
    LOGI("23.1.3-psync: DRAIN RETURN count=%d waiters=%d self=%p",
         read_i32(self, kBarrierCountOffset), waiters(self), self);
}

inline void hook_flush(void* self, const MethodInfo* method) {
    uintptr_t caller = 0;
    PSYNC_CALLER(caller);
    LOGI("23.1.3-psync: FLUSH ENTER count=%d waiters=%d "
         "caller=libil2cpp.so+0x%llx self=%p",
         read_i32(self, kBarrierCountOffset), waiters(self),
         static_cast<unsigned long long>(caller), self);
    if (g_flush != nullptr) g_flush(self, method);
    LOGI("23.1.3-psync: FLUSH RETURN count=%d waiters=%d self=%p",
         read_i32(self, kBarrierCountOffset), waiters(self), self);
}

#undef PSYNC_CALLER

} // namespace detail

inline bool install_hooks() {
    uintptr_t base = 0;
    if (!elfsym::find_library("libil2cpp.so", &base) || base == 0) {
        LOGW("23.1.3-psync: libil2cpp.so base not found");
        return false;
    }
    detail::g_il2cpp_base = base;

    struct Target {
        uintptr_t rva;
        const char* label;
        void* replacement;
        void** original;
    };
    const Target targets[] = {
        {detail::kAcquireRva, "23.1.3 progress barrier acquire",
         reinterpret_cast<void*>(&detail::hook_acquire),
         reinterpret_cast<void**>(&detail::g_acquire)},
        {detail::kReleaseRva, "23.1.3 progress barrier release",
         reinterpret_cast<void*>(&detail::hook_release),
         reinterpret_cast<void**>(&detail::g_release)},
        {detail::kRegisterRva, "23.1.3 progress barrier callback register",
         reinterpret_cast<void*>(&detail::hook_register),
         reinterpret_cast<void**>(&detail::g_register)},
        {detail::kDrainRva, "23.1.3 progress barrier drain",
         reinterpret_cast<void*>(&detail::hook_drain),
         reinterpret_cast<void**>(&detail::g_drain)},
        {detail::kFlushRva, "23.1.3 progress queue flush",
         reinterpret_cast<void*>(&detail::hook_flush),
         reinterpret_cast<void**>(&detail::g_flush)},
    };

    int armed = 0;
    for (const Target& target : targets) {
        if (hook::install_address(reinterpret_cast<void*>(base + target.rva),
                                  target.label, target.replacement,
                                  target.original, false)) {
            ++armed;
        }
    }
    LOGI("23.1.3-psync: armed %d/%zu passive balance hooks", armed,
         sizeof(targets) / sizeof(targets[0]));
    return armed == static_cast<int>(sizeof(targets) / sizeof(targets[0]));
}

} // namespace progress_sync_balance_trace_2313

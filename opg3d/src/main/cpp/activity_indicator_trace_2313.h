#pragma once

// Attributes every write of the boot loading percentage in 23.1.3.
//
// Run 89 named the widget: the percentage is a plain UILabel, never an NGUI
// UIProgressBar, and every caption it shows is written from
// ActivityIndicator's static float setter at RVA 0x1E18DC0. That run also
// showed the label counting 75, 76, 79, 80, 81, 87, 88, 90 and then never
// changing again, so the boot does not die after some later failure -- it
// simply stops asking for a higher number.
//
// What run 89 could not answer is which of the 25 native call sites emits that
// final 0.90, because a hook on the label only ever sees the label. This trace
// hooks the writer itself and reports the return address of each call, so the
// emitter of the last value is named instead of inferred. The caller is
// printed as a libil2cpp.so RVA, which tools/resolve_rva.py maps straight back
// to a method.
//
// The writer is static and takes a single float, so its IL2CPP entry point is
// void(float, const MethodInfo*). The hook calls straight through and changes
// no value, no timing and no control flow.

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "elf_sym.h"
#include "hook.h"
#include "log.h"

namespace activity_indicator_trace_2313 {
namespace detail {

using MethodInfo = void;
using IndicatorSetFn = void (*)(float value, const MethodInfo* method);

// Filled by hook::install_address at runtime.
inline IndicatorSetFn g_indicator_set = nullptr;
inline uintptr_t g_il2cpp_base = 0;

// ActivityIndicator's percentage setter. Named only by address because the
// managed name is obfuscated to CJK codepoints in this build.
constexpr uintptr_t kIndicatorSetRva = 0x1E18DC0u;

constexpr size_t kSeenSlots = 1024u;

inline std::atomic<uint64_t> g_seen[kSeenSlots];

// A stalled boot repaints the same value forever, so the trace is budgeted.
inline std::atomic<int> g_budget{600};
inline std::atomic<int> g_calls{0};

inline uint64_t mix(uint64_t h) noexcept {
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdULL;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53ULL;
    h ^= h >> 33;
    return h;
}

// Single-slot-per-key dedup. A collision costs one extra line, never a missed
// call site, which is the right trade for an attribution trace.
inline bool first_time(uint64_t key) noexcept {
    if (key == 0u) key = 1u;
    const size_t slot = static_cast<size_t>(mix(key) % kSeenSlots);
    return g_seen[slot].exchange(key, std::memory_order_relaxed) != key;
}

inline void hook_indicator_set(float value, const MethodInfo* method) {
    const void* raw =
        __builtin_extract_return_addr(__builtin_return_address(0));
    const uintptr_t pc = reinterpret_cast<uintptr_t>(raw) & ~uintptr_t{1};
    const uintptr_t caller =
        g_il2cpp_base != 0 && pc >= g_il2cpp_base ? pc - g_il2cpp_base : 0;

    const int seq = g_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    const bool finite = std::isfinite(value);
    const int32_t quantised =
        finite ? static_cast<int32_t>(value * 1000.0f) : INT32_MIN;
    const uint64_t key =
        (static_cast<uint64_t>(caller) * 0x9e3779b97f4a7c15ULL) ^
        static_cast<uint64_t>(static_cast<uint32_t>(quantised));

    if (first_time(key) &&
        g_budget.fetch_sub(1, std::memory_order_relaxed) > 0) {
        LOGI("23.1.3-indicator: #%d value=%.4f caller=libil2cpp.so+0x%llx",
             seq, static_cast<double>(value),
             static_cast<unsigned long long>(caller));
    }

    if (g_indicator_set != nullptr) {
        g_indicator_set(value, method);
    }
}

} // namespace detail

// Call once after Assembly-CSharp.dll is ready (from main.cpp init_thread).
inline bool install_hooks() {
    uintptr_t base = 0;
    if (!elfsym::find_library("libil2cpp.so", &base) || base == 0) {
        LOGW("23.1.3-indicator: libil2cpp.so base not found; the writer of the "
             "loading percentage cannot be hooked by address");
        return false;
    }
    detail::g_il2cpp_base = base;

    const bool armed = hook::install_address(
        reinterpret_cast<void*>(base + detail::kIndicatorSetRva),
        "23.1.3 ActivityIndicator percentage writer trace",
        reinterpret_cast<void*>(&detail::hook_indicator_set),
        reinterpret_cast<void**>(&detail::g_indicator_set), false);

    if (armed) {
        LOGI("23.1.3-indicator: percentage writer armed at "
             "libil2cpp.so+0x%llx",
             static_cast<unsigned long long>(detail::kIndicatorSetRva));
    } else {
        LOGW("23.1.3-indicator: percentage writer hook failed; the emitter of "
             "the final value will stay unnamed");
    }
    return armed;
}

} // namespace activity_indicator_trace_2313

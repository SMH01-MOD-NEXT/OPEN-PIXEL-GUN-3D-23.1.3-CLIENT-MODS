#pragma once

#include <atomic>
#include <cstdint>
#include <cstring>
#include <string>

#include "elf_sym.h"
#include "hook.h"
#include "il2cpp.h"
#include "log.h"

// Passive diagnostics for the persisted flag that gates the authorization
// scene's Authorized branch.
//
// Run 90 located the 90% wall exactly: the scene dispatches state 2
// (Authorized) once, the dispatcher gate at libil2cpp.so+0x2B71728 returns
// false, the Authorized branch therefore never requests its coroutine,
// MainMenuController never awakes, and the loading label stops at 90%.
//
// Disassembling that gate shows it is not a network answer at all. It reads a
// single key out of the game's own Storager (TypeDefIndex 4790) and behaves as:
//
//     if (Storager.HasKey(KEY)) return Storager.GetInt(KEY, ...) == 1;
//     Storager.SetInt(KEY, 0, ...);
//     return false;
//
// These hooks name KEY and report the value behind it. Calls that come from the
// gate itself are always reported; every other caller is reported once per
// distinct key so the whole persisted-state picture of a boot is visible
// without flooding the log. Nothing here changes client behavior: every hook
// logs and then calls the stock body.
namespace gate_key_trace_2313 {
namespace detail {

using MethodInfo = void;
using HasKeyFn = bool (*)(void* key, const MethodInfo* method);
using GetIntFn = int32_t (*)(void* key, bool a, bool b, bool c,
                            const MethodInfo* method);
using SetIntFn = void (*)(void* key, int32_t value, bool a, bool b,
                          const MethodInfo* method);

// Storager members, resolved from dump2313.cs (TypeDefIndex 4790).
constexpr uintptr_t kHasKeyRva = 0x4573800u;
constexpr uintptr_t kGetIntRva = 0x4573458u;
constexpr uintptr_t kSetIntRva = 0x4572DF0u;

// The dispatcher gate, so its own reads can be told apart from every other
// caller in the game.
constexpr uintptr_t kGateStart = 0x2B71728u;
constexpr uintptr_t kGateEnd = 0x2B717E0u;

constexpr size_t kSeenSlots = 4096;
constexpr int32_t kDistinctBudget = 500;
constexpr size_t kKeyPreview = 192;

inline HasKeyFn g_has_key = nullptr;
inline GetIntFn g_get_int = nullptr;
inline SetIntFn g_set_int = nullptr;
inline uintptr_t g_il2cpp_base = 0;
inline std::atomic<uint64_t> g_seen[kSeenSlots];
inline std::atomic<int32_t> g_distinct{0};

uintptr_t caller_rva() {
    const void* raw = __builtin_extract_return_addr(__builtin_return_address(0));
    const uintptr_t pc = reinterpret_cast<uintptr_t>(raw) & ~uintptr_t{1};
    return g_il2cpp_base != 0 && pc >= g_il2cpp_base ? pc - g_il2cpp_base : 0;
}

bool from_gate(uintptr_t rva) {
    return rva >= kGateStart && rva <= kGateEnd;
}

// Return sites inside the state-13/state-9 predicate at 0x3DC7F78. These must
// bypass the distinct-key budget: the boot reads hundreds of unrelated keys
// before it reaches this fork.
bool from_sync_progress_predicate(uintptr_t rva) {
    return rva == 0x3DC7FF8u || rva == 0x3DC8044u;
}

uint64_t fingerprint(const std::string& text, char tag) {
    uint64_t hash = 1469598103934665603ull;
    hash = (hash ^ static_cast<uint8_t>(tag)) * 1099511628211ull;
    for (const char c : text) {
        hash = (hash ^ static_cast<uint8_t>(c)) * 1099511628211ull;
    }
    return hash == 0 ? 1 : hash;
}

// True the first time a given key is seen, false afterwards.
bool first_sighting(const std::string& text, char tag) {
    const uint64_t mark = fingerprint(text, tag);
    size_t slot = static_cast<size_t>(mark) % kSeenSlots;
    for (size_t probe = 0; probe < kSeenSlots; ++probe) {
        uint64_t expected = 0;
        std::atomic<uint64_t>& cell = g_seen[slot];
        if (cell.compare_exchange_strong(expected, mark,
                                         std::memory_order_relaxed)) {
            return g_distinct.fetch_add(1, std::memory_order_relaxed) <
                   kDistinctBudget;
        }
        if (expected == mark) {
            return false;
        }
        slot = (slot + 1) % kSeenSlots;
    }
    return false;
}

std::string key_text(void* key) {
    return key != nullptr ? il2cpp::to_utf8(key, kKeyPreview)
                          : std::string("<null>");
}

bool hook_has_key(void* key, const MethodInfo* method) {
    const uintptr_t caller = caller_rva();
    const bool result = g_has_key != nullptr ? g_has_key(key, method) : false;
    const std::string text = key_text(key);
    if (from_sync_progress_predicate(caller)) {
        LOGI("23.1.3-gatekey: SYNC-PROGRESS asks HasKey('%s') -> %d "
             "caller=libil2cpp.so+0x%llx",
             text.c_str(), result ? 1 : 0,
             static_cast<unsigned long long>(caller));
    } else if (from_gate(caller)) {
        LOGI("23.1.3-gatekey: THE GATE asks HasKey('%s') -> %d", text.c_str(),
             result ? 1 : 0);
    } else if (first_sighting(text, 'h')) {
        LOGI("23.1.3-gatekey: HasKey('%s') -> %d caller=libil2cpp.so+0x%llx",
             text.c_str(), result ? 1 : 0,
             static_cast<unsigned long long>(caller));
    }
    return result;
}

int32_t hook_get_int(void* key, bool a, bool b, bool c,
                     const MethodInfo* method) {
    const uintptr_t caller = caller_rva();
    const int32_t result =
        g_get_int != nullptr ? g_get_int(key, a, b, c, method) : 0;
    const std::string text = key_text(key);
    if (from_sync_progress_predicate(caller)) {
        LOGI("23.1.3-gatekey: SYNC-PROGRESS reads GetInt('%s') -> %d "
             "caller=libil2cpp.so+0x%llx (predicate passes only on 0)",
             text.c_str(), result,
             static_cast<unsigned long long>(caller));
    } else if (from_gate(caller)) {
        LOGI("23.1.3-gatekey: THE GATE reads GetInt('%s') -> %d (gate passes "
             "only on 1)",
             text.c_str(), result);
    } else if (first_sighting(text, 'g')) {
        LOGI("23.1.3-gatekey: GetInt('%s') -> %d caller=libil2cpp.so+0x%llx",
             text.c_str(), result,
             static_cast<unsigned long long>(caller));
    }
    return result;
}

void hook_set_int(void* key, int32_t value, bool a, bool b,
                  const MethodInfo* method) {
    const uintptr_t caller = caller_rva();
    const std::string text = key_text(key);
    if (from_gate(caller)) {
        LOGI("23.1.3-gatekey: THE GATE writes SetInt('%s', %d) -- the key was "
             "absent and is being created",
             text.c_str(), value);
    } else if (first_sighting(text, 's')) {
        LOGI("23.1.3-gatekey: SetInt('%s', %d) caller=libil2cpp.so+0x%llx",
             text.c_str(), value,
             static_cast<unsigned long long>(caller));
    }
    if (g_set_int != nullptr) g_set_int(key, value, a, b, method);
}

} // namespace detail

inline bool install_hooks() {
    if (!elfsym::find_library("libil2cpp.so", &detail::g_il2cpp_base) ||
        detail::g_il2cpp_base == 0) {
        LOGW("23.1.3-gatekey: libil2cpp.so base not found; the gate key stays "
             "unnamed");
        return false;
    }

    for (size_t i = 0; i < detail::kSeenSlots; ++i) {
        detail::g_seen[i].store(0, std::memory_order_relaxed);
    }

    int armed = 0;
    if (hook::install_address(
            reinterpret_cast<void*>(detail::g_il2cpp_base + detail::kHasKeyRva),
            "Storager.HasKey",
            reinterpret_cast<void*>(&detail::hook_has_key),
            reinterpret_cast<void**>(&detail::g_has_key), false)) {
        ++armed;
    }
    if (hook::install_address(
            reinterpret_cast<void*>(detail::g_il2cpp_base + detail::kGetIntRva),
            "Storager.GetInt",
            reinterpret_cast<void*>(&detail::hook_get_int),
            reinterpret_cast<void**>(&detail::g_get_int), false)) {
        ++armed;
    }
    if (hook::install_address(
            reinterpret_cast<void*>(detail::g_il2cpp_base + detail::kSetIntRva),
            "Storager.SetInt",
            reinterpret_cast<void*>(&detail::hook_set_int),
            reinterpret_cast<void**>(&detail::g_set_int), false)) {
        ++armed;
    }

    LOGI("23.1.3-gatekey: %d/3 Storager hooks installed; the dispatcher gate "
         "lives at libil2cpp.so+0x%llx",
         armed, static_cast<unsigned long long>(detail::kGateStart));
    return armed > 0;
}

} // namespace gate_key_trace_2313

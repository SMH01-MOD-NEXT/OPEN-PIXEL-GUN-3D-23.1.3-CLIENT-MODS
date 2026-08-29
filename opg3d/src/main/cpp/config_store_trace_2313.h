#pragma once

#include <algorithm>
#include <cstdint>
#include <string>

#include "hook.h"
#include "il2cpp.h"
#include "log.h"

// Names what reaches the client's binary config store, and where it came from.
//
// Why this exists. slot_config_trace_2313 measured the document the client
// parses: 596 characters, fourteen application versions from 15.99.2 to 16.2.2,
// every entry carrying exactly `slots` and `commands`, and no entry for the
// served build 23.1.3. That is the whole of the blocker - a stale copy shipped
// inside the APK, which only a server can replace.
//
// The backend now hosts a copy that carries 23.1.3, and three spellings of the
// config entry in the subscribe answer have been put on the device: the body as
// JSON text, the body as a nested object, and the body next to a version field.
// All three were refuted by the same measurement - the answer arrives, the
// server records it as served, and about 3.5 seconds later the client's parser
// still receives its own 596-character embedded copy. Nothing was written into
// the config store either, and no HTTP fetch for this config followed.
//
// So the ack carries hashes and the document travels some other way. The config
// registry reads through PGCompany's binary config store (dump2313.cs:448789,
// TypeDefIndex 11078, version const "18.1.0", key const
// "BinaryConfigStorage.Key"), and a server copy that never lands in that store
// can only ever lose to the embedded fallback. Hooking its save and its two
// reads answers both open questions at once: whether anything is ever stored
// from the socket path, and if something is, what an accepted document looks
// like on the way in.
//
// Nothing is altered: every call is forwarded to the stock implementation and
// its result is returned unchanged.
namespace config_store_trace_2313 {
namespace detail {

using MethodInfo = void;

constexpr const char* kNamespace = "PGCompany";
// Spelled as UTF-8 bytes so this file's source encoding cannot change which
// type or method is resolved.
constexpr const char* kStoreClass = u8"\u4e05\u4e1d\u4e1a\u4e03\u4e09\u4e08\u4e1d\u4e11\u4e0f";

// dump2313.cs:448836 `internal bool <save>(ConfigId, byte[], out string)`.
constexpr const char* kTrySave = u8"\u4e0e\u4e07\u4e1d\u4e17\u4e01\u4e0d\u4e17\u4e00\u4e17";
// dump2313.cs:448842 `internal bool <loadOrFallback>(ConfigId, out byte[],
// out string)`.
constexpr const char* kTryLoadOrFallback = u8"\u4e0e\u4e0c\u4e0b\u4e11\u4e1d\u4e01\u4e04\u4e0f\u4e1b";
// dump2313.cs:448839 `internal bool <loadCore>(ConfigId, out byte[],
// out string)`.
constexpr const char* kTryLoadCore = u8"\u4e1c\u4e17\u4e0e\u4e0f\u4e1f\u4e1b\u4e02\u4e09\u4e1e";

// dump2313.cs:449064 `public const ConfigId CommandsWithFullSlots = 7`.
constexpr int32_t kFullSlotsConfigId = 7;

constexpr uint64_t kMaxPlausibleBytes = 64u * 1024u * 1024u;
constexpr size_t kPreviewBytes = 96u;

// Instance methods, so `self` comes first; ConfigId is an enum and travels as a
// plain int32; every `out` parameter is a pointer to a managed reference.
using SaveFn = bool (*)(void*, int32_t, void*, void**, const MethodInfo*);
using LoadFn = bool (*)(void*, int32_t, void**, void**, const MethodInfo*);

inline SaveFn g_save = nullptr;
inline LoadFn g_load_or_fallback = nullptr;
inline LoadFn g_load_core = nullptr;

// Il2CppArray on 64-bit: klass, monitor, bounds, then max_length at 0x18 and
// the payload at 0x20. Only the length is read, and an implausible value is
// reported rather than trusted, because a wrong guess about this layout must
// not turn a diagnostic into a crash.
inline long array_length(void* array) {
    if (array == nullptr) return -1;
    const uint64_t length = *reinterpret_cast<const uint64_t*>(
        reinterpret_cast<const char*>(array) + 0x18);
    if (length > kMaxPlausibleBytes) return -2;
    return static_cast<long>(length);
}

// A JSON document and a sealed blob are told apart at a glance, which is the
// whole reason for printing a few bytes of one.
inline std::string array_preview(void* array, size_t max) {
    const long size = array_length(array);
    if (size <= 0) return std::string();
    const auto* bytes = reinterpret_cast<const unsigned char*>(
        reinterpret_cast<const char*>(array) + 0x20);
    const size_t count = std::min(static_cast<size_t>(size), max);
    std::string out;
    out.reserve(count);
    for (size_t i = 0u; i < count; ++i) {
        const unsigned char c = bytes[i];
        out.push_back((c >= 0x20u && c < 0x7Fu) ? static_cast<char>(c) : '.');
    }
    return out;
}

inline const char* names(int32_t config_id) {
    return config_id == kFullSlotsConfigId ? " (CommandsWithFullSlots)" : "";
}

bool hook_save(void* self, int32_t config_id, void* payload, void** message,
               const MethodInfo* method) {
    LOGW("23.1.3-storetrace: save id=%d bytes=%ld%s head='%s'", config_id,
         array_length(payload), names(config_id),
         array_preview(payload, kPreviewBytes).c_str());
    if (g_save == nullptr) return false;
    return g_save(self, config_id, payload, message, method);
}

bool hook_load_or_fallback(void* self, int32_t config_id, void** payload,
                           void** message, const MethodInfo* method) {
    if (g_load_or_fallback == nullptr) return false;
    const bool ok =
        g_load_or_fallback(self, config_id, payload, message, method);
    LOGW("23.1.3-storetrace: read id=%d ok=%d bytes=%ld%s head='%s'", config_id,
         static_cast<int>(ok),
         payload == nullptr ? -1 : array_length(*payload), names(config_id),
         payload == nullptr ? ""
                            : array_preview(*payload, kPreviewBytes).c_str());
    return ok;
}

bool hook_load_core(void* self, int32_t config_id, void** payload,
                    void** message, const MethodInfo* method) {
    if (g_load_core == nullptr) return false;
    const bool ok = g_load_core(self, config_id, payload, message, method);
    LOGI("23.1.3-storetrace: core-read id=%d ok=%d bytes=%ld%s", config_id,
         static_cast<int>(ok),
         payload == nullptr ? -1 : array_length(*payload), names(config_id));
    return ok;
}

} // namespace detail

// No hook is required: this trace is diagnostic, and a refusal must never make
// the thin client report itself as broken.
inline bool install_hooks() {
    const bool save = hook::install(
        {detail::kNamespace, detail::kStoreClass, detail::kTrySave, 3},
        reinterpret_cast<void*>(&detail::hook_save),
        reinterpret_cast<void**>(&detail::g_save));
    const bool load = hook::install(
        {detail::kNamespace, detail::kStoreClass, detail::kTryLoadOrFallback,
         3},
        reinterpret_cast<void*>(&detail::hook_load_or_fallback),
        reinterpret_cast<void**>(&detail::g_load_or_fallback));
    const bool core = hook::install(
        {detail::kNamespace, detail::kStoreClass, detail::kTryLoadCore, 3},
        reinterpret_cast<void*>(&detail::hook_load_core),
        reinterpret_cast<void**>(&detail::g_load_core));
    LOGI("23.1.3-storetrace: config store trace armed on %d/3 entry point(s) "
         "(save, load-or-fallback, load-core); every ConfigId that moves "
         "through the store is named, with the size of its document",
         static_cast<int>(save) + static_cast<int>(load) +
             static_cast<int>(core));
    return save || load || core;
}

} // namespace config_store_trace_2313

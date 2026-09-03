#pragma once

#include <atomic>
#include <cstdint>
#include <cstring>
#include <string>


#include "hook.h"
#include "il2cpp.h"
#include "log.h"

// Observational trace for the full-slot parser and matching warning messages.
// The parser receives the original document, is called exactly once, and only a
// bounded preview is logged. UnityEngine.Debug.Log/LogError belong to the common
// debug_log trace; this file owns only the non-conflicting LogWarning surface.
namespace slot_config_trace_2313 {
namespace detail {

using MethodInfo = void;

// Static method: the boxed message, then MethodInfo*. There is no `self`.
using LogFn = void (*)(void*, const MethodInfo*);

constexpr const char* kUnityNamespace = "UnityEngine";
constexpr const char* kDebugClass = "Debug";

// dump2313.cs:1068864, :1068873 and :1068891, class Debug (TypeDefIndex
// 20302). All three take a single System.Object.
constexpr const char* kLogWarning = "LogWarning";

constexpr size_t kMaxMessageChars = 512u;

// dump2313.cs:284380-284381: namespace Progress, internal sealed class
// 东丝丂丄业丕且丙丑 (TypeDefIndex 6674). The parser is the method at
// dump2313.cs:284933, RVA 0x1B3F618, taking exactly one string.
constexpr const char* kProgressNamespace = "Progress";
// Spelled as UTF-8 bytes so this file's source encoding cannot change which
// type or method is resolved.
constexpr const char* kParserClass = u8"东丝丂丄业丕且丙丑";
constexpr const char* kParseFullSlotConfig = u8"七丈丆丈三丆丄丛丞";

constexpr size_t kDocumentPreview = 240u;

inline LogFn g_log_warning = nullptr;

inline std::atomic<uint32_t> g_hits{0u};

// Instance method: self, the document string, then MethodInfo*.
using ParseFn = void (*)(void*, void*, const MethodInfo*);

inline ParseFn g_parse = nullptr;
inline std::atomic<uint32_t> g_documents{0u};

// Debug.Log takes System.Object, so a non-string argument is normal and must
// never be handed to the string reader.
inline bool string_text(void* message, std::string& out) {
    if (message == nullptr) return false;
    void* klass = il2cpp::object_get_class(message);
    if (klass == nullptr) return false;
    const char* name = il2cpp::class_get_name(klass);
    if (name == nullptr || std::strcmp(name, "String") != 0) return false;
    out = il2cpp::to_utf8(message, kMaxMessageChars);
    return !out.empty();
}

// A boot prints thousands of unrelated lines; only the slot-configuration ones
// are worth a record.
inline bool belongs_to_slot_path(const std::string& text) {
    static const char* const kNeedles[] = {
        "ParseFullSlotConfig", "versionDict", "FullSlot", "slot", "Slot",
    };
    for (const char* needle : kNeedles) {
        if (text.find(needle) != std::string::npos) return true;
    }
    return false;
}

inline bool matched(void* message, std::string& out) {
    return string_text(message, out) && belongs_to_slot_path(out);
}

void hook_log_warning(void* message, const MethodInfo* method) {
    std::string text;
    if (matched(message, text)) {
        LOGW("23.1.3-slottrace: #%u Debug.LogWarning '%s'",
             g_hits.fetch_add(1u) + 1u, text.c_str());
    }
    if (g_log_warning != nullptr) g_log_warning(message, method);
}

// An empty or absent document is itself the answer: it would mean the client
// parses nothing at all, rather than parsing a document that lacks a version
// key. Both cases are recorded.
void hook_parse(void* self, void* document, const MethodInfo* method) {
    const uint32_t n = g_documents.fetch_add(1u) + 1u;
    const int32_t chars =
        document != nullptr && il2cpp::string_length != nullptr
            ? il2cpp::string_length(document)
            : -1;
    const std::string head = il2cpp::to_utf8(document, kDocumentPreview);
    LOGI("23.1.3-slottrace: #%u parse(chars=%d) head='%s'", n, chars,
         head.c_str());
    if (g_parse != nullptr) g_parse(self, document, method);
}

} // namespace detail

// No hook is required: this trace is diagnostic, and a refusal must never make
// the thin client report itself as broken.
inline bool install_hooks() {
    const bool warning = hook::install(
        {detail::kUnityNamespace, detail::kDebugClass, detail::kLogWarning, 1},
        reinterpret_cast<void*>(&detail::hook_log_warning),
        reinterpret_cast<void**>(&detail::g_log_warning), false);
    const bool parse = hook::install(
        {detail::kProgressNamespace, detail::kParserClass,
         detail::kParseFullSlotConfig, 1},
        reinterpret_cast<void*>(&detail::hook_parse),
        reinterpret_cast<void**>(&detail::g_parse), false);
    LOGI("23.1.3-slottrace: parser=%d warning=%d; bounded passive preview",
         parse ? 1 : 0, warning ? 1 : 0);
    return warning || parse;
}

} // namespace slot_config_trace_2313

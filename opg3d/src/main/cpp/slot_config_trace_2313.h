#pragma once

#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include <fcntl.h>
#include <unistd.h>

#include "hook.h"
#include "il2cpp.h"
#include "log.h"

// Names the managed method that reads the full-slot configuration document.
//
// Why this exists. config_ingest_trace_2313 established that BalanceController
// accepts exactly fourteen documents per boot, all of them Embedded balance
// documents (BalanceWeaponsCore, BalanceEconomic, BalancePets, ...), and that
// not one of them carries a version dictionary. CommandsWithFullSlots
// (ConfigId 7) never reaches that method at all, yet every boot still logs
//
//   ParseFullSlotConfig: versionDict not contains version 23.1.3
//
// exactly once. The document therefore has a different reader. That reader
// cannot be found by grepping the dump, because the names in the parser's error
// strings (commandsDict, dataDict, slotList, versionDict) are local variables
// and the method names themselves are obfuscated.
//
// How it is found. The native logger prints the return address of the frame
// that called it, so a hook that logs from its own body reports the RVA of its
// managed caller. Hooking UnityEngine.Debug.Log makes the game name the method
// that logged the failure, and tools/resolve_rva.py turns that RVA into
// `Type.Method (+0xNN)`.
//
// The log call has to sit directly in each hook body. The return address
// belongs to the enclosing function, so moving it into a shared helper would
// report this file instead of the managed caller.
//
// Nothing is altered: every message is passed to the stock logger unchanged,
// and only messages that belong to the slot path are printed.
//
// Second stage. The trace above named the reader on the first boot that carried
// it: Progress.东丝丂丄业丕且丙丑.七丈丆丈三丆丄丛丞 (dump2313.cs:284381 and :284933,
// RVA 0x1B3F618), declared `private void <parse>(string document)`. Progress is
// the namespace rather than an outer class, so the method resolves without the
// nested-type walk described in docs/IL2CPP_NESTED_TYPES.md. That parser is now
// hooked too, which turns the last guess in this investigation into evidence:
// the document the client actually parses is saved whole, so its real field
// names can be read off the device instead of inferred from error strings.
namespace slot_config_trace_2313 {
namespace detail {

using MethodInfo = void;

// Static method: the boxed message, then MethodInfo*. There is no `self`.
using LogFn = void (*)(void*, const MethodInfo*);

constexpr const char* kUnityNamespace = "UnityEngine";
constexpr const char* kDebugClass = "Debug";

// dump2313.cs:1068864, :1068873 and :1068891, class Debug (TypeDefIndex
// 20302). All three take a single System.Object.
constexpr const char* kLog = "Log";
constexpr const char* kLogError = "LogError";
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

constexpr size_t kMaxDocumentChars = 4u * 1024u * 1024u;
constexpr size_t kDocumentPreview = 240u;

inline LogFn g_log = nullptr;
inline LogFn g_log_error = nullptr;
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

void hook_log(void* message, const MethodInfo* method) {
    std::string text;
    if (matched(message, text)) {
        LOGW("23.1.3-slottrace: #%u Debug.Log '%s'",
             g_hits.fetch_add(1u) + 1u, text.c_str());
    }
    if (g_log != nullptr) g_log(message, method);
}

void hook_log_error(void* message, const MethodInfo* method) {
    std::string text;
    if (matched(message, text)) {
        LOGW("23.1.3-slottrace: #%u Debug.LogError '%s'",
             g_hits.fetch_add(1u) + 1u, text.c_str());
    }
    if (g_log_error != nullptr) g_log_error(message, method);
}

void hook_log_warning(void* message, const MethodInfo* method) {
    std::string text;
    if (matched(message, text)) {
        LOGW("23.1.3-slottrace: #%u Debug.LogWarning '%s'",
             g_hits.fetch_add(1u) + 1u, text.c_str());
    }
    if (g_log_warning != nullptr) g_log_warning(message, method);
}

// The document is written whole to the app's own external files directory:
// logcat truncates long lines, and truncation would destroy exactly the field
// names this trace exists to recover.
inline void dump_document(uint32_t ordinal, const std::string& text) {
    char path[192];
    std::snprintf(path, sizeof(path),
                  "/storage/emulated/0/Android/data/com.pixel.gun3d/files/"
                  "opg3d-slotdoc-%u.json",
                  ordinal);
    const int fd = ::open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        LOGW("23.1.3-slottrace: cannot write %s (errno=%d)", path, errno);
        return;
    }
    size_t written = 0u;
    while (written < text.size()) {
        const ssize_t chunk =
            ::write(fd, text.data() + written, text.size() - written);
        if (chunk <= 0) break;
        written += static_cast<size_t>(chunk);
    }
    ::close(fd);
    LOGI("23.1.3-slottrace: document saved %zu/%zu char(s) -> %s", written,
         text.size(), path);
}

// An empty or absent document is itself the answer: it would mean the client
// parses nothing at all, rather than parsing a document that lacks a version
// key. Both cases are recorded.
void hook_parse(void* self, void* document, const MethodInfo* method) {
    const uint32_t n = g_documents.fetch_add(1u) + 1u;
    const std::string text = il2cpp::to_utf8(document, kMaxDocumentChars);
    LOGW("23.1.3-slottrace: #%u parse(chars=%zu) head='%s'", n, text.size(),
         text.substr(0u, kDocumentPreview).c_str());
    if (!text.empty()) dump_document(n, text);
    if (g_parse != nullptr) g_parse(self, document, method);
}

} // namespace detail

// No hook is required: this trace is diagnostic, and a refusal must never make
// the thin client report itself as broken.
inline bool install_hooks() {
    const bool log = hook::install(
        {detail::kUnityNamespace, detail::kDebugClass, detail::kLog, 1},
        reinterpret_cast<void*>(&detail::hook_log),
        reinterpret_cast<void**>(&detail::g_log));
    const bool error = hook::install(
        {detail::kUnityNamespace, detail::kDebugClass, detail::kLogError, 1},
        reinterpret_cast<void*>(&detail::hook_log_error),
        reinterpret_cast<void**>(&detail::g_log_error));
    const bool warning = hook::install(
        {detail::kUnityNamespace, detail::kDebugClass, detail::kLogWarning, 1},
        reinterpret_cast<void*>(&detail::hook_log_warning),
        reinterpret_cast<void**>(&detail::g_log_warning));
    const bool parse = hook::install(
        {detail::kProgressNamespace, detail::kParserClass,
         detail::kParseFullSlotConfig, 1},
        reinterpret_cast<void*>(&detail::hook_parse),
        reinterpret_cast<void**>(&detail::g_parse));
    LOGI("23.1.3-slottrace: full-slot document parser hook %s "
         "(Progress.<parser>, 1 string argument); every document it receives "
         "is saved whole",
         parse ? "armed" : "refused");
    LOGI("23.1.3-slottrace: slot document reader trace armed on %d/3 "
         "UnityEngine.Debug entry point(s); the pc of every reported line is "
         "the RVA of the managed method that logged it",
         static_cast<int>(log) + static_cast<int>(error) +
             static_cast<int>(warning));
    return log || error || warning || parse;
}

} // namespace slot_config_trace_2313

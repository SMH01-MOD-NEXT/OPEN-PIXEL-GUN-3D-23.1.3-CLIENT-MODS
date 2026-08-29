#pragma once

#include <atomic>
#include <cerrno>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <string>

#include <fcntl.h>
#include <unistd.h>

#include "hook.h"
#include "il2cpp.h"
#include "log.h"

// Passive trace over the single point where BalanceController accepts a
// configuration document.
//
// dump2313.cs:8454, class BalanceController (TypeDefIndex 190, no namespace):
//
//   internal void <kAccept>(string name, byte[] payload,
//                           BalanceController.<source> source,
//                           ConfigId id)                       RVA 0x471CB40
//
// The third argument is the nested enum at dump2313.cs:7380, which is the
// reason this method is worth more than the parser itself:
//
//   Embedded = 0, Server = 1, Fallback = 2
//
// so every line says whether a document came out of the APK or off the wire.
//
// Why this trace exists. The boot logs
// `ParseFullSlotConfig: versionDict not contains version 23.1.3` once, for
// CommandsWithFullSlots (ConfigId 7). Four of that parser's five null checks
// pass, so a well-formed document does reach it and only the version key is
// missing. The names in the parser's error strings (commandsDict, dataDict,
// slotList, versionDict) are local variables, not document fields: every
// occurrence in global-metadata.dat belongs to an error message. The real field
// names therefore cannot be guessed, and the copy the client already accepts is
// the only ground truth left. Reading it here is what lets the backend author a
// document the client will parse.
//
// Cost control. The client accepts 153 documents per boot, so the per-call line
// is one compact record. The body is written to disk for ConfigId 7 alone,
// because logcat would truncate it and truncation destroys exactly the evidence
// being collected.
//
// Nothing is altered: stock code runs with the arguments it was called with.
namespace config_ingest_trace_2313 {
namespace detail {

using MethodInfo = void;

// Instance method: self, then the four managed arguments, then MethodInfo*.
// Both enums are passed by value as int32.
using AcceptFn = void (*)(void*, void*, void*, int32_t, int32_t,
                          const MethodInfo*);

constexpr const char* kBalanceClass = "BalanceController";
// Spelled as UTF-8 bytes so this file's source encoding cannot change which
// method is resolved.
constexpr const char* kAccept = u8"世丄丅丏丌专上世丄";

// ConfigId.CommandsWithFullSlots, from the enum at dump2313.cs:449054.
constexpr int32_t kCommandsWithFullSlots = 7;

// Three further points on the same controller, added after run 26 proved the
// subscribe answer is not the ingestion channel.
//
//   dump2313.cs:8410  private void <kRequest>(ConfigId)                 0x471C074
//   dump2313.cs:8413  private static bool <kGate>(ConfigId)             0x471CAC0
//   dump2313.cs:8465  private void <kAnswer>(Dictionary<string,object>) 0x4724604
//
// Run 26 answered every one of the 153 polled names, 86 of which the client
// holds no copy of, and served one document body. The client still accepted
// only the 14 embedded documents, still reported the clan config empty and
// still logged `Start wait config response`. So the reply either never reaches
// this controller or is dropped inside it, and guessing which is what these
// hooks end: the request hook says what the controller asks for, the gate says
// what it considers hosted, and the answer hook names the keys it actually
// reads out of a socket answer.
constexpr const char* kRequest = u8"\u4e1b\u4e10\u4e04\u4e00\u4e1e\u4e0d\u4e14\u4e02\u4e0b";
constexpr const char* kGate = u8"\u4e01\u4e01\u4e01\u4e0f\u4e1f\u4e04\u4e18\u4e04\u4e13";
constexpr const char* kAnswer = u8"\u4e0a\u4e05\u4e1e\u4e1c\u4e1e\u4e12\u4e00\u4e06\u4e15";

using RequestFn = void (*)(void*, int32_t, const MethodInfo*);
using GateFn = bool (*)(int32_t, const MethodInfo*);
using AnswerFn = void (*)(void*, void*, const MethodInfo*);

// Dictionary<string, object> layout for this build: 16-byte managed header,
// then buckets, entries, count. Entry is {int hashCode; int next; TKey key;
// TValue value}, so 24 bytes with both slots held by reference. The walk is
// read-only and never calls managed code, because a managed call from inside a
// hook is the one thing that has already crashed this client.
constexpr size_t kDictEntries = 0x18u;
constexpr size_t kDictCount = 0x20u;
constexpr size_t kEntrySize = 24u;
constexpr size_t kEntryKey = 8u;
constexpr size_t kEntryValue = 16u;
constexpr size_t kDictCountCoreLib = 0x28u;
constexpr int32_t kMaxDictEntries = 1024;

// Il2CppArray layout for this build: the managed object header is 16 bytes,
// which puts bounds at 0x10, max_length at 0x18 and the first element at 0x20.
// Same constants sio_codec_trace_2313.h derived for the sealed attachments.
constexpr size_t kArrayLength = 0x18u;
constexpr size_t kArrayData = 0x20u;
constexpr int64_t kMaxArrayBytes = 8 * 1024 * 1024;

constexpr size_t kHexPreview = 96u;
constexpr size_t kTextPreview = 240u;

inline AcceptFn g_accept = nullptr;
inline RequestFn g_request = nullptr;
inline GateFn g_gate = nullptr;
inline AnswerFn g_answer = nullptr;
inline std::atomic<uint32_t> g_requests{0u};
inline std::atomic<uint32_t> g_answers{0u};
inline std::atomic<uint32_t> g_gates{0u};
inline std::atomic<uint32_t> g_calls{0u};
inline std::atomic<uint32_t> g_dumps{0u};

template <typename T>
inline T read_at(void* base, size_t offset, T fallback) {
    if (base == nullptr) return fallback;
    return *reinterpret_cast<const T*>(
        reinterpret_cast<const uint8_t*>(base) + offset);
}

// -1 means "this pointer is not the array we assumed", never a real length.
inline int64_t array_length(void* array) {
    if (array == nullptr) return -1;
    const uint64_t raw = read_at<uint64_t>(array, kArrayLength, 0u);
    if (raw > static_cast<uint64_t>(kMaxArrayBytes)) return -1;
    return static_cast<int64_t>(raw);
}

inline const uint8_t* array_data(void* array) {
    if (array == nullptr) return nullptr;
    return reinterpret_cast<const uint8_t*>(array) + kArrayData;
}

inline std::string bytes_to_hex(const uint8_t* data, size_t length,
                                size_t cap) {
    static const char kDigits[] = "0123456789abcdef";
    std::string out;
    if (data == nullptr) return out;
    const size_t limit = length < cap ? length : cap;
    out.reserve(limit * 2u + 2u);
    for (size_t i = 0; i < limit; ++i) {
        out.push_back(kDigits[(data[i] >> 4) & 0x0F]);
        out.push_back(kDigits[data[i] & 0x0F]);
    }
    if (limit < length) out.append("..");
    return out;
}

// Printable-ASCII rendering, so a JSON document is readable straight out of
// logcat while a MessagePack one is still recognizable by its structure.
inline std::string bytes_to_text(const uint8_t* data, size_t length,
                                 size_t cap) {
    std::string out;
    if (data == nullptr) return out;
    const size_t limit = length < cap ? length : cap;
    out.reserve(limit + 2u);
    for (size_t i = 0; i < limit; ++i) {
        const uint8_t c = data[i];
        out.push_back(c >= 0x20 && c < 0x7F ? static_cast<char>(c) : '.');
    }
    if (limit < length) out.append("..");
    return out;
}

inline const char* source_name(int32_t source) {
    switch (source) {
        case 0: return "Embedded";
        case 1: return "Server";
        case 2: return "Fallback";
        default: return "?";
    }
}

// Writes the body into the app's own external files directory, next to the
// client's binary config cache, so it can be pulled with adb and analysed
// whole instead of through a truncated log line.
inline void dump_to_file(int32_t config_id, uint32_t ordinal,
                         const uint8_t* data, size_t length) {
    char path[192];
    std::snprintf(path, sizeof(path),
                  "/storage/emulated/0/Android/data/com.pixel.gun3d/files/"
                  "opg3d-config-%d-%u.bin",
                  config_id, ordinal);
    const int fd = ::open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        LOGW("23.1.3-cfgtrace: cannot write %s (errno=%d)", path, errno);
        return;
    }
    size_t written = 0u;
    while (written < length) {
        const ssize_t chunk = ::write(fd, data + written, length - written);
        if (chunk <= 0) break;
        written += static_cast<size_t>(chunk);
    }
    ::close(fd);
    LOGW("23.1.3-cfgtrace: body saved %zu/%zu byte(s) -> %s", written, length,
         path);
}

// Saves the document text under its ConfigId so the exact shape the client
// accepts can be read off the device instead of guessed from error strings.
inline void dump_text_to_file(int32_t config_id, const std::string& text) {
    char path[192];
    std::snprintf(path, sizeof(path),
                  "/storage/emulated/0/Android/data/com.pixel.gun3d/files/"
                  "opg3d-doc-%d.json",
                  config_id);
    const int fd = ::open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        LOGW("23.1.3-cfgtrace: cannot write %s (errno=%d)", path, errno);
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
    LOGI("23.1.3-cfgtrace: document saved %zu/%zu char(s) -> %s", written,
         text.size(), path);
}

void hook_accept(void* self, void* document, void* payload, int32_t source,
                 int32_t config_id, const MethodInfo* method) {
    const uint32_t n = detail::g_calls.fetch_add(1u) + 1u;
    const std::string text = il2cpp::to_utf8(document, 4u * 1024u * 1024u);
    const int64_t length = array_length(payload);
    const uint8_t* data = array_data(payload);

    LOGI("23.1.3-cfgtrace: #%u accept id=%d source=%s chars=%zu bytes=%" PRId64
         " head='%s'",
         n, config_id, source_name(source), text.size(), length,
         text.substr(0u, kTextPreview).c_str());

    if (!text.empty()) {
        dump_text_to_file(config_id, text);
    }
    if (length > 0 && data != nullptr) {
        const size_t size = static_cast<size_t>(length);
        LOGW("23.1.3-cfgtrace: id=%d sealed body hex=%s", config_id,
             bytes_to_hex(data, size, kHexPreview).c_str());
        LOGW("23.1.3-cfgtrace: id=%d sealed body text='%s'", config_id,
             bytes_to_text(data, size, kTextPreview).c_str());
        dump_to_file(config_id, detail::g_dumps.fetch_add(1u) + 1u, data, size);
    }

    if (g_accept != nullptr) {
        g_accept(self, document, payload, source, config_id, method);
    }
}

inline const char* class_name_of(void* object) {
    if (object == nullptr) return "null";
    if (il2cpp::object_get_class == nullptr ||
        il2cpp::class_get_name == nullptr) {
        return "?";
    }
    void* klass = il2cpp::object_get_class(object);
    if (klass == nullptr) return "?";
    const char* name = il2cpp::class_get_name(klass);
    return name != nullptr ? name : "?";
}

// Names the keys of a managed Dictionary<string, object> by reading memory
// only. Refuses to walk anything whose header does not look like the expected
// layout, so a wrong guess prints one warning instead of taking the game down.
inline void describe_dict(void* dict, int depth, int32_t limit,
                         const char* label) {
    if (dict == nullptr) {
        LOGI("23.1.3-cfganswer: %s dict=null", label);
        return;
    }
    // Two BCL layouts exist for Dictionary<TKey, TValue>. The framework one
    // puts count directly after entries; the CoreLib one inserts a 64-bit
    // _fastModMultiplier first, which moves count to 0x28. Run 27 read 1197 at
    // the framework offset, so neither offset is trusted: the walk is driven by
    // the entries array's own length, and both candidates are logged so the
    // real layout is named by the next run instead of guessed again.
    void* entries = read_at<void*>(dict, kDictEntries, nullptr);
    const int32_t count_a = read_at<int32_t>(dict, kDictCount, -1);
    const int32_t count_b = read_at<int32_t>(dict, kDictCountCoreLib, -1);
    const int64_t capacity = array_length(entries);
    LOGI("23.1.3-cfganswer: %s dict=%p class=%s capacity=%" PRId64
         " count@0x20=%d count@0x28=%d",
         label, dict, class_name_of(dict), capacity, count_a, count_b);
    if (entries == nullptr || capacity <= 0 ||
        capacity > static_cast<int64_t>(kMaxDictEntries)) {
        LOGW("23.1.3-cfganswer: %s entries=%p capacity=%" PRId64
             " not walkable",
             label, entries, capacity);
        return;
    }
    const int32_t count = static_cast<int32_t>(capacity);
    const uint8_t* base =
        reinterpret_cast<const uint8_t*>(entries) + kArrayData;
    for (int32_t i = 0; i < count; ++i) {
        if (i >= limit) {
            LOGI("23.1.3-cfganswer: %s [+%d more entries not printed]", label,
                 count - i);
            break;
        }
        const uint8_t* entry = base + static_cast<size_t>(i) * kEntrySize;
        void* key = *reinterpret_cast<void* const*>(entry + kEntryKey);
        void* value = *reinterpret_cast<void* const*>(entry + kEntryValue);
        if (key == nullptr) continue;
        const std::string name = il2cpp::to_utf8(key, 64u);
        const char* vclass = class_name_of(value);
        std::string preview;
        if (value != nullptr && std::string(vclass) == "String") {
            preview = il2cpp::to_utf8(value, 96u);
        }
        LOGI("23.1.3-cfganswer: %s [%d] key='%s' value=%s%s%s", label, i,
             name.c_str(), vclass, preview.empty() ? "" : " text=",
             preview.c_str());
        if (depth > 0 && value != nullptr &&
            std::string(vclass).rfind("Dictionary", 0u) == 0u) {
            char nested[128];
            std::snprintf(nested, sizeof(nested), "%s.%s", label,
                          name.c_str());
            describe_dict(value, depth - 1, limit, nested);
        }
    }
}

void hook_request(void* self, int32_t config_id, const MethodInfo* method) {
    const uint32_t n = g_requests.fetch_add(1u) + 1u;
    LOGI("23.1.3-cfgreq: #%u controller requests id=%d self=%p", n, config_id,
         self);
    if (g_request != nullptr) {
        g_request(self, config_id, method);
    }
}

bool hook_gate(int32_t config_id, const MethodInfo* method) {
    const uint32_t n = g_gates.fetch_add(1u) + 1u;
    bool result = false;
    if (g_gate != nullptr) {
        result = g_gate(config_id, method);
    }
    LOGI("23.1.3-cfggate: #%u id=%d verdict=%d", n, config_id, result ? 1 : 0);
    return result;
}

void hook_answer(void* self, void* dict, const MethodInfo* method) {
    const uint32_t n = g_answers.fetch_add(1u) + 1u;
    LOGI("23.1.3-cfganswer: #%u socket answer reaches BalanceController self=%p",
         n, self);
    describe_dict(dict, 1, 24, "answer");
    if (g_answer != nullptr) {
        g_answer(self, dict, method);
    }
}

} // namespace detail

inline bool install_hooks() {
    const bool armed = hook::install(
        {"", detail::kBalanceClass, detail::kAccept, 4},
        reinterpret_cast<void*>(&detail::hook_accept),
        reinterpret_cast<void**>(&detail::g_accept), true);
    LOGI("23.1.3-cfgtrace: config ingestion trace %s "
         "(BalanceController document accept, 4 args); passive passthrough, "
         "ConfigId 7 body saved to the app files directory",
         armed ? "armed" : "refused");
    const bool request_armed = hook::install(
        {"", detail::kBalanceClass, detail::kRequest, 1},
        reinterpret_cast<void*>(&detail::hook_request),
        reinterpret_cast<void**>(&detail::g_request), false);
    const bool gate_armed = hook::install(
        {"", detail::kBalanceClass, detail::kGate, 1},
        reinterpret_cast<void*>(&detail::hook_gate),
        reinterpret_cast<void**>(&detail::g_gate), false);
    const bool answer_armed = hook::install(
        {"", detail::kBalanceClass, detail::kAnswer, 1},
        reinterpret_cast<void*>(&detail::hook_answer),
        reinterpret_cast<void**>(&detail::g_answer), false);
    LOGI("23.1.3-cfgtrace: config request trace %s, hosted-config gate %s, "
         "socket answer trace %s",
         request_armed ? "armed" : "refused",
         gate_armed ? "armed" : "refused",
         answer_armed ? "armed" : "refused");
    return armed;
}

} // namespace config_ingest_trace_2313

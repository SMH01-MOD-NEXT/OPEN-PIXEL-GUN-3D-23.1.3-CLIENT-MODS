#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>

#include "hook.h"
#include "il2cpp.h"
#include "log.h"

// Config system ingestion trace (23.1.3).
//
// Run 29 let the client name its own waiter: the managed caller of the
// `Start wait config response` log line is
// AuthSceneController.<iterator>.MoveNext() at 0x3DC771C (dump2313.cs:91539),
// and the loop it then enters reads the config system singleton (0x2497BA0)
// and tests one byte at instance offset 0x2A every frame. That byte is the
// backing field of the config system's `ready` property (dump2313.cs:449523,
// TypeDefIndex 11094).
//
// Only two instructions in the whole class write that byte: the property setter
// itself (0x24A0534, which the AOT compiler inlined everywhere, so a call-site
// scan finds nothing) and one store at 0x24A2F58, which sits inside
//
//   private void <kParse>(Dictionary<string, object>,
//                         Dictionary<object, object>,
//                         List<byte[]>)            0x24A1E1C
//
// That signature is the whole answer to the question runs 23-28 kept missing:
// the config bodies are expected as socket.io *binary attachments*, not as text
// inside the JSON answer. The class also owns the "h", "c" and "v" key
// constants (dump2313.cs:449525-449527), so the subscribe answer really is the
// delivery channel -- but each entry has to reference an attachment instead of
// carrying a document body as a string.
//
// These four hooks read the expected per-entry shape out of the live process
// instead of guessing it a fifth time:
//
//   kParse  the answer, the requested set and the attachment list, plus the
//           ready flag immediately after the call
//   kToInt  the object->int conversion the parser runs per entry, including the
//           class of what it was handed and the text when that is a string
//   kApply  the per-document apply path, id and body length
//   kStore  the binary config storage, which is what turns bytes into an
//           accepted document (class dump2313.cs:448789)
//
// Nothing is altered: every proxy calls the original with its own arguments.
namespace config_system_trace_2313 {
namespace detail {

using MethodInfo = void;

// Spelled as escapes so this file's source encoding cannot change which class
// or method is resolved. Two runs were already lost to a mistyped code point.
// Namespace, not guessed: dump2313.cs carries `// Namespace: PGCompany` above
// both classes. Run 30 armed 0/4 with an empty namespace, which is the only
// thing that was wrong -- the class and method code points below match the dump
// character for character.
constexpr const char* kNamespace = "PGCompany";

constexpr const char* kConfigSystem =
    u8"\u4e1c\u4e0b\u4e18\u4e0a\u4e17\u4e1e\u4e05\u4e06\u4e0c";
constexpr const char* kStorage =
    u8"\u4e05\u4e1d\u4e1a\u4e03\u4e09\u4e08\u4e1d\u4e11\u4e0f";
constexpr const char* kParse =
    u8"\u4e13\u4e04\u4e14\u4e01\u4e05\u4e0e\u4e04\u4e12\u4e17";
constexpr const char* kToInt =
    u8"\u4e17\u4e00\u4e11\u4e01\u4e0f\u4e00\u4e0b\u4e01\u4e14";
constexpr const char* kApply =
    u8"\u4e17\u4e03\u4e13\u4e07\u4e10\u4e17\u4e11\u4e0f\u4e19";
constexpr const char* kStore =
    u8"\u4e0e\u4e07\u4e1d\u4e17\u4e01\u4e0d\u4e17\u4e00\u4e17";

// Request side, added after run 31. The parser armed 4/4 and was never called,
// and `tools/find_callers.py` reports 0 call sites for it: it is invoked through
// a subscriber delegate, so the answer either never arrives at the config
// system or the request that would register that subscriber never happens. The
// log agrees with the second reading -- after the wait line the client emits
// nothing but `get_inapps`. These five points say how far the request sequence
// gets, all names taken from dump2313.cs by RVA rather than retyped:
//
//   0x24A1740  private List<List<ConfigId>> <kGroups>()
//   0x24A1CC8  private IEnumerator <kRequestCo>(List<List<ConfigId>>)
//              its state machine is the nested <kIterator>, per-group timeout
//   0x249FEE0  internal static Dictionary<string, ConfigId> <kNameMap>()
//   0x249EFE0  private Dictionary<string, object> <kEmbedded>()
constexpr const char* kGroups =
    u8"\u4e14\u4e01\u4e18\u4e05\u4e04\u4e05\u4e15\u4e05\u4e09";
constexpr const char* kRequestCo =
    u8"\u4e1f\u4e1c\u4e06\u4e07\u4e11\u4e03\u4e13\u4e05\u4e07";
constexpr const char* kNameMap =
    u8"\u4e0e\u4e00\u4e0e\u4e08\u4e00\u4e05\u4e04\u4e13\u4e0f";
constexpr const char* kEmbedded =
    u8"\u4e04\u4e1d\u4e01\u4e08\u4e06\u4e02\u4e14\u4e1b\u4e10";
// Nested state machine of the request coroutine. il2cpp.h resolves a composite
// name by splitting at the last '.', so the declaring type is looked up first
// and the nested type matched by its short metadata name.
constexpr const char* kIterator =
    u8"\u4e1c\u4e0b\u4e18\u4e0a\u4e17\u4e1e\u4e05\u4e06\u4e0c."
    u8"\u4e0e\u4e1e\u4e09\u4e02\u4e06\u4e08\u4e15\u4e0e\u4e1e";

// Iterator layout: <>1__state at 0x10, <>2__current at 0x18.
constexpr size_t kIterState = 0x10u;
constexpr size_t kIterCurrent = 0x18u;

// Instance layout of the config system, read from the dump.
constexpr size_t kReadyFlag = 0x2Au;

// Dictionary count: this build's CoreLib inserts a 64-bit _fastModMultiplier,
// which moved count from 0x20 to 0x28 (run 28 measured both). Log both rather
// than trust either.
constexpr size_t kDictCountFramework = 0x20u;
constexpr size_t kDictCountCoreLib = 0x28u;

// List<T>: managed header, _items at 0x10, _size at 0x18.
constexpr size_t kListItems = 0x10u;
constexpr size_t kListSize = 0x18u;

// Il2CppArray: max_length at 0x18, first element at 0x20.
constexpr size_t kArrayLength = 0x18u;

constexpr size_t kTextPreview = 160u;
constexpr uint32_t kVerboseCalls = 24u;

using ParseFn = void (*)(void*, void*, void*, void*, const MethodInfo*);
using ToIntFn = bool (*)(void*, int32_t*, void**, const MethodInfo*);
using ApplyFn = void (*)(void*, int32_t, void*, const MethodInfo*);
using StoreFn = bool (*)(void*, int32_t, void*, void**, const MethodInfo*);

inline ParseFn g_parse = nullptr;
inline ToIntFn g_to_int = nullptr;
inline ApplyFn g_apply = nullptr;
inline StoreFn g_store = nullptr;

inline std::atomic<uint32_t> g_parses{0u};
inline std::atomic<uint32_t> g_to_ints{0u};
inline std::atomic<uint32_t> g_applies{0u};
inline std::atomic<uint32_t> g_stores{0u};

inline int32_t read_i32(const void* base, size_t offset) {
    if (base == nullptr) return -1;
    return *reinterpret_cast<const int32_t*>(
        static_cast<const char*>(base) + offset);
}

inline void* read_ptr(const void* base, size_t offset) {
    if (base == nullptr) return nullptr;
    return *reinterpret_cast<void* const*>(
        static_cast<const char*>(base) + offset);
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

inline std::string text_of(void* object) {
    if (object == nullptr) return std::string();
    return il2cpp::to_utf8(object, kTextPreview);
}

inline void hook_parse(void* self, void* answer, void* requested,
                       void* attachments, const MethodInfo* method) {
    const uint32_t call = detail::g_parses.fetch_add(1u) + 1u;
    void* items = read_ptr(attachments, kListItems);
    LOGI("23.1.3-cfgsys: #%u parse answer=%p count@0x20=%d count@0x28=%d "
         "requested=%p count@0x20=%d count@0x28=%d attachments=%p size=%d "
         "items=%p itemsLen=%d",
         call, answer, read_i32(answer, kDictCountFramework),
         read_i32(answer, kDictCountCoreLib), requested,
         read_i32(requested, kDictCountFramework),
         read_i32(requested, kDictCountCoreLib), attachments,
         read_i32(attachments, kListSize), items,
         read_i32(items, kArrayLength));

    if (g_parse != nullptr) {
        g_parse(self, answer, requested, attachments, method);
    }

    const int32_t ready =
        self != nullptr
            ? static_cast<int32_t>(*reinterpret_cast<const uint8_t*>(
                  static_cast<const char*>(self) + kReadyFlag))
            : -1;
    LOGI("23.1.3-cfgsys: #%u parse returned, ready flag now %d", call, ready);
}

inline bool hook_to_int(void* value, int32_t* out_value, void** out_error,
                        const MethodInfo* method) {
    const uint32_t call = detail::g_to_ints.fetch_add(1u) + 1u;
    const char* klass = class_name_of(value);
    std::string text;
    if (klass != nullptr && std::string(klass) == "String") {
        text = text_of(value);
    }

    bool ok = false;
    if (g_to_int != nullptr) {
        ok = g_to_int(value, out_value, out_error, method);
    }

    const int32_t produced = out_value != nullptr ? *out_value : -1;
    if (call <= kVerboseCalls || !ok) {
        LOGI("23.1.3-cfgsys: #%u toInt arg=%p class=%s ok=%d value=%d "
             "text='%s'",
             call, value, klass, ok ? 1 : 0, produced, text.c_str());
    }
    return ok;
}

inline void hook_apply(void* self, int32_t id, void* body,
                       const MethodInfo* method) {
    const uint32_t call = detail::g_applies.fetch_add(1u) + 1u;
    const int32_t chars =
        (body != nullptr && il2cpp::string_length != nullptr)
            ? il2cpp::string_length(body)
            : -1;
    const std::string head = text_of(body);
    LOGI("23.1.3-cfgsys: #%u apply id=%d chars=%d head='%s'", call, id, chars,
         head.c_str());

    if (g_apply != nullptr) {
        g_apply(self, id, body, method);
    }
}

inline bool hook_store(void* self, int32_t id, void* payload, void** out_error,
                       const MethodInfo* method) {
    const uint32_t call = detail::g_stores.fetch_add(1u) + 1u;
    const int32_t bytes = read_i32(payload, kArrayLength);

    bool ok = false;
    if (g_store != nullptr) {
        ok = g_store(self, id, payload, out_error, method);
    }

    std::string error;
    if (out_error != nullptr) {
        error = text_of(*out_error);
    }
    LOGI("23.1.3-cfgsys: #%u store id=%d bytes=%d ok=%d error='%s'", call, id,
         bytes, ok ? 1 : 0, error.c_str());
    return ok;
}

using GroupsFn = void* (*)(void*, const MethodInfo*);
using RequestCoFn = void* (*)(void*, void*, const MethodInfo*);
using NameMapFn = void* (*)(const MethodInfo*);
using EmbeddedFn = void* (*)(void*, const MethodInfo*);
using MoveNextFn = bool (*)(void*, const MethodInfo*);

inline GroupsFn g_groups = nullptr;
inline RequestCoFn g_request_co = nullptr;
inline NameMapFn g_name_map = nullptr;
inline EmbeddedFn g_embedded = nullptr;
inline MoveNextFn g_move_next = nullptr;

inline std::atomic<uint32_t> g_move_nexts{0u};

inline void* hook_groups(void* self, const MethodInfo* method) {
    void* result = g_groups != nullptr ? g_groups(self, method) : nullptr;
    LOGI("23.1.3-cfgsys: groups built list=%p size=%d", result,
         read_i32(result, kListSize));
    return result;
}

inline void* hook_request_co(void* self, void* groups,
                             const MethodInfo* method) {
    void* result =
        g_request_co != nullptr ? g_request_co(self, groups, method) : nullptr;
    LOGI("23.1.3-cfgsys: request coroutine created groups=%p size=%d "
         "enumerator=%p",
         groups, read_i32(groups, kListSize), result);
    return result;
}

inline void* hook_name_map(const MethodInfo* method) {
    void* result = g_name_map != nullptr ? g_name_map(method) : nullptr;
    LOGI("23.1.3-cfgsys: name map=%p count@0x20=%d count@0x28=%d", result,
         read_i32(result, kDictCountFramework),
         read_i32(result, kDictCountCoreLib));
    return result;
}

inline void* hook_embedded(void* self, const MethodInfo* method) {
    void* result = g_embedded != nullptr ? g_embedded(self, method) : nullptr;
    LOGI("23.1.3-cfgsys: embedded set=%p count@0x20=%d count@0x28=%d", result,
         read_i32(result, kDictCountFramework),
         read_i32(result, kDictCountCoreLib));
    return result;
}

// Runs once per frame while the request sequence is alive, so the record is
// capped: every step for the first 40, then one in 100. A silent hook here
// would be worse than a loud one -- the whole question is whether this state
// machine advances at all.
inline bool hook_move_next(void* self, const MethodInfo* method) {
    const uint32_t call = g_move_nexts.fetch_add(1u) + 1u;
    const int32_t before = read_i32(self, kIterState);
    const bool more = g_move_next != nullptr ? g_move_next(self, method) : false;
    const int32_t after = read_i32(self, kIterState);
    if (call <= 40u || call % 100u == 0u || before != after) {
        void* current = read_ptr(self, kIterCurrent);
        LOGI("23.1.3-cfgsys: request step #%u state %d -> %d more=%d "
             "current=%p class=%s",
             call, before, after, more ? 1 : 0, current,
             class_name_of(current));
    }
    return more;
}

} // namespace detail

// Installs the four trace points. None is required: a build where a name no
// longer resolves keeps every other component armed, and the verdict line says
// exactly how many went in so a run is never mistaken for a negative result.
inline bool install_hooks() {
    int armed = 0;

    armed += hook::install({detail::kNamespace, detail::kConfigSystem, detail::kParse, 3},
                           reinterpret_cast<void*>(&detail::hook_parse),
                           reinterpret_cast<void**>(&detail::g_parse), false)
                 ? 1
                 : 0;
    armed += hook::install({detail::kNamespace, detail::kConfigSystem, detail::kToInt, 3},
                           reinterpret_cast<void*>(&detail::hook_to_int),
                           reinterpret_cast<void**>(&detail::g_to_int), false)
                 ? 1
                 : 0;
    armed += hook::install({detail::kNamespace, detail::kConfigSystem, detail::kApply, 2},
                           reinterpret_cast<void*>(&detail::hook_apply),
                           reinterpret_cast<void**>(&detail::g_apply), false)
                 ? 1
                 : 0;
    armed += hook::install({detail::kNamespace, detail::kStorage, detail::kStore, 3},
                           reinterpret_cast<void*>(&detail::hook_store),
                           reinterpret_cast<void**>(&detail::g_store), false)
                 ? 1
                 : 0;

    armed += hook::install({detail::kNamespace, detail::kConfigSystem, detail::kGroups, 0},
                           reinterpret_cast<void*>(&detail::hook_groups),
                           reinterpret_cast<void**>(&detail::g_groups), false)
                 ? 1
                 : 0;
    armed += hook::install({detail::kNamespace, detail::kConfigSystem, detail::kRequestCo, 1},
                           reinterpret_cast<void*>(&detail::hook_request_co),
                           reinterpret_cast<void**>(&detail::g_request_co), false)
                 ? 1
                 : 0;
    armed += hook::install({detail::kNamespace, detail::kConfigSystem, detail::kNameMap, 0},
                           reinterpret_cast<void*>(&detail::hook_name_map),
                           reinterpret_cast<void**>(&detail::g_name_map), false)
                 ? 1
                 : 0;
    armed += hook::install({detail::kNamespace, detail::kConfigSystem, detail::kEmbedded, 0},
                           reinterpret_cast<void*>(&detail::hook_embedded),
                           reinterpret_cast<void**>(&detail::g_embedded), false)
                 ? 1
                 : 0;
    armed += hook::install({detail::kNamespace, detail::kIterator, "MoveNext", 0},
                           reinterpret_cast<void*>(&detail::hook_move_next),
                           reinterpret_cast<void**>(&detail::g_move_next), false)
                 ? 1
                 : 0;

    LOGI("23.1.3-cfgsys: config system trace armed on %d/9 entry point(s) in "
         "namespace %s; "
         "parse/toInt/apply/store answer the delivery question, "
         "groups/request/namemap/embedded/step answer the request question, "
         "apply=document text, store=binary config storage",
         armed, detail::kNamespace);
    return armed > 0;
}

} // namespace config_system_trace_2313

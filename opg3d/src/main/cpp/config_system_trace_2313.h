#pragma once

#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>
#include <unordered_set>

#include <fcntl.h>
#include <unistd.h>

#include "elf_sym.h"
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

// Read side, added after run 63. The five configs the boot stops on are
// stored successfully (`store id=... ok=1`) and their consumers still report a
// null config seconds later, so the question is no longer delivery but what
// the per-id read hands back. Both accessors are declared on the config system
// class itself (TypeDefIndex 11094), not on the storage class that owns the
// store method, which is why run 64 armed 9/11 with the wrong owner:
//
//   0x24981B0  internal string <kGetText>(ConfigId)
//   0x24A0F34  internal byte[] <kGetBytes>(ConfigId)
constexpr const char* kGetText =
    u8"\u4e12\u4e0d\u4e0f\u4e00\u4e02\u4e08\u4e19\u4e1c\u4e1f";
constexpr const char* kGetBytes =
    u8"\u4e12\u4e1f\u4e0f\u4e0d\u4e1d\u4e0e\u4e15\u4e14\u4e1e";

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

// A manifest does not fit in a log line, so the whole document is written to a
// file once per id. Temporary: it exists to recover the exact documents this
// server has to host, and comes out once they are hosted.
constexpr size_t kDocumentDumpLimit = 1u << 20;
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

using GetTextFn = void* (*)(void*, int32_t, const MethodInfo*);
using GetBytesFn = void* (*)(void*, int32_t, const MethodInfo*);

inline GetTextFn g_get_text = nullptr;
inline GetBytesFn g_get_bytes = nullptr;

inline std::atomic<uint32_t> g_get_text_calls{0u};
inline std::atomic<uint32_t> g_get_bytes_calls{0u};

// All six calls below compile to the same shared generic body. Hooking that
// body and rejecting every other return address reveals the literal key passed
// by the parser and reader without changing either dictionary.
constexpr uintptr_t kTryGetValueRva = 0x468C2E0u;
using TryGetValueFn = bool (*)(void*, void*, void**, const MethodInfo*);
inline TryGetValueFn g_try_get_value = nullptr;
inline uintptr_t g_il2cpp_base = 0;

inline bool config_dictionary_call(uintptr_t rva) {
    switch (rva) {
        case 0x24A2934u: // parser: outer lookup, first path
        case 0x24A2980u: // parser: inner lookup, first path
        case 0x24A2AA4u: // parser: outer lookup, saved set
        case 0x24A2AF0u: // parser: inner fingerprint lookup
        case 0x24A0EC4u: // reader: outer ConfigId lookup
        case 0x24A0F08u: // reader: inner text lookup
        // refund_get callback nested dictionary lookups. These are passive
        // observations of the live reply/config parser, not behavior changes.
        case 0x3D04B98u:
        case 0x3D04BF8u:
        case 0x3D04C88u:
        case 0x3D04D00u:
        case 0x3D04F34u:
        case 0x3D04FA8u:
        case 0x3D0501Cu:
        case 0x3D05074u:
        case 0x3D050E8u:
        case 0x3D052B4u:
            return true;
        default:
            return false;
    }
}

// The get_inapps subscriber at 0x294E108 delegates all reply parsing to
// 0x294DF50. That parser asks one shared integer-reader for exactly three
// fields. Trace only those return addresses so the backend reply schema comes
// from the running client rather than from guessed product/in-app names.
constexpr uintptr_t kGetInappsIntLookupRva = 0x468BAA0u;
using GetInappsIntLookupFn = void (*)(void*, void*, int32_t*,
                                     const MethodInfo*);
inline GetInappsIntLookupFn g_get_inapps_int_lookup = nullptr;

inline bool get_inapps_lookup_call(uintptr_t rva) {
    switch (rva) {
        case 0x294E02Cu:
        case 0x294E040u:
        case 0x294E054u:
        case 0x33F12F8u:  // get_progress callback
            return true;
        default:
            return false;
    }
}

// FriendsController installs 0x1D8F60C as the one-shot callback for
// get_player_version. It tests one reply key through the shared
// Dictionary<string, object>.ContainsKey body at 0x3B5F914 and returns early
// when that key is absent. Run71 proved the generic acknowledgement takes that
// early return while the final authorization initializer otherwise completes.
// Trace the exact return site so the backend member name comes from the stock
// client rather than another response-shape guess.
constexpr uintptr_t kPlayerVersionContainsKeyRva = 0x3B5F914u;
constexpr uintptr_t kPlayerVersionGetItemRva = 0x3B5F68Cu;
constexpr uintptr_t kPlayerVersionContainsKeyReturnRva = 0x1D8F740u;
constexpr uintptr_t kPlayerVersionGetItemReturnRva = 0x1D8F76Cu;
using ReplyContainsKeyFn = bool (*)(void*, void*, const MethodInfo*);
using ReplyGetItemFn = void* (*)(void*, void*, const MethodInfo*);
inline ReplyContainsKeyFn g_reply_contains_key = nullptr;
inline ReplyGetItemFn g_reply_get_item = nullptr;

inline const char* reply_contains_flow(uintptr_t caller) {
    if (caller == kPlayerVersionContainsKeyReturnRva) {
        return "get_player_version";
    }
    if (caller == 0x17A9A70u || caller == 0x17A9AE4u) {
        // Progress flag handler for progress_status (0x17A9980). These are
        // the return addresses of its two membership tests, which decide
        // the static Nullable<bool> that the auth
        // scene fork at 0x3DC9854 reads before it starts the coroutine that
        // carries the boot past 90%.
        return "progress_status";
    }
    if (caller == 0x33F124Cu) {
        // get_progress callback at 0x33F1114. The value behind this key is
        // accepted only when it is a HashSet<string>.
        return "get_progress";
    }
    if (caller == 0x33F1568u) {
        // get_progress_slots callback at 0x33F145C, immediately after its
        // single top-level ContainsKey call.
        return "get_progress_slots";
    }
    if (caller == 0x2BC0B4Cu) {
        // GameModeEvents JSON root: this names the required top-level member.
        // The stock return value is passed through unchanged.
        return "gme_root";
    }
    if (caller == 0x2BC1014u) {
        return "gme_event_id";
    }
    if (caller == 0x2BC1098u) {
        return "gme_owner_id";
    }
    if (caller == 0x17AB878u || caller == 0x17AB8C4u ||
        caller == 0x17AB8F4u) {
        // sync_full callback at 0x17AB704. The first optional marker adds the
        // stock 1602 code, the second tests the error-code member, and the
        // third optionally applies a server document. Run 151 names all three
        // keys without changing the reply or either branch.
        return "sync_full";
    }
    switch (caller) {
        case 0x3D03EF8u:
        case 0x3D03F7Cu:
        case 0x3D041BCu:
        case 0x3D04280u:
        case 0x3D043ECu:
        case 0x3D04658u:
        case 0x3D04D64u:
            return "refund_get";
        default:
            return nullptr;
    }
}

inline const char* reply_get_item_flow(uintptr_t caller) {
    if (caller == kPlayerVersionGetItemReturnRva) {
        return "get_player_version";
    }
    if (caller == 0x17A9B10u) {
        // Value paired with the progress_status membership tests above.
        return "progress_status";
    }
    if (caller == 0x33F1268u) {
        return "get_progress";
    }
    if (caller == 0x33F1594u) {
        // Value paired with the ContainsKey call above. Logging its runtime
        // class checks that the backend served Dictionary<string, object>.
        return "get_progress_slots";
    }
    if (caller == 0x2BC0B70u) {
        return "gme_root";
    }
    if (caller == 0x2BC1034u) {
        return "gme_event_id";
    }
    if (caller == 0x2BC10CCu) {
        return "gme_owner_id";
    }
    if (caller == 0x17AB918u || caller == 0x17ABA80u) {
        return "sync_full";
    }
    switch (caller) {
        case 0x3D03F14u:
        case 0x3D03F94u:
        case 0x3D041D0u:
        case 0x3D04294u:
        case 0x3D0466Cu:
        case 0x3D04D98u:
            return "refund_get";
        default:
            return nullptr;
    }
}

inline bool hook_player_version_contains_key(void* dictionary, void* key,
                                             const MethodInfo* method) {
    const void* raw =
        __builtin_extract_return_addr(__builtin_return_address(0));
    const uintptr_t pc = reinterpret_cast<uintptr_t>(raw) & ~uintptr_t{1};
    const uintptr_t caller =
        g_il2cpp_base != 0 && pc >= g_il2cpp_base ? pc - g_il2cpp_base : 0;
    const char* flow = reply_contains_flow(caller);
    const std::string key_text = flow != nullptr ? text_of(key) : std::string();
    const bool present = g_reply_contains_key != nullptr
                             ? g_reply_contains_key(dictionary, key, method)
                             : false;
    if (flow != nullptr) {
        LOGI("23.1.3-replykey: flow=%s op=ContainsKey caller=0x%llx "
             "key='%s' present=%d dictionary=%p count@0x20=%d count@0x28=%d",
             flow, static_cast<unsigned long long>(caller), key_text.c_str(),
             present ? 1 : 0, dictionary,
             read_i32(dictionary, kDictCountFramework),
             read_i32(dictionary, kDictCountCoreLib));
    }
    return present;
}

inline void* hook_reply_get_item(void* dictionary, void* key,
                                 const MethodInfo* method) {
    const void* raw =
        __builtin_extract_return_addr(__builtin_return_address(0));
    const uintptr_t pc = reinterpret_cast<uintptr_t>(raw) & ~uintptr_t{1};
    const uintptr_t caller =
        g_il2cpp_base != 0 && pc >= g_il2cpp_base ? pc - g_il2cpp_base : 0;
    const char* flow = reply_get_item_flow(caller);
    const std::string key_text = flow != nullptr ? text_of(key) : std::string();
    void* value = g_reply_get_item != nullptr
                      ? g_reply_get_item(dictionary, key, method)
                      : nullptr;
    if (flow != nullptr) {
        const char* value_class = class_name_of(value);
        const std::string value_text =
            std::string(value_class) == "String" ? text_of(value) : std::string();
        LOGI("23.1.3-replykey: flow=%s op=get_Item caller=0x%llx key='%s' "
             "value=%p class=%s text='%s'",
             flow, static_cast<unsigned long long>(caller), key_text.c_str(),
             value, value_class, value_text.c_str());
        if (std::string(flow) == "gme_root") {
            LOGI("23.1.3-gme-root: key='%s' value=%p class=%s "
                 "listSize@0x18=%d -- passive",
                 key_text.c_str(), value, value_class,
                 read_i32(value, kListSize));
        }
    }
    return value;
}

inline void hook_get_inapps_int_lookup(void* dictionary, void* key,
                                       int32_t* out_value,
                                       const MethodInfo* method) {
    const void* raw =
        __builtin_extract_return_addr(__builtin_return_address(0));
    const uintptr_t pc = reinterpret_cast<uintptr_t>(raw) & ~uintptr_t{1};
    const uintptr_t caller =
        g_il2cpp_base != 0 && pc >= g_il2cpp_base ? pc - g_il2cpp_base : 0;
    const bool wanted = get_inapps_lookup_call(caller);
    const std::string key_text = wanted ? text_of(key) : std::string();
    if (g_get_inapps_int_lookup != nullptr) {
        g_get_inapps_int_lookup(dictionary, key, out_value, method);
    }
    if (wanted) {
        LOGI("23.1.3-replyint: parser caller=0x%llx key='%s' value=%d dict=%p",
             static_cast<unsigned long long>(caller), key_text.c_str(),
             out_value != nullptr ? *out_value : INT32_MIN, dictionary);
    }
}

inline bool hook_try_get_value(void* dictionary, void* key, void** out_value,
                               const MethodInfo* method) {
    const void* raw =
        __builtin_extract_return_addr(__builtin_return_address(0));
    const uintptr_t pc = reinterpret_cast<uintptr_t>(raw) & ~uintptr_t{1};
    const uintptr_t caller =
        g_il2cpp_base != 0 && pc >= g_il2cpp_base ? pc - g_il2cpp_base : 0;
    const bool wanted = config_dictionary_call(caller);
    const char* key_class = wanted ? class_name_of(key) : "";
    const std::string key_text =
        wanted && std::string(key_class) == "String" ? text_of(key) : std::string();

    bool hit = false;
    if (g_try_get_value != nullptr) {
        hit = g_try_get_value(dictionary, key, out_value, method);
    }
    if (wanted) {
        void* value = out_value != nullptr ? *out_value : nullptr;
        const char* value_class = class_name_of(value);
        const std::string value_text =
            std::string(value_class) == "String" ? text_of(value) : std::string();
        LOGI("23.1.3-cfgdict: caller=0x%llx dict=%p count@0x20=%d count@0x28=%d "
             "key=%p class=%s text='%s' hit=%d value=%p class=%s text='%s'",
             static_cast<unsigned long long>(caller), dictionary,
             read_i32(dictionary, kDictCountFramework),
             read_i32(dictionary, kDictCountCoreLib), key, key_class,
             key_text.c_str(), hit ? 1 : 0, value, value_class,
             value_text.c_str());
    }
    return hit;
}

// Both read hooks call the original first and report what it returned, so a
// consumer that logs a null config can be matched to the exact id it asked for
// and to whether the storage answered with our document or with nothing.
// Saves a document whole, once per id, in the app's own external files
// directory, so the text the client reads can be compared with what this
// server hosts instead of being guessed from a truncated preview.
inline void dump_document_once(int32_t id, void* text_object) {
    static std::mutex mutex;
    static std::unordered_set<int32_t> dumped;
    {
        const std::lock_guard<std::mutex> guard(mutex);
        if (!dumped.insert(id).second) return;
    }
    const std::string text = il2cpp::to_utf8(text_object, kDocumentDumpLimit);
    if (text.empty()) return;
    char path[192];
    std::snprintf(path, sizeof(path),
                  "/storage/emulated/0/Android/data/com.pixel.gun3d/files/"
                  "opg3d-doc-%d.json",
                  id);
    const int fd = ::open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        LOGW("23.1.3-cfgsys: cannot write %s (errno=%d)", path, errno);
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
    LOGI("23.1.3-cfgsys: document %d saved %zu/%zu char(s) -> %s", id, written,
         text.size(), path);
}

inline void* hook_get_text(void* self, int32_t id, const MethodInfo* method) {
    void* result = nullptr;
    if (g_get_text != nullptr) {
        result = g_get_text(self, id, method);
    }
    const uint32_t call = g_get_text_calls.fetch_add(1u) + 1u;
    if (result != nullptr) dump_document_once(id, result);
    const std::string head = text_of(result);
    LOGI("23.1.3-cfgsys: #%u get-text id=%d null=%d chars=%d head='%s'", call,
         id, result == nullptr ? 1 : 0, static_cast<int>(head.size()),
         head.c_str());
    return result;
}

inline void* hook_get_bytes(void* self, int32_t id, const MethodInfo* method) {
    void* result = nullptr;
    if (g_get_bytes != nullptr) {
        result = g_get_bytes(self, id, method);
    }
    const uint32_t call = g_get_bytes_calls.fetch_add(1u) + 1u;
    LOGI("23.1.3-cfgsys: #%u get-bytes id=%d bytes=%d", call, id,
         result == nullptr ? -1 : read_i32(result, kArrayLength));
    return result;
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

    armed += hook::install({detail::kNamespace, detail::kConfigSystem, detail::kGetText, 1},
                           reinterpret_cast<void*>(&detail::hook_get_text),
                           reinterpret_cast<void**>(&detail::g_get_text), false)
                 ? 1
                 : 0;
    armed += hook::install({detail::kNamespace, detail::kConfigSystem, detail::kGetBytes, 1},
                           reinterpret_cast<void*>(&detail::hook_get_bytes),
                           reinterpret_cast<void**>(&detail::g_get_bytes), false)
                 ? 1
                 : 0;

    uintptr_t il2cpp_base = 0;
    const bool found_il2cpp =
        elfsym::find_library("libil2cpp.so", &il2cpp_base) && il2cpp_base != 0;
    detail::g_il2cpp_base = found_il2cpp ? il2cpp_base : 0;
    const bool dictionary_trace =
        found_il2cpp && hook::install_address(
            reinterpret_cast<void*>(il2cpp_base + detail::kTryGetValueRva),
            "23.1.3 config Dictionary.TryGetValue trace",
            reinterpret_cast<void*>(&detail::hook_try_get_value),
            reinterpret_cast<void**>(&detail::g_try_get_value), false);
    const bool get_inapps_trace =
        found_il2cpp && hook::install_address(
            reinterpret_cast<void*>(il2cpp_base +
                                    detail::kGetInappsIntLookupRva),
            "23.1.3 get_inapps integer reply trace",
            reinterpret_cast<void*>(&detail::hook_get_inapps_int_lookup),
            reinterpret_cast<void**>(&detail::g_get_inapps_int_lookup), false);
    const bool player_version_trace =
        found_il2cpp && hook::install_address(
            reinterpret_cast<void*>(
                il2cpp_base + detail::kPlayerVersionContainsKeyRva),
            "23.1.3 get_player_version reply-key trace",
            reinterpret_cast<void*>(&detail::hook_player_version_contains_key),
            reinterpret_cast<void**>(&detail::g_reply_contains_key), false);
    const bool reply_getter_trace =
        found_il2cpp && hook::install_address(
            reinterpret_cast<void*>(il2cpp_base +
                                    detail::kPlayerVersionGetItemRva),
            "23.1.3 selected socket reply value trace",
            reinterpret_cast<void*>(&detail::hook_reply_get_item),
            reinterpret_cast<void**>(&detail::g_reply_get_item), false);

    LOGI("23.1.3-cfgsys: config system trace armed on %d/11 entry point(s), "
         "dictionary=%d, get_inapps=%d, reply_keys=%d, reply_values=%d, in "
         "namespace %s; "
         "parse/toInt/apply/store answer the delivery question, "
         "groups/request/namemap/embedded/step answer the request question, "
         "apply=document text, store=binary config storage",
         armed, dictionary_trace ? 1 : 0, get_inapps_trace ? 1 : 0,
         player_version_trace ? 1 : 0, reply_getter_trace ? 1 : 0,
         detail::kNamespace);
    return armed > 0 || dictionary_trace || get_inapps_trace ||
           player_version_trace || reply_getter_trace;
}

} // namespace config_system_trace_2313

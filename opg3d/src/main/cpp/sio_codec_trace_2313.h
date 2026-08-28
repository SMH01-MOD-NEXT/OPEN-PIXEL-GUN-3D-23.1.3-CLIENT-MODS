#pragma once

// -----------------------------------------------------------------------------
// 23.1.3 (ARM64) thin client - socket.io payload codec trace (LOGGING ONLY)
//
// Why this exists
// ---------------
// The auth namespace does not speak plaintext socket.io. The client connects to
// /sio and immediately emits one BINARY_EVENT whose argument array holds nothing
// but two placeholders:
//
//   452-/sio,[{"_placeholder":true,"num":0},{"_placeholder":true,"num":1}]
//     attachment 0:  16 bytes  <- the event NAME, sealed
//     attachment 1: 608 bytes  <- the payload,    sealed
//
// Both lengths are exact multiples of the AES block size, and attachment 0 came
// out completely different in two sessions whose first emit was the same event,
// so the codec is keyed or IV'd per connection. Every offline attempt to recover
// a static key failed: the metadata literal table (43998 entries) carries no
// standalone key - every hex or base64 blob of key length is a BouncyCastle
// curve parameter - and the four app-owned GUID-shaped constants were all
// rejected by the decrypt oracle.
//
// So the key material is assembled at runtime. This component only observes the
// boundary where it is used. It changes no behaviour: every hook calls straight
// through to the original and returns its result untouched.
//
// The boundary (Rilisoft.WebSocket.WebSocketManager, TypeDefIndex 9374)
// --------------------------------------------------------------------
// dump2313.cs shows the send path still carrying an OPEN event name, with the
// sealing happening underneath it:
//
//   send overloads (string name, Dictionary args)
//     -> builder      (string, Dictionary, float, callback, byte[])  0x35C2D4C
//        -> emit      (string name, object args, byte[] blob) : bool 0x35C31E8
//           -> codec pair (Dictionary<string,object>,
//                          Dictionary<object,object>,
//                          List<byte[]>)              0x35C4028 / 0x35C4170
//
// That last pair is the codec itself: open dictionaries on one side, the wire's
// List<byte[]> attachments on the other. Logging that list before and after the
// original call is what reveals which overload seals and which opens, and it
// pairs a known plaintext event name with the exact 16-byte block the server
// already holds on the wire.
//
// The builder is deliberately NOT hooked: it returns ValueTuple<int, Task<...>>,
// a 16-byte aggregate returned in x0/x1, and the open event name is already
// available from the emit wrapper, which returns a plain bool.
//
// Overload safety
// ---------------
// Every target is unique by argument count inside this class. The identically
// named members of Rilisoft.WebSocket.MatchmakingSocketManager belong to a
// different class, so metadata lookup by {namespace, class, name, argc} cannot
// reach them, and the codec-B name occurs exactly once in the whole dump. No
// absolute RVA is used anywhere: the ones in these comments are build evidence
// only, and they were confirmed against a live run - libil2cpp.so base
// 0x77E7635000 plus 0x35C1C50 is the 0x77EABF6C50 that the address setter hook
// reported in logcat.
//
// What the first fully armed run proved (2026-08-28)
// --------------------------------------------------
// All four WebSocketManager hooks installed at exactly their intended
// addresses - libil2cpp.so base 0x77F0541000 plus 0x35C31E8 / 0x35C4028 /
// 0x35C4170 / 0x35C396C matched the four logcat lines one for one - and the
// client then completed a full sealed handshake. Yet across the entire session
// only [emit] printed:
//
//   23.1.3-sio[emit] event='auth_v2' args=0x788d9102a0 blob_bytes=-1
//   23.1.3-sio[emit] event='auth_v2' accepted=1
//
// codec-shared, codec-sio and inbound printed nothing at all. That silence is
// evidence, not a missed hook: the trace numbers its lines, logcat carried
// #000001 through #000262 with no gap, and the 600-line budget had spent 2. So
// the codec pair hypothesis recorded above is wrong. Those two methods are not
// on the emit path; they are reached only by whatever else that class serves.
//
// The sealing sits one level down instead, on the standalone cipher type that
// SocketManager.get_Extras() lazily builds and caches in its _extras field
// (offset 0xA0). Disassembling that getter at RVA 0x3ECF1F4 shows it XOR two
// static 32-byte arrays into a password buffer and hand it to the cipher's
// constructor at RVA 0x3B680F4 - with no session token, sid, handshake value or
// RNG anywhere on the path. Hooking that constructor and its encrypt/decrypt
// pair is therefore both necessary and sufficient, and it is what the cipher
// hooks below do.
//
// Reading managed containers
// --------------------------
// Il2CppDumper cannot resolve field offsets for open generic types: List<T> and
// Dictionary<TKey,TValue> print every field as "// 0x0". So no generic field is
// read by offset here - fields are fetched by name through
// il2cpp_class_get_field_from_name plus il2cpp_field_get_value on the
// instantiated class, which is layout independent and fails closed. All three of
// those exports sit in resolve()'s required set.
//
// Arrays have no named fields, so Il2CppArray is read by its ABI layout. That
// layout is derived rather than assumed: String._stringLength sits at 0x10 in
// this build's dump, which pins the managed object header at 16 bytes, giving
// bounds at 0x10, max_length at 0x18 and the first element at 0x20.
// -----------------------------------------------------------------------------

#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "hook.h"
#include "il2cpp.h"
#include "log.h"

namespace sio_codec_trace_2313 {
namespace detail {

static_assert(sizeof(void*) == 8, "PG3D 23.1.3 target must be arm64-v8a");

constexpr const char* kNs = "Rilisoft.WebSocket";
constexpr const char* kClass = "WebSocketManager";

// The members are renamed to CJK identifiers in this build. These are the real
// metadata names, spelled as UTF-8 bytes so the source encoding cannot matter.

// emit wrapper: (string name, object args, byte[] blob) : bool   RVA 0x35C31E8
constexpr const char* kEmit =
    "\xE4\xB8\x94\xE4\xB8\x85\xE4\xB8\x92\xE4\xB8\x93\xE4\xB8\x96"
    "\xE4\xB8\x88\xE4\xB8\x91\xE4\xB8\x9D\xE4\xB8\x9E";
// codec A, also present in MatchmakingSocketManager                RVA 0x35C4028
constexpr const char* kCodecShared =
    "\xE4\xB8\x8C\xE4\xB8\x97\xE4\xB8\x83\xE4\xB8\x8E\xE4\xB8\x98"
    "\xE4\xB8\x84\xE4\xB8\x8C\xE4\xB8\x9E\xE4\xB8\x87";
// codec B, unique to WebSocketManager in the whole dump            RVA 0x35C4170
constexpr const char* kCodecUnique =
    "\xE4\xB8\x9E\xE4\xB8\x9B\xE4\xB8\x9A\xE4\xB8\x8A\xE4\xB8\x97"
    "\xE4\xB8\x93\xE4\xB8\x8F\xE4\xB8\x84\xE4\xB8\x89";
// inbound dispatcher: (Socket, Packet, object[])                   RVA 0x35C396C
constexpr const char* kInbound =
    "\xE4\xB8\x82\xE4\xB8\x81\xE4\xB8\x8E\xE4\xB8\x9B\xE4\xB8\x95"
    "\xE4\xB8\x91\xE4\xB8\x85\xE4\xB8\x81\xE4\xB8\x9A";

// -- the real cipher boundary ----------------------------------------------
//
// The four WebSocketManager hooks above are not it. See the note at the top of
// this file: in the first fully armed run only [emit] produced output, while
// codec-shared, codec-sio and inbound stayed silent for the whole session even
// though all four installed and the line budget was untouched. The sealing
// therefore happens below that class, in the standalone codec type that
// SocketManager.get_Extras() builds once and caches.
//
// dump2313.cs, TypeDefIndex 8304:
//
//   // Namespace: Rilisoft
//   public sealed class <kCodecClass>
//   {
//       private readonly SymmetricAlgorithm ...;              // 0x10
//       public void .ctor(byte[] password)                    RVA 0x3B680F4
//       public byte[] <kCodecDecrypt>(byte[])                 RVA 0x3B6835C
//       public byte[] <kCodecEncrypt>(byte[], bool = False)   RVA 0x3B6883C
//   }
//
// The constructor argument is the 32-byte password that every offline search
// failed to recover, and the encrypt argument is the open event name / open
// payload whose sealed forms are already on the wire. Logging both ends of that
// pair is what turns the sealed handshake into a known-plaintext transcript.
//
// Name safety: the sibling type at TypeDefIndex 8307 declares same-named
// members, but they are ArraySegment<byte> overloads on a different class, so
// metadata lookup by {namespace, class, name, argc} cannot reach them. Inside
// 8304 all three targets are unique by argument count.
constexpr const char* kCodecClass =
    "\xE4\xB8\x8F\xE4\xB8\x9C\xE4\xB8\x91\xE4\xB8\x84\xE4\xB8\x9C"
    "\xE4\xB8\x88\xE4\xB8\x9F\xE4\xB8\x94\xE4\xB8\x9C";
constexpr const char* kCodecCtor = ".ctor";
// decrypt: (byte[]) : byte[]                                       RVA 0x3B6835C
constexpr const char* kCodecDecrypt =
    "\xE4\xB8\x82\xE4\xB8\x9C\xE4\xB8\x94\xE4\xB8\x85\xE4\xB8\x8F"
    "\xE4\xB8\x91\xE4\xB8\x89\xE4\xB8\x93\xE4\xB8\x9B";
// encrypt: (byte[], bool = False) : byte[]                         RVA 0x3B6883C
constexpr const char* kCodecEncrypt =
    "\xE4\xB8\x9A\xE4\xB8\x8D\xE4\xB8\x90\xE4\xB8\x85\xE4\xB8\x92"
    "\xE4\xB8\x9D\xE4\xB8\x99\xE4\xB8\x93\xE4\xB8\x87";

// Mono spells SymmetricAlgorithm's protected state with these names. Both reads
// fail closed, so a layout that hides them only costs the two extra log fields.
constexpr const char* kAlgField =
    "\xE4\xB8\x8D\xE4\xB8\x8D\xE4\xB8\x94\xE4\xB8\x81\xE4\xB8\x84"
    "\xE4\xB8\x91\xE4\xB8\x95\xE4\xB8\x84\xE4\xB8\x8D";
constexpr const char* kKeyValue = "KeyValue";
constexpr const char* kIVValue = "IVValue";

// Generated managed methods take their explicit arguments followed by
// MethodInfo*. Instance methods take `this` first.
using EmitFn = bool (*)(void* self, void* name, void* args, void* blob,
                        void* method);
using CodecFn = void (*)(void* self, void* headers, void* payload,
                         void* attachments, void* method);
using InboundFn = void (*)(void* self, void* socket, void* packet, void* args,
                           void* method);
// .ctor(byte[] password); the password is the whole missing secret.
using CipherCtorFn = void (*)(void* self, void* password, void* method);
// (byte[]) : byte[]
using CipherOpenFn = void* (*)(void* self, void* input, void* method);
// (byte[], bool) : byte[]; the trailing bool rides in w2, ahead of MethodInfo*.
using CipherSealFn = void* (*)(void* self, void* input, bool flag,
                               void* method);

inline EmitFn g_orig_emit = nullptr;
inline CodecFn g_orig_codec_shared = nullptr;
inline CodecFn g_orig_codec_unique = nullptr;
inline InboundFn g_orig_inbound = nullptr;
inline CipherCtorFn g_orig_cipher_ctor = nullptr;
inline CipherOpenFn g_orig_cipher_open = nullptr;
inline CipherSealFn g_orig_cipher_seal = nullptr;

inline bool g_installed = false;

// Il2CppArray layout, derived from String._stringLength being at 0x10.
constexpr size_t kArrayLength = 0x18u;
constexpr size_t kArrayData = 0x20u;

constexpr int64_t kMaxArrayBytes = 1024 * 1024;
constexpr size_t kHexBytesLogged = 320u;
constexpr int32_t kMaxAttachments = 8;
// The open payload is a 608-byte sealed blob, so its plaintext fits well inside
// this cap; the sealed side stays on the hex budget above.
constexpr size_t kTextBytesLogged = 768u;

// A sealed handshake is a handful of lines. The budget only stops a chatty
// config channel from burying the rest of logcat.
constexpr uint64_t kMaxLines = 600u;
inline uint64_t g_lines = 0u;
inline bool g_quiet = false;

inline bool budget() {
    if (g_lines < kMaxLines) {
        ++g_lines;
        return true;
    }
    if (!g_quiet) {
        g_quiet = true;
        LOGW("23.1.3-sio: %" PRIu64 " trace lines emitted; going quiet so the "
             "rest of logcat stays usable", kMaxLines);
    }
    return false;
}

template <typename T>
static T read_at(const void* base, size_t offset, T fallback) noexcept {
    if (base == nullptr) return fallback;
    T value = fallback;
    std::memcpy(&value, static_cast<const uint8_t*>(base) + offset, sizeof(T));
    return value;
}

// Field read by name on the instantiated class: the only layout-independent way
// to touch List<T> and Dictionary<K,V>, whose offsets the dump cannot provide.
inline bool read_field(void* object, const char* name, void* scratch) {
    if (object == nullptr || name == nullptr || scratch == nullptr) return false;
    if (il2cpp::object_get_class == nullptr ||
        il2cpp::class_get_field_from_name == nullptr ||
        il2cpp::field_get_value == nullptr) {
        return false;
    }
    void* klass = il2cpp::object_get_class(object);
    if (klass == nullptr) return false;
    void* field = il2cpp::class_get_field_from_name(klass, name);
    if (field == nullptr) return false;
    il2cpp::field_get_value(object, field, scratch);
    return true;
}

inline void* read_field_ptr(void* object, const char* name) {
    void* value = nullptr;
    return read_field(object, name, &value) ? value : nullptr;
}

inline int32_t read_field_i32(void* object, const char* name, int32_t fallback) {
    int32_t value = fallback;
    return read_field(object, name, &value) ? value : fallback;
}

// -1 means "this pointer is not the array we assumed", never a real length.
inline int64_t array_length(void* array) {
    if (array == nullptr) return -1;
    const uint64_t raw = read_at<uint64_t>(array, kArrayLength, 0u);
    if (raw > static_cast<uint64_t>(kMaxArrayBytes)) return -1;
    return static_cast<int64_t>(raw);
}

inline std::string bytes_to_hex(const uint8_t* data, size_t length, size_t cap) {
    if (data == nullptr) return "<null>";
    static constexpr char kDigits[] = "0123456789abcdef";
    const size_t limit = length < cap ? length : cap;
    std::string out;
    out.reserve(limit * 2u + 8u);
    for (size_t i = 0u; i < limit; ++i) {
        out.push_back(kDigits[data[i] >> 4u]);
        out.push_back(kDigits[data[i] & 0x0Fu]);
    }
    if (length > limit) out += "...";
    return out;
}

inline std::string byte_array_hex(void* array) {
    const int64_t length = array_length(array);
    if (length < 0) return "<not-an-array>";
    if (length == 0) return "<empty>";
    const uint8_t* data = static_cast<const uint8_t*>(array) + kArrayData;
    return bytes_to_hex(data, static_cast<size_t>(length), kHexBytesLogged);
}

// The open side of the cipher is a UTF-8 event name or a UTF-8 JSON payload, so
// a readable rendering saves hand-decoding hex. Non-printable bytes collapse to
// '.' rather than being dropped, which keeps every offset lined up with the hex.
inline std::string bytes_to_text(const uint8_t* data, size_t length,
                                 size_t cap) {
    if (data == nullptr) return "<null>";
    const size_t limit = length < cap ? length : cap;
    std::string out;
    out.reserve(limit + 8u);
    for (size_t i = 0u; i < limit; ++i) {
        const uint8_t byte = data[i];
        out.push_back(byte >= 0x20u && byte < 0x7Fu ? static_cast<char>(byte)
                                                   : '.');
    }
    if (length > limit) out += "...";
    return out;
}

inline std::string byte_array_text(void* array) {
    const int64_t length = array_length(array);
    if (length < 0) return "<not-an-array>";
    if (length == 0) return "<empty>";
    const uint8_t* data = static_cast<const uint8_t*>(array) + kArrayData;
    return bytes_to_text(data, static_cast<size_t>(length), kTextBytesLogged);
}

inline int32_t dictionary_count(void* dictionary) {
    return read_field_i32(dictionary, "_count", -1);
}

// Dumps a List<byte[]> exactly as it stands at this instant. Called before and
// after the original, it shows which direction the codec ran: a packer starts
// empty and ends full, an opener starts full.
inline void log_attachments(const char* surface, const char* stage,
                            void* list) {
    if (!budget()) return;
    if (list == nullptr) {
        LOGI("23.1.3-sio[%s] %s: attachments=<null>", surface, stage);
        return;
    }

    const int32_t size = read_field_i32(list, "_size", -1);
    void* items = read_field_ptr(list, "_items");
    if (size < 0 || items == nullptr) {
        LOGW("23.1.3-sio[%s] %s: attachment list unreadable (size=%d items=%p)",
             surface, stage, size, items);
        return;
    }
    LOGI("23.1.3-sio[%s] %s: attachments=%d", surface, stage, size);

    const int32_t shown = size < kMaxAttachments ? size : kMaxAttachments;
    for (int32_t i = 0; i < shown; ++i) {
        if (!budget()) return;
        void* element = read_at<void*>(
            items, kArrayData + 8u * static_cast<size_t>(i), nullptr);
        LOGI("23.1.3-sio[%s] %s: attachment %d bytes=%" PRId64 " hex=%s",
             surface, stage, i, array_length(element),
             byte_array_hex(element).c_str());
    }
}

// -- hook bodies ------------------------------------------------------------

// The open event name lands here. Attachment 0 on the wire is exactly this
// string sealed, so this line is the known plaintext for that 16-byte block.
inline bool emit_hook(void* self, void* name, void* args, void* blob,
                      void* method) {
    const std::string event = il2cpp::to_utf8(name, 128);
    if (budget()) {
        LOGI("23.1.3-sio[emit] event='%s' args=%p blob_bytes=%" PRId64
             " blob_hex=%s",
             event.c_str(), args, array_length(blob),
             byte_array_hex(blob).c_str());
    }

    const bool result = g_orig_emit != nullptr
                            ? g_orig_emit(self, name, args, blob, method)
                            : false;

    if (budget()) {
        LOGI("23.1.3-sio[emit] event='%s' accepted=%d", event.c_str(),
             result ? 1 : 0);
    }
    return result;
}

inline void codec_common(const char* surface, CodecFn original, void* self,
                         void* headers, void* payload, void* attachments,
                         void* method) {
    if (budget()) {
        LOGI("23.1.3-sio[%s] enter: headers=%d payload=%d", surface,
             dictionary_count(headers), dictionary_count(payload));
    }
    log_attachments(surface, "before", attachments);

    if (original != nullptr) {
        original(self, headers, payload, attachments, method);
    }

    if (budget()) {
        LOGI("23.1.3-sio[%s] leave: headers=%d payload=%d", surface,
             dictionary_count(headers), dictionary_count(payload));
    }
    log_attachments(surface, "after", attachments);
}

inline void codec_shared_hook(void* self, void* headers, void* payload,
                              void* attachments, void* method) {
    codec_common("codec-shared", g_orig_codec_shared, self, headers, payload,
                 attachments, method);
}

inline void codec_unique_hook(void* self, void* headers, void* payload,
                              void* attachments, void* method) {
    codec_common("codec-sio", g_orig_codec_unique, self, headers, payload,
                 attachments, method);
}

// Inbound side. Packet is not generic, so its attachment list is reachable by
// name too.
inline void inbound_hook(void* self, void* socket, void* packet, void* args,
                         void* method) {
    if (budget()) {
        LOGI("23.1.3-sio[inbound] packet=%p args=%p", packet, args);
    }
    log_attachments("inbound", "received", read_field_ptr(packet, "attachments"));

    if (g_orig_inbound != nullptr) {
        g_orig_inbound(self, socket, packet, args, method);
    }
}

// -- the cipher itself ------------------------------------------------------

// The cipher type is a plain Rilisoft type, not a Rilisoft.WebSocket one.
constexpr const char* kCipherNs = "Rilisoft";

// The constructor argument is the 32-byte password that every offline search
// failed to recover, so this one line is the point of the whole component: with
// it the backend can rebuild the cipher and open both attachments itself.
inline void cipher_ctor_hook(void* self, void* password, void* method) {
    if (budget()) {
        LOGI("23.1.3-sio[cipher] .ctor password bytes=%" PRId64 " hex=%s",
             array_length(password), byte_array_hex(password).c_str());
    }

    if (g_orig_cipher_ctor != nullptr) {
        g_orig_cipher_ctor(self, password, method);
    }

    // Best effort, after the original has built the algorithm: if Mono's
    // protected state is reachable by name, the derived key and IV can be
    // lifted directly and the PBKDF2 salt stops mattering at all. Both reads
    // fail closed, so an unexpected layout only costs these two lines.
    void* algorithm = read_field_ptr(self, kAlgField);
    if (algorithm == nullptr) return;

    if (budget()) {
        void* key = read_field_ptr(algorithm, kKeyValue);
        LOGI("23.1.3-sio[cipher] .ctor derived key bytes=%" PRId64 " hex=%s",
             array_length(key), byte_array_hex(key).c_str());
    }
    if (budget()) {
        void* iv = read_field_ptr(algorithm, kIVValue);
        LOGI("23.1.3-sio[cipher] .ctor derived iv bytes=%" PRId64 " hex=%s",
             array_length(iv), byte_array_hex(iv).c_str());
    }
}

// Inbound direction: sealed bytes in, open bytes out. This is what the client
// expects the server's reply to look like once it is unwrapped.
inline void* cipher_open_hook(void* self, void* input, void* method) {
    if (budget()) {
        LOGI("23.1.3-sio[cipher] decrypt sealed bytes=%" PRId64 " hex=%s",
             array_length(input), byte_array_hex(input).c_str());
    }

    void* output = g_orig_cipher_open != nullptr
                       ? g_orig_cipher_open(self, input, method)
                       : nullptr;

    if (budget()) {
        LOGI("23.1.3-sio[cipher] decrypt open bytes=%" PRId64 " text='%s'",
             array_length(output), byte_array_text(output).c_str());
    }
    return output;
}

// Outbound direction. The open side is the event name on the first call and the
// encoded argument payload on the second, which pairs each wire attachment with
// its exact plaintext.
inline void* cipher_seal_hook(void* self, void* input, bool flag,
                              void* method) {
    if (budget()) {
        LOGI("23.1.3-sio[cipher] encrypt open bytes=%" PRId64
             " flag=%d text='%s'",
             array_length(input), flag ? 1 : 0,
             byte_array_text(input).c_str());
    }

    void* output = g_orig_cipher_seal != nullptr
                       ? g_orig_cipher_seal(self, input, flag, method)
                       : nullptr;

    if (budget()) {
        LOGI("23.1.3-sio[cipher] encrypt sealed bytes=%" PRId64 " hex=%s",
             array_length(output), byte_array_hex(output).c_str());
    }
    return output;
}

inline bool install() {
    if (g_installed) return true;

    const bool emit =
        hook::install({kNs, kClass, kEmit, 3},
                      reinterpret_cast<void*>(&emit_hook),
                      reinterpret_cast<void**>(&g_orig_emit));
    const bool shared =
        hook::install({kNs, kClass, kCodecShared, 3},
                      reinterpret_cast<void*>(&codec_shared_hook),
                      reinterpret_cast<void**>(&g_orig_codec_shared));
    const bool unique =
        hook::install({kNs, kClass, kCodecUnique, 3},
                      reinterpret_cast<void*>(&codec_unique_hook),
                      reinterpret_cast<void**>(&g_orig_codec_unique));
    const bool inbound =
        hook::install({kNs, kClass, kInbound, 3},
                      reinterpret_cast<void*>(&inbound_hook),
                      reinterpret_cast<void**>(&g_orig_inbound));

    // The decisive three. Argument counts separate them from each other and
    // from the same-named ArraySegment overloads on the sibling type.
    const bool cipher_ctor =
        hook::install({kCipherNs, kCodecClass, kCodecCtor, 1},
                      reinterpret_cast<void*>(&cipher_ctor_hook),
                      reinterpret_cast<void**>(&g_orig_cipher_ctor));
    const bool cipher_open =
        hook::install({kCipherNs, kCodecClass, kCodecDecrypt, 1},
                      reinterpret_cast<void*>(&cipher_open_hook),
                      reinterpret_cast<void**>(&g_orig_cipher_open));
    const bool cipher_seal =
        hook::install({kCipherNs, kCodecClass, kCodecEncrypt, 2},
                      reinterpret_cast<void*>(&cipher_seal_hook),
                      reinterpret_cast<void**>(&g_orig_cipher_seal));

    g_installed = emit || shared || unique || inbound || cipher_ctor ||
                  cipher_open || cipher_seal;

    LOGI("23.1.3-sio: codec trace armed (emit=%d codec-shared=%d codec-sio=%d "
         "inbound=%d cipher-ctor=%d cipher-open=%d cipher-seal=%d); logging "
         "only, no packet is altered",
         emit ? 1 : 0, shared ? 1 : 0, unique ? 1 : 0, inbound ? 1 : 0,
         cipher_ctor ? 1 : 0, cipher_open ? 1 : 0, cipher_seal ? 1 : 0);

    if (!cipher_ctor && !cipher_seal) {
        LOGE("23.1.3-sio: neither the cipher constructor nor its encrypt method "
             "could be hooked, so this run recovers no password and no open "
             "payload");
    }

    if (!emit && !cipher_ctor && !cipher_seal) {
        LOGE("23.1.3-sio: no usable hook on either the emit or the cipher path, "
             "so the sealed event name stays unreadable");
        return false;
    }
    return true;
}

} // namespace detail

// Observes the auth namespace's payload codec. Logging only: every hook calls
// the original and returns its result unchanged. Must run after
// Assembly-CSharp.dll is available.
inline bool install_hooks() { return detail::install(); }

} // namespace sio_codec_trace_2313

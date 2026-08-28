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
// Both lengths are exact multiples of the AES block size, and the backend has
// since measured exactly what varies. Attachment 0 is byte-identical across
// four connections that shared one pinned socket.io sid - spanning three server
// restarts - and different for every distinct sid. Attachment 1 repeats its
// first seven blocks under a repeated sid and only then diverges, where the
// payload's own per-connection counter starts. So the key material is a
// deterministic function of the sid, with no per-connection randomness of its
// own, and the plaintext is what moves the tail. Every offline attempt to
// recover a static key failed: the metadata literal table (43998 entries)
// carries no standalone key - every hex or base64 blob of key length is a
// BouncyCastle curve parameter - and the four app-owned GUID-shaped constants
// were all rejected by the decrypt oracle.
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
// constructor at RVA 0x3B680F4. That read said no session value reaches the
// password, and the wire says the sid reaches the key material anyway, so the
// sid must enter somewhere that disassembly did not cover: as the PBKDF2 salt
// the constructor picks up itself, or through one of those two arrays being
// filled in at runtime rather than being the constant it looks like. Either way
// the salt was never in the static candidate set the offline search swept,
// which is why that search could not have succeeded. Hooking the constructor
// and its encrypt/decrypt pair settles it, because the constructor logs the
// password, the derived key and the derived IV side by side.
//
// What the second armed run proved (2026-08-28): the class is shared
// -----------------------------------------------------------------
// All seven hooks installed and the cipher hooks worked exactly as designed -
// they printed passwords, derived keys, derived IVs and both sides of every
// transform. What they did not print was anything belonging to socket.io. This
// cipher type is not the socket codec's private property: the app also uses it
// for PlayerPrefs key obfuscation and for its on-disk config cache. One boot
// spent the entire 600-line budget in 2.15 seconds on 279 encrypts of
// PlayerPrefs keys (caller RVA 0x3B6C4D4), 312 decrypts of cached config JSON
// (caller RVA 0x24A4318) and three constructors belonging to neither socket -
// and the backend did not log the websocket open until 2.6 seconds after the
// budget was already spent. [emit] printed nothing for the same reason.
//
// Two of those three constructors are worth keeping in mind while reading a
// capture, because they prove the type is reused rather than reconfigured: both
// take a 40-byte password, and the two differ only in their first 20 bytes
// while sharing an identical 20-byte tail. The socket codec's own password is
// 32 bytes, matching the XOR of the two arrays above.
//
// Raising the budget is the wrong fix: at that rate the interesting lines
// arrive thousands of lines deep and logd starts dropping messages first. The
// codec has to be identified instead, which the accessor does for free - see
// the get_Extras section below. Its return value is the socket.io instance, so
// encrypt and decrypt log only when `this` matches, and the pointer starts out
// null so every pre-handshake caller is filtered out without needing to be
// recognised. The constructor deliberately stays ungated: it fires a handful of
// times per boot, it is the one place the password is visible, and it still
// records the socket codec's password even if the accessor hook fails.
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

// -- identifying the socket.io instance -------------------------------------
//
// The cipher type above is shared: PlayerPrefs key obfuscation and the on-disk
// config cache both build their own instances of it, and between them they
// produce hundreds of transforms per boot. Filtering by content cannot separate
// those from the handshake, but identity can, and SocketManager hands the right
// pointer over for free.
//
// dump2313.cs 422832: namespace BestHTTP.SocketIO, TypeDefIndex 10433.
//
//   private <kCodecClass> _extras;                     // 0xA0
//   internal <kCodecClass> get_Extras()                RVA 0x3ECF1F4
//
// That class declares exactly one get_Extras, so {namespace, class, name, 0} is
// unambiguous; the two other get_Extras in the dump return
// IDictionary<string, object> from unrelated types and this lookup cannot reach
// them. Both the emit path and the inbound path go through this accessor to
// reach the codec, so its return value identifies the socket.io instance for
// both directions.
constexpr const char* kSocketNs = "BestHTTP.SocketIO";
constexpr const char* kSocketClass = "SocketManager";
constexpr const char* kGetExtras = "get_Extras";

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
// get_Extras() : <kCodecClass>; no explicit arguments, so `this` is followed
// straight by MethodInfo*.
using GetExtrasFn = void* (*)(void* self, void* method);

inline EmitFn g_orig_emit = nullptr;
inline CodecFn g_orig_codec_shared = nullptr;
inline CodecFn g_orig_codec_unique = nullptr;
inline InboundFn g_orig_inbound = nullptr;
inline CipherCtorFn g_orig_cipher_ctor = nullptr;
inline CipherOpenFn g_orig_cipher_open = nullptr;
inline CipherSealFn g_orig_cipher_seal = nullptr;
inline GetExtrasFn g_orig_get_extras = nullptr;

// The socket.io codec, published by get_extras_hook the first time the managed
// side asks for it. It stays null until then, which is exactly the filter the
// encrypt and decrypt hooks need: every caller that runs before the handshake
// belongs to some other subsystem and is rejected without having to be
// recognised.
inline void* g_socket_codec = nullptr;

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

// Once encrypt and decrypt are filtered to the socket.io codec, a sealed
// handshake is a handful of lines, so this is headroom rather than a real
// limit. Raising it is not an alternative to that filter: the first attempt ran
// unfiltered and spent 600 lines in 2.15 seconds on other subsystems, 2.6
// seconds before the websocket even opened, and at that rate logd starts
// dropping messages long before anything interesting arrives. The margin is
// here so that the ungated constructor lines still land if the accessor hook
// ever fails to install.
constexpr uint64_t kMaxLines = 2000u;
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

// True only for the instance SocketManager handed out. Every other user of this
// cipher - PlayerPrefs key obfuscation, the on-disk config cache - is rejected
// by identity rather than by content, so nothing they do can bury the
// handshake.
inline bool is_socket_codec(void* self) {
    return self != nullptr && self == g_socket_codec;
}

// Publishes the socket.io codec the first time the managed side asks for it.
// The accessor builds it lazily, so on that first call the constructor below
// has already logged this instance's password by the time the pointer arrives
// here - which is why the constructor stays ungated.
//
// Reading the derived key and IV off the returned object is the same fail-closed
// pair the constructor uses: if Mono's protected state is reachable by name they
// land here in full, and the PBKDF2 salt stops mattering at all.
inline void* get_extras_hook(void* self, void* method) {
    void* codec = g_orig_get_extras != nullptr
                      ? g_orig_get_extras(self, method)
                      : nullptr;

    if (codec != nullptr && codec != g_socket_codec) {
        g_socket_codec = codec;
        if (budget()) {
            void* algorithm = read_field_ptr(codec, kAlgField);
            void* key = read_field_ptr(algorithm, kKeyValue);
            void* iv = read_field_ptr(algorithm, kIVValue);
            LOGI("23.1.3-sio[cipher] socket codec identified self=%p key=%s "
                 "iv=%s; encrypt and decrypt now log this instance only",
                 codec, byte_array_hex(key).c_str(),
                 byte_array_hex(iv).c_str());
        }
    }
    return codec;
}

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
    const bool ours = is_socket_codec(self);

    if (ours && budget()) {
        LOGI("23.1.3-sio[cipher] decrypt sealed bytes=%" PRId64 " hex=%s",
             array_length(input), byte_array_hex(input).c_str());
    }

    void* output = g_orig_cipher_open != nullptr
                       ? g_orig_cipher_open(self, input, method)
                       : nullptr;

    if (ours && budget()) {
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
    const bool ours = is_socket_codec(self);

    if (ours && budget()) {
        LOGI("23.1.3-sio[cipher] encrypt open bytes=%" PRId64
             " flag=%d text='%s'",
             array_length(input), flag ? 1 : 0,
             byte_array_text(input).c_str());
    }

    void* output = g_orig_cipher_seal != nullptr
                       ? g_orig_cipher_seal(self, input, flag, method)
                       : nullptr;

    if (ours && budget()) {
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

    // The accessor that tells the encrypt and decrypt hooks which instance is
    // the socket's. Without it they stay silent, because the cipher type is
    // shared with PlayerPrefs and the config cache.
    const bool get_extras =
        hook::install({kSocketNs, kSocketClass, kGetExtras, 0},
                      reinterpret_cast<void*>(&get_extras_hook),
                      reinterpret_cast<void**>(&g_orig_get_extras));

    g_installed = emit || shared || unique || inbound || cipher_ctor ||
                  cipher_open || cipher_seal || get_extras;

    LOGI("23.1.3-sio: codec trace armed (emit=%d codec-shared=%d codec-sio=%d "
         "inbound=%d cipher-ctor=%d cipher-open=%d cipher-seal=%d "
         "get-extras=%d); logging only, no packet is altered",
         emit ? 1 : 0, shared ? 1 : 0, unique ? 1 : 0, inbound ? 1 : 0,
         cipher_ctor ? 1 : 0, cipher_open ? 1 : 0, cipher_seal ? 1 : 0,
         get_extras ? 1 : 0);

    if (!get_extras) {
        LOGE("23.1.3-sio: SocketManager.get_Extras could not be hooked, so the "
             "socket codec cannot be told apart from the PlayerPrefs and "
             "config-cache instances; encrypt and decrypt stay silent and only "
             "the constructor lines arrive");
    }

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

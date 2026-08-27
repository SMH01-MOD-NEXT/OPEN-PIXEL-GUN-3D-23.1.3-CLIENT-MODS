#pragma once

// -----------------------------------------------------------------------------
// 23.1.3 (ARM64) thin client - WebSocket address trace
//
// The HTTP redirect (backend_redirect_2313.h) covers UnityWebRequest and WWW,
// and that is all the game's REST traffic. It is not all of its traffic: the
// live configuration arrives over a socket.io channel that never touches those
// two transports, so nothing in this port has ever seen its address.
//
// The evidence, from a boot log of the self-hosted run:
//
//   [ConfigSystem.OnWebS_ocketCallback] save incoming config: MessagePackTest
//     Rilisoft.WebSocket.WebSocketManager <- BestHTTP.SocketIO.Socket.OnPacket
//     <- BestHTTP.SocketIO.Transports.WebSocketTransport.OnBinary
//   ParseFullSlotConfig: versionDict not contains version 23.1.3
//   Start wait config response / End wait config response
//
// UpdatesChecker, TierMatchmakingController, the ads facade and the clan
// configuration are all fed from that one channel. Serving files under
// /pixelgun3d-config/ cannot replace it, because it is a different transport.
//
// What this component does, and deliberately does not do
// -----------------------------------------------------
// It only reports addresses. No rewriting happens here yet: the socket.io
// handshake shape (namespace, EIO version, polling-then-upgrade order) has to
// be known before a self-hosted endpoint can answer it, and guessing it would
// mean writing a server against an assumption. One boot with this trace gives
// the exact address the client builds, and the server is then written against
// a fact.
//
// Two independent surfaces are traced, because two different WebSockets exist
// in this build and they are easy to confuse:
//
//   1. Rilisoft.WebSocket.WebSocketManager - the game's own config/live
//      channel. Its address lives in one string field; the setter and the
//      getter of that field are hooked, so the address is reported whoever
//      sets or reads it.
//
//   2. ExitGames.Client.Photon.PeerBase.PepareWebSocketUrl - Photon's own
//      WebSocket address (the misspelling is upstream's). This is battle
//      transport, not the config channel; it is traced so that a Photon URL in
//      the log is never mistaken for the game's own.
//
// Obfuscated names
// ----------------
// The manager's members are renamed to CJK identifiers in this build. Those are
// the real metadata names, so they are matched literally. If a future build
// renames them again, the lookup fails closed: the trace reports that it could
// not attach and the boot continues untouched.
// -----------------------------------------------------------------------------

#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "hook.h"
#include "il2cpp.h"
#include "log.h"

namespace ws_trace_2313 {
namespace detail {

static_assert(sizeof(void*) == 8, "PG3D 23.1.3 target must be arm64-v8a");

constexpr const char* kManagerNs = "Rilisoft.WebSocket";
constexpr const char* kManagerClass = "WebSocketManager";
// Setter of the manager's address field (offset 0x48) and its getter.
constexpr const char* kManagerSetAddress = "\xE4\xB8\x82\xE4\xB8\x9B\xE4\xB8\x8B\xE4\xB8\x93\xE4\xB8\x93\xE4\xB8\x88\xE4\xB8\x94\xE4\xB8\x93\xE4\xB8\x84";
constexpr const char* kManagerGetAddress = "\xE4\xB8\x9C\xE4\xB8\x93\xE4\xB8\x99\xE4\xB8\x90\xE4\xB8\x9E\xE4\xB8\x93\xE4\xB8\x97\xE4\xB8\x86\xE4\xB8\x88";

constexpr const char* kPhotonNs = "ExitGames.Client.Photon";
constexpr const char* kPhotonClass = "PeerBase";
constexpr const char* kPhotonWsUrl = "PepareWebSocketUrl";

// Addresses are long, and the getter is polled every frame while the channel
// reconnects. Only new values are reported, so one boot yields one line per
// distinct address instead of thousands of identical ones.
constexpr size_t kMaxUrlLength = 4096u;

using SetStringFn = void (*)(void* self, void* value, void* method);
using GetStringFn = void* (*)(void* self, void* method);
using PhotonUrlFn = void* (*)(void* self, void* server, void* app_id,
                              void* custom, void* method);

inline SetStringFn g_orig_set_address = nullptr;
inline GetStringFn g_orig_get_address = nullptr;
inline PhotonUrlFn g_orig_photon_url = nullptr;

inline bool g_installed = false;
inline std::string g_last_config_address;
inline std::string g_last_photon_url;
inline uint64_t g_config_reports = 0u;
inline uint64_t g_photon_reports = 0u;

inline bool is_managed_string(void* candidate) {
    if (candidate == nullptr) return false;
    if (il2cpp::object_get_class == nullptr || il2cpp::class_get_name == nullptr) {
        return false;
    }
    void* klass = il2cpp::object_get_class(candidate);
    if (klass == nullptr) return false;
    const char* name = il2cpp::class_get_name(klass);
    return name != nullptr && std::strcmp(name, "String") == 0;
}

inline std::string read_string(void* managed) {
    if (!is_managed_string(managed)) return std::string();
    return il2cpp::to_utf8(managed, kMaxUrlLength);
}

// Reports an address once per distinct value. `remembered` keeps the previous
// value for this surface, so a channel that reconnects to the same address
// stays quiet and a change of address is always visible.
inline void report(const char* surface, const std::string& value,
                   std::string* remembered, uint64_t* counter) {
    if (value.empty() || remembered == nullptr || counter == nullptr) return;
    if (*remembered == value) return;
    *remembered = value;
    ++(*counter);
    LOGI("23.1.3-ws: %s (#%" PRIu64 ")\n  address: %s", surface, *counter,
         value.c_str());
}

inline void manager_set_address_hook(void* self, void* value, void* method) {
    report("the game set its config socket address", read_string(value),
           &g_last_config_address, &g_config_reports);
    if (g_orig_set_address != nullptr) g_orig_set_address(self, value, method);
}

inline void* manager_get_address_hook(void* self, void* method) {
    void* result =
        g_orig_get_address != nullptr ? g_orig_get_address(self, method) : nullptr;
    report("the game read its config socket address", read_string(result),
           &g_last_config_address, &g_config_reports);
    return result;
}

inline void* photon_url_hook(void* self, void* server, void* app_id,
                             void* custom, void* method) {
    void* result = g_orig_photon_url != nullptr
                       ? g_orig_photon_url(self, server, app_id, custom, method)
                       : nullptr;
    report("Photon built its own WebSocket address (battle transport, not the "
           "config channel)",
           read_string(result), &g_last_photon_url, &g_photon_reports);
    return result;
}

inline bool install() {
    if (g_installed) return true;

    int config_hooks = 0;
    if (hook::install({kManagerNs, kManagerClass, kManagerSetAddress, 1},
                      reinterpret_cast<void*>(&manager_set_address_hook),
                      reinterpret_cast<void**>(&g_orig_set_address))) {
        ++config_hooks;
    }
    if (hook::install({kManagerNs, kManagerClass, kManagerGetAddress, 0},
                      reinterpret_cast<void*>(&manager_get_address_hook),
                      reinterpret_cast<void**>(&g_orig_get_address))) {
        ++config_hooks;
    }

    int photon_hooks = 0;
    if (hook::install({kPhotonNs, kPhotonClass, kPhotonWsUrl, 3},
                      reinterpret_cast<void*>(&photon_url_hook),
                      reinterpret_cast<void**>(&g_orig_photon_url))) {
        ++photon_hooks;
    }

    if (config_hooks == 0 && photon_hooks == 0) {
        LOGE("23.1.3-ws: no WebSocket address surface could be attached, so the "
             "config channel's address stays unknown and only HTTP traffic can "
             "be self-hosted");
        return false;
    }
    if (config_hooks == 0) {
        LOGE("23.1.3-ws: Photon's WebSocket address is traced, but the game's "
             "own config channel is not; its members were probably renamed in "
             "this build");
    }

    g_installed = true;
    LOGI("23.1.3-ws: watching %d/2 config channel and %d/1 Photon address "
         "surface(s); addresses are reported, never rewritten",
         config_hooks, photon_hooks);
    return true;
}

} // namespace detail

// Reports the addresses of both WebSockets this build opens. Read-only: it
// changes no argument and no return value.
inline bool install_hooks() { return detail::install(); }

} // namespace ws_trace_2313

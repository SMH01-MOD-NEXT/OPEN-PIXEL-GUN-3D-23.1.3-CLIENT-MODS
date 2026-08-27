#pragma once

// -----------------------------------------------------------------------------
// 23.1.3 (ARM64) thin client - config socket redirect
//
// The HTTP redirect (backend_redirect_2313.h) covers UnityWebRequest and WWW,
// which is all of the game's REST traffic - but not all of its traffic. The
// live configuration arrives over a socket.io channel that runs on BestHTTP,
// a transport of its own that never touches those two entry points. Until this
// component existed, that channel was invisible to the port and left the
// device unaltered.
//
// The trace pass of this component reported the address the client builds:
//
//   23.1.3-ws: the game set its config socket address (#1)
//     address: https://server-v2.pixelgun3dserver.com/socket.io/
//
// and the log proved the channel really reached Rilisoft's server, decoding
// live packets through
//
//   BestHTTP.SocketIO.Transports.WebSocketTransport.OnBinary
//     -> Rilisoft.WebSocket.WebSocketManager.<dispatcher>
//     -> ConfigSystem.OnWebS_ocketCallback
//
// UpdatesChecker, TierMatchmakingController, the clan configuration and the
// ads facade are all fed from that one channel, which is why serving files
// under /pixelgun3d-config/ could never replace it and why the boot stalled
// waiting for a config that only the retired service could send.
//
// So the address is now rewritten, using the same convention as the HTTP
// redirect - the original host survives as the first path segment, so one
// server routes every retired service with no client-side table:
//
//   https://server-v2.pixelgun3dserver.com/socket.io/
//     -> http://<server>/@server-v2.pixelgun3dserver.com/socket.io/
//
// Why no identity in the query string
// -----------------------------------
// The HTTP redirect appends opg3d_player/opg3d_session to the URL. This one
// must not: socket.io owns the query string of this address and appends its
// own handshake parameters (EIO, transport, t, sid) to it. A pre-existing
// query would either be overwritten or produce a second '?'. The server
// identifies this channel from the handshake instead.
//
// Two independent surfaces, one of them read-only
// ----------------------------------------------
//   1. Rilisoft.WebSocket.WebSocketManager - the game's own config channel.
//      Its address lives in one string field (offset 0x48); the setter is
//      rewritten and the getter is only observed, which is enough to catch an
//      address that was stored before this hook was armed.
//
//   2. ExitGames.Client.Photon.PeerBase.PepareWebSocketUrl - Photon's own
//      WebSocket address (the misspelling is upstream's). This is battle
//      transport and is deliberately NOT rewritten: Photon owns that traffic
//      and it is not socket.io. It is reported only so that a Photon URL in
//      the log is never mistaken for the config channel's.
//
// Obfuscated names
// ----------------
// The manager's members are renamed to CJK identifiers in this build. Those
// are the real metadata names, so they are matched literally. If a future
// build renames them, the lookup fails closed: the component reports that it
// could not attach and the boot continues untouched.
// -----------------------------------------------------------------------------

#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "backend_client_2313.h"
#include "hook.h"
#include "il2cpp.h"
#include "log.h"

namespace ws_redirect_2313 {
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

// Host suffixes of the retired services, kept in step with the HTTP redirect.
constexpr const char* kBackendHosts[] = {
    "pixelgun3dserver.com",
    "pixelgun3d.com",
    "pixelgunserver.com",
    "lightmap.com",
};

constexpr size_t kMaxUrlLength = 4096u;
// The config channel is built early, so it is worth waiting out LAN discovery
// rather than letting the address leave for a host that cannot answer.
constexpr int kServerWaitMs = 2500;

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
inline uint64_t g_rewrites = 0u;
inline uint64_t g_config_reports = 0u;
inline uint64_t g_photon_reports = 0u;
inline uint64_t g_no_server = 0u;

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

inline bool starts_with_ci(const std::string& text, const char* prefix) {
    const size_t length = std::strlen(prefix);
    if (text.size() < length) return false;
    for (size_t i = 0u; i < length; ++i) {
        char left = text[i];
        char right = prefix[i];
        if (left >= 'A' && left <= 'Z') left = static_cast<char>(left + 32);
        if (right >= 'A' && right <= 'Z') right = static_cast<char>(right + 32);
        if (left != right) return false;
    }
    return true;
}

inline bool ends_with(const std::string& text, const char* suffix) {
    const size_t length = std::strlen(suffix);
    if (text.size() < length) return false;
    return text.compare(text.size() - length, length, suffix) == 0;
}

// Reports an address once per distinct value, so a channel that reconnects to
// the same address stays quiet while a change is always visible.
inline void report(const char* surface, const std::string& value,
                   std::string* remembered, uint64_t* counter) {
    if (value.empty() || remembered == nullptr || counter == nullptr) return;
    if (*remembered == value) return;
    *remembered = value;
    ++(*counter);
    LOGI("23.1.3-ws: %s (#%" PRIu64 ")\n  address: %s", surface, *counter,
         value.c_str());
}

// Rebuilds a socket.io address against the selected backend. Accepts the ws
// schemes as well as http(s): the game uses https here, but BestHTTP also
// accepts an explicit ws address and a future build might pass one.
inline bool rewrite_address(const std::string& url, std::string* out) {
    if (url.size() > kMaxUrlLength) return false;

    size_t host_start = 0u;
    if (starts_with_ci(url, "https://")) {
        host_start = 8u;
    } else if (starts_with_ci(url, "http://")) {
        host_start = 7u;
    } else if (starts_with_ci(url, "wss://")) {
        host_start = 6u;
    } else if (starts_with_ci(url, "ws://")) {
        host_start = 5u;
    } else {
        return false;
    }

    const size_t host_end = url.find('/', host_start);
    const size_t authority_end =
        host_end == std::string::npos ? url.size() : host_end;
    std::string authority = url.substr(host_start, authority_end - host_start);
    const std::string tail =
        host_end == std::string::npos ? std::string("/") : url.substr(host_end);

    const size_t at = authority.find('@');
    if (at != std::string::npos) authority = authority.substr(at + 1u);
    std::string host = authority;
    const size_t colon = host.find(':');
    if (colon != std::string::npos) host = host.substr(0u, colon);
    if (host.empty()) return false;

    bool known = false;
    for (const char* suffix : kBackendHosts) {
        if (ends_with(host, suffix)) {
            known = true;
            break;
        }
    }
    if (!known) return false;

    if (!backend_client_2313::has_server()) {
        backend_client_2313::wait_for_server(kServerWaitMs);
    }
    const std::string server = backend_client_2313::endpoint();
    if (server.empty()) {
        ++g_no_server;
        LOGE("23.1.3-ws: the config socket address %s was built before any "
             "backend was selected, so it stays pointed at the retired service "
             "and the live config will not arrive (#%" PRIu64 ")",
             url.c_str(), g_no_server);
        return false;
    }
    // The server never rewrites its own address into itself.
    if (host == server || starts_with_ci(server, host.c_str())) return false;

    // Plain http on purpose: socket.io derives its websocket scheme from this
    // one, so http yields ws and no certificate has to be trusted or faked.
    std::string rebuilt("http://");
    rebuilt += server;
    rebuilt += "/@";
    rebuilt += host;
    if (!tail.empty() && tail[0] != '/') rebuilt += '/';
    rebuilt += tail;

    // No query string is added here: socket.io owns this address's query and
    // appends EIO/transport/t/sid to whatever it is given.
    if (out != nullptr) *out = rebuilt;
    return true;
}

inline void manager_set_address_hook(void* self, void* value, void* method) {
    void* replacement = nullptr;
    const std::string url = read_string(value);
    std::string rewritten;
    if (!url.empty() && rewrite_address(url, &rewritten) &&
        il2cpp::string_new != nullptr) {
        replacement = il2cpp::string_new(rewritten.c_str());
        if (replacement != nullptr) {
            ++g_rewrites;
            g_last_config_address = rewritten;
            LOGI("23.1.3-ws: the config socket was pointed at the self-hosted "
                 "backend (#%" PRIu64 ")\n  game asked for: %s\n  sent instead "
                 "to: %s", g_rewrites, url.c_str(), rewritten.c_str());
        }
    }
    if (replacement == nullptr) {
        report("the game set its config socket address (left untouched)", url,
               &g_last_config_address, &g_config_reports);
    }
    if (g_orig_set_address != nullptr) {
        g_orig_set_address(self, replacement != nullptr ? replacement : value,
                          method);
    }
}

// Read-only: an address stored before this hook was armed would never pass
// through the setter, so the getter is watched to make that case visible.
inline void* manager_get_address_hook(void* self, void* method) {
    void* result =
        g_orig_get_address != nullptr ? g_orig_get_address(self, method) : nullptr;
    report("the game read its config socket address", read_string(result),
           &g_last_config_address, &g_config_reports);
    return result;
}

// Read-only by design: Photon owns its own transport.
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

    bool setter = hook::install({kManagerNs, kManagerClass, kManagerSetAddress, 1},
                                reinterpret_cast<void*>(&manager_set_address_hook),
                                reinterpret_cast<void**>(&g_orig_set_address));
    bool getter = hook::install({kManagerNs, kManagerClass, kManagerGetAddress, 0},
                                reinterpret_cast<void*>(&manager_get_address_hook),
                                reinterpret_cast<void**>(&g_orig_get_address));
    bool photon = hook::install({kPhotonNs, kPhotonClass, kPhotonWsUrl, 3},
                                reinterpret_cast<void*>(&photon_url_hook),
                                reinterpret_cast<void**>(&g_orig_photon_url));

    if (!setter) {
        LOGE("23.1.3-ws: the config socket's address setter could not be "
             "hooked, so the live config channel still leaves the device and "
             "no self-hosted server can answer it");
        if (!getter && !photon) return false;
    }

    g_installed = true;
    const std::string server = backend_client_2313::endpoint();
    LOGI("23.1.3-ws: config socket %s (setter=%d getter=%d), Photon address "
         "watched=%d, target %s",
         setter ? "redirected" : "only observed", setter ? 1 : 0,
         getter ? 1 : 0, photon ? 1 : 0,
         server.empty() ? "<no server selected yet>" : server.c_str());
    return setter;
}

} // namespace detail

// Points the game's socket.io config channel at the self-hosted backend and
// reports Photon's own WebSocket address without touching it. Must run after
// the IL2CPP metadata is available.
inline bool install_hooks() { return detail::install(); }

} // namespace ws_redirect_2313

#pragma once

// -----------------------------------------------------------------------------
// 23.1.3 (ARM64) thin client - transport redirect
//
// Every request the game aims at a retired Pixel Gun service is pointed at the
// self-hosted backend instead, by rewriting the URL string before the original
// method ever sees it:
//
//   https://server-v2.pixelgun3dserver.com/auth_v2/?id_player=1
//     -> http://<server>/@server-v2.pixelgun3dserver.com/auth_v2/?id_player=1
//        &opg3d_player=<id>&opg3d_session=<token>
//
// The original host is kept as the first path segment, so one server can tell
// the retired services apart without any client-side routing table, and adding
// a new service later needs no new build.
//
// Why the URL and not the response
// --------------------------------
// Synthesising responses would mean hooking every result member of every
// transport (isDone, downloadHandler.text, error, responseCode, ...) and
// keeping their states consistent. Rewriting the address keeps the game on its
// own real HTTP path: status codes, POST bodies, headers, retries and coroutine
// timing all behave as they did against the live service, and the only thing
// that changed is which socket answers. It also removes TLS from the picture:
// the rewritten address is plain http, so no certificate has to be trusted,
// pinned or faked.
//
// Identity travels in the query string, not in headers, because the URL is the
// only thing a hook on the constructor can rewrite: the game builds its own
// headers afterwards and this port does not own that dictionary.
//
// Overload safety
// ---------------
// UnityWebRequest overloads its constructors and helpers on (string) and (Uri),
// and IL2CPP metadata lookup by name plus argument count cannot tell those
// apart. So no argument is ever assumed to be a string: every candidate is
// type-checked against System.String first, and anything else is passed through
// untouched.
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

namespace backend_redirect_2313 {
namespace detail {

static_assert(sizeof(void*) == 8, "PG3D 23.1.3 target must be arm64-v8a");

constexpr const char* kNetworkingNs = "UnityEngine.Networking";
constexpr const char* kUnityNs = "UnityEngine";
constexpr const char* kWebRequest = "UnityWebRequest";
constexpr const char* kWww = "WWW";

// Host suffixes of the retired services, taken from the string literals in
// global-metadata.dat. Photon's own hosts are deliberately absent: Photon owns
// that traffic and it is not HTTP.
constexpr const char* kBackendHosts[] = {
    "pixelgun3dserver.com",
    "pixelgun3d.com",
    "pixelgunserver.com",
    "lightmap.com",
};

constexpr uint64_t kLogBurst = 24u;
constexpr uint64_t kLogPeriod = 64u;
constexpr size_t kMaxUrlLength = 4096u;

// Generated managed methods take their explicit arguments followed by
// MethodInfo*. Instance methods take `this` first.
using Ctor2Fn = void (*)(void* self, void* a, void* b, void* method);
using Ctor4Fn = void (*)(void* self, void* a, void* b, void* c, void* d,
                         void* method);
using SetUrlFn = void (*)(void* self, void* value, void* method);
using Static1Fn = void* (*)(void* a, void* method);
using Static2Fn = void* (*)(void* a, void* b, void* method);
using WwwCtor1Fn = void (*)(void* self, void* a, void* method);
using WwwCtor2Fn = void (*)(void* self, void* a, void* b, void* method);
using WwwCtor3Fn = void (*)(void* self, void* a, void* b, void* c, void* method);

inline Ctor2Fn g_orig_uwr_ctor2 = nullptr;
inline Ctor4Fn g_orig_uwr_ctor4 = nullptr;
inline SetUrlFn g_orig_uwr_set_url = nullptr;
inline Static1Fn g_orig_uwr_get = nullptr;
inline Static2Fn g_orig_uwr_post = nullptr;
inline WwwCtor1Fn g_orig_www_ctor1 = nullptr;
inline WwwCtor2Fn g_orig_www_ctor2 = nullptr;
inline WwwCtor3Fn g_orig_www_ctor3 = nullptr;

inline bool g_installed = false;
inline uint64_t g_rewrites = 0u;
inline uint64_t g_passthrough = 0u;
inline uint64_t g_non_string = 0u;
inline uint64_t g_no_server = 0u;

inline bool should_log(uint64_t counter) {
    return counter <= kLogBurst || (counter % kLogPeriod) == 0u;
}

// Only a real System.String may be read as one: the (Uri) overloads share the
// name and argument count of the (string) ones in metadata.
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

// Splits an absolute http(s) URL and rebuilds it against the selected backend.
inline bool rewrite_url(const std::string& url, std::string* out) {
    if (url.size() > kMaxUrlLength) return false;

    size_t host_start = 0u;
    if (starts_with_ci(url, "https://")) {
        host_start = 8u;
    } else if (starts_with_ci(url, "http://")) {
        host_start = 7u;
    } else {
        return false;
    }

    const size_t host_end = url.find('/', host_start);
    const size_t authority_end =
        host_end == std::string::npos ? url.size() : host_end;
    std::string authority = url.substr(host_start, authority_end - host_start);
    const std::string tail =
        host_end == std::string::npos ? std::string("/") : url.substr(host_end);

    // Drop credentials and the port, keep the bare host for matching.
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

    const std::string server = backend_client_2313::endpoint();
    if (server.empty()) {
        ++g_no_server;
        if (should_log(g_no_server)) {
            LOGE("23.1.3-redirect: %s was requested but no backend is selected "
                 "yet, so it goes to the retired service and will fail "
                 "(#%" PRIu64 ")", url.c_str(), g_no_server);
        }
        return false;
    }
    // The server never rewrites its own address into itself.
    if (host == server || starts_with_ci(server, host.c_str())) return false;

    std::string rebuilt("http://");
    rebuilt += server;
    rebuilt += "/@";
    rebuilt += host;
    if (!tail.empty() && tail[0] != '/') rebuilt += '/';
    rebuilt += tail;

    // Identity for the server: which account this request belongs to, and proof
    // that the session is live. The game's own id_player stays in the URL
    // untouched, so a handler can compare the two.
    const std::string player = backend_client_2313::player_id();
    const std::string session = backend_client_2313::session();
    if (!player.empty() && !session.empty()) {
        rebuilt += (rebuilt.find('?') == std::string::npos) ? '?' : '&';
        rebuilt += "opg3d_player=";
        rebuilt += player;
        rebuilt += "&opg3d_session=";
        rebuilt += session;
    }

    if (out != nullptr) *out = rebuilt;
    return true;
}

// Returns a replacement managed string, or nullptr when the argument has to be
// passed through unchanged.
inline void* redirect(void* candidate, const char* surface) {
    if (candidate == nullptr) return nullptr;
    if (!is_managed_string(candidate)) {
        ++g_non_string;
        if (should_log(g_non_string)) {
            LOGI("23.1.3-redirect: %s was called with a non-string address "
                 "(probably the Uri overload); it was passed through "
                 "(#%" PRIu64 ")", surface, g_non_string);
        }
        return nullptr;
    }

    const std::string url = il2cpp::to_utf8(candidate, kMaxUrlLength);
    std::string rewritten;
    if (!rewrite_url(url, &rewritten)) {
        ++g_passthrough;
        if (should_log(g_passthrough)) {
            LOGI("23.1.3-redirect: %s left untouched (#%" PRIu64 "): %s", surface,
                 g_passthrough, url.c_str());
        }
        return nullptr;
    }
    if (il2cpp::string_new == nullptr) return nullptr;
    void* managed = il2cpp::string_new(rewritten.c_str());
    if (managed == nullptr) return nullptr;

    ++g_rewrites;
    if (should_log(g_rewrites)) {
        LOGI("23.1.3-redirect: %s (#%" PRIu64 ")\n  game asked for: %s\n  sent "
             "instead to: %s", surface, g_rewrites, url.c_str(),
             rewritten.c_str());
    }
    return managed;
}

inline void uwr_ctor2_hook(void* self, void* url, void* method_name, void* method) {
    void* replacement = redirect(url, "UnityWebRequest(url, method)");
    if (g_orig_uwr_ctor2 != nullptr) {
        g_orig_uwr_ctor2(self, replacement != nullptr ? replacement : url,
                         method_name, method);
    }
}

inline void uwr_ctor4_hook(void* self, void* url, void* method_name,
                           void* download, void* upload, void* method) {
    void* replacement = redirect(url, "UnityWebRequest(url, method, dl, ul)");
    if (g_orig_uwr_ctor4 != nullptr) {
        g_orig_uwr_ctor4(self, replacement != nullptr ? replacement : url,
                         method_name, download, upload, method);
    }
}

inline void uwr_set_url_hook(void* self, void* value, void* method) {
    void* replacement = redirect(value, "UnityWebRequest.url setter");
    if (g_orig_uwr_set_url != nullptr) {
        g_orig_uwr_set_url(self, replacement != nullptr ? replacement : value,
                           method);
    }
}

inline void* uwr_get_hook(void* uri, void* method) {
    void* replacement = redirect(uri, "UnityWebRequest.Get");
    return g_orig_uwr_get != nullptr
               ? g_orig_uwr_get(replacement != nullptr ? replacement : uri, method)
               : nullptr;
}

inline void* uwr_post_hook(void* uri, void* form, void* method) {
    void* replacement = redirect(uri, "UnityWebRequest.Post");
    return g_orig_uwr_post != nullptr
               ? g_orig_uwr_post(replacement != nullptr ? replacement : uri, form,
                                 method)
               : nullptr;
}

inline void www_ctor1_hook(void* self, void* url, void* method) {
    void* replacement = redirect(url, "WWW(url)");
    if (g_orig_www_ctor1 != nullptr) {
        g_orig_www_ctor1(self, replacement != nullptr ? replacement : url, method);
    }
}

inline void www_ctor2_hook(void* self, void* url, void* form, void* method) {
    void* replacement = redirect(url, "WWW(url, form)");
    if (g_orig_www_ctor2 != nullptr) {
        g_orig_www_ctor2(self, replacement != nullptr ? replacement : url, form,
                         method);
    }
}

inline void www_ctor3_hook(void* self, void* url, void* data, void* headers,
                           void* method) {
    void* replacement = redirect(url, "WWW(url, data, headers)");
    if (g_orig_www_ctor3 != nullptr) {
        g_orig_www_ctor3(self, replacement != nullptr ? replacement : url, data,
                         headers, method);
    }
}

inline bool install() {
    if (g_installed) return true;

    int webrequest_hooks = 0;
    if (hook::install({kNetworkingNs, kWebRequest, ".ctor", 2},
                      reinterpret_cast<void*>(&uwr_ctor2_hook),
                      reinterpret_cast<void**>(&g_orig_uwr_ctor2))) {
        ++webrequest_hooks;
    }
    if (hook::install({kNetworkingNs, kWebRequest, ".ctor", 4},
                      reinterpret_cast<void*>(&uwr_ctor4_hook),
                      reinterpret_cast<void**>(&g_orig_uwr_ctor4))) {
        ++webrequest_hooks;
    }
    if (hook::install({kNetworkingNs, kWebRequest, "set_url", 1},
                      reinterpret_cast<void*>(&uwr_set_url_hook),
                      reinterpret_cast<void**>(&g_orig_uwr_set_url))) {
        ++webrequest_hooks;
    }
    if (hook::install({kNetworkingNs, kWebRequest, "Get", 1},
                      reinterpret_cast<void*>(&uwr_get_hook),
                      reinterpret_cast<void**>(&g_orig_uwr_get))) {
        ++webrequest_hooks;
    }
    if (hook::install({kNetworkingNs, kWebRequest, "Post", 2},
                      reinterpret_cast<void*>(&uwr_post_hook),
                      reinterpret_cast<void**>(&g_orig_uwr_post))) {
        ++webrequest_hooks;
    }

    int www_hooks = 0;
    if (hook::install({kUnityNs, kWww, ".ctor", 1},
                      reinterpret_cast<void*>(&www_ctor1_hook),
                      reinterpret_cast<void**>(&g_orig_www_ctor1))) {
        ++www_hooks;
    }
    if (hook::install({kUnityNs, kWww, ".ctor", 2},
                      reinterpret_cast<void*>(&www_ctor2_hook),
                      reinterpret_cast<void**>(&g_orig_www_ctor2))) {
        ++www_hooks;
    }
    if (hook::install({kUnityNs, kWww, ".ctor", 3},
                      reinterpret_cast<void*>(&www_ctor3_hook),
                      reinterpret_cast<void**>(&g_orig_www_ctor3))) {
        ++www_hooks;
    }

    if (webrequest_hooks == 0 && www_hooks == 0) {
        LOGE("23.1.3-redirect: neither UnityWebRequest nor WWW could be hooked; "
             "the game's traffic still leaves the device and no self-hosted "
             "server can answer it");
        return false;
    }

    g_installed = true;
    const std::string server = backend_client_2313::endpoint();
    LOGI("23.1.3-redirect: armed - %d/5 UnityWebRequest and %d/3 WWW entry "
         "point(s) rewritten towards %s",
         webrequest_hooks, www_hooks,
         server.empty() ? "<no server selected yet>" : server.c_str());
    return true;
}

} // namespace detail

// Redirects the game's HTTP transports at the self-hosted backend. Must run
// after the IL2CPP metadata is available. Safe to install before the backend
// handshake finishes: until a server is known, requests are passed through and
// loudly logged.
inline bool install_hooks() { return detail::install(); }

} // namespace backend_redirect_2313

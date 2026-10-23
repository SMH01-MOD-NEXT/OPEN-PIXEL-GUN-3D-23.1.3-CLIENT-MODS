#pragma once

// -----------------------------------------------------------------------------
// 23.1.3 (ARM64) thin client - the only backend this library still contains
//
// Nothing is emulated in-process any more. This module finds the self-hosted
// Open Pixel Gun 3D backend, registers the device with it, and keeps the
// session and the sealed PUN credential that the rest of the port needs.
// Everything else (accounts, bans, config,
// progression, telemetry) belongs to the server.
//
// How the server is chosen, in order:
//   1. OPG3D_SERVER in the environment (adb shell setprop is not needed; this
//      is meant for scripted runs);
//   2. the external, user-writable file
//        /sdcard/Android/data/<package>/files/opg3d-server.txt
//      which a player can edit with any file manager, no root required;
//   3. the server remembered from the previous successful session;
//   4. UDP discovery on the local network (probe "OPG3D-DISCOVER-V1");
//   5. the compile-time default OPG3D_DEFAULT_SERVER, normally empty.
//
// A discovered or manually entered address is remembered, so the next launch
// starts at step 3 and is instant.
//
// This is deliberately the plumbing layer only. The on-screen server picker
// belongs on the managed side and lands next; it will write the same file this
// module already reads, so both halves stay independent and either can ship
// first.
//
// Logging
// -------
// Every request and response is logged with method, URL, status, timing, body
// size and a bounded body preview, because the whole point of self-hosting is
// to see exactly what the game sent, what came back, and what it expected when
// it fell over. Bearer tokens are never printed in full.
// -----------------------------------------------------------------------------

#include <atomic>
#include <cerrno>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "config.h"
#include "crypto_2313.h"
#include "log.h"

namespace backend_client_2313 {
namespace detail {

static_assert(sizeof(void*) == 8, "PG3D 23.1.3 target must be arm64-v8a");

constexpr const char* kApiPrefix = "/api/v1";
constexpr const char* kDiscoveryProbe = "OPG3D-DISCOVER-V1";
constexpr int kDiscoveryPort = 47318;
constexpr int kDiscoveryWaitMs = 1500;
constexpr int kDiscoveryAttempts = 3;
constexpr int kSocketTimeoutMs = 6000;
constexpr size_t kMaxResponseBytes = 1u << 20;
constexpr size_t kBodyPreview = 900u;
constexpr int kDefaultPort = 8080;
// The search does not give up after the first sweep: the server is often started
// after the game, and the player may write the manual file while the loading
// screen is up. Retrying costs one broadcast every few seconds and saves a
// restart every time.
constexpr int kRetryDelayMs = 5000;
constexpr int kRetryRounds = 60;   // ~5 minutes of looking

inline std::atomic<bool> g_search_hint_logged{false};

inline pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
inline pthread_cond_t g_ready_signal = PTHREAD_COND_INITIALIZER;

inline std::string g_package;
inline std::string g_dir;
inline std::string g_external_config;
inline std::string g_host;
inline int g_port = kDefaultPort;
inline std::string g_device_id;
inline std::string g_player_id;
inline std::string g_session;
inline std::string g_diagnostics_token;
inline std::string g_appid_pun;
inline std::string g_source = "none";
inline bool g_started = false;
inline uint64_t g_requests = 0u;

struct Guard {
    Guard() { pthread_mutex_lock(&g_lock); }
    ~Guard() { pthread_mutex_unlock(&g_lock); }
    Guard(const Guard&) = delete;
    Guard& operator=(const Guard&) = delete;
};

// ------------------------------------------------------------------- files

inline std::string read_text_file(const std::string& path, size_t limit = 8192u) {
    const int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) return std::string();
    std::string content;
    char buffer[1024];
    ssize_t chunk;
    while ((chunk = read(fd, buffer, sizeof(buffer))) > 0) {
        content.append(buffer, static_cast<size_t>(chunk));
        if (content.size() >= limit) break;
    }
    close(fd);
    return content;
}

inline bool write_text_file(const std::string& path, const std::string& content) {
    const std::string temporary = path + ".tmp";
    const int fd = open(temporary.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
                        0600);
    if (fd < 0) {
        LOGW("23.1.3-client: cannot write %s (%s)", path.c_str(), std::strerror(errno));
        return false;
    }
    size_t written = 0u;
    bool ok = true;
    while (written < content.size()) {
        const ssize_t chunk = write(fd, content.data() + written,
                                    content.size() - written);
        if (chunk <= 0) { ok = false; break; }
        written += static_cast<size_t>(chunk);
    }
    if (ok) ok = fsync(fd) == 0;
    close(fd);
    if (!ok || rename(temporary.c_str(), path.c_str()) != 0) {
        unlink(temporary.c_str());
        return false;
    }
    return true;
}

inline std::string trim(const std::string& raw) {
    size_t begin = 0u;
    size_t end = raw.size();
    while (begin < end && (raw[begin] == ' ' || raw[begin] == '\t' ||
                           raw[begin] == '\r' || raw[begin] == '\n')) {
        ++begin;
    }
    while (end > begin && (raw[end - 1u] == ' ' || raw[end - 1u] == '\t' ||
                           raw[end - 1u] == '\r' || raw[end - 1u] == '\n')) {
        --end;
    }
    return raw.substr(begin, end - begin);
}

// First non-empty, non-comment line of a config file.
inline std::string first_config_line(const std::string& content) {
    size_t offset = 0u;
    while (offset <= content.size()) {
        const size_t newline = content.find('\n', offset);
        const std::string line = trim(content.substr(
            offset, newline == std::string::npos ? std::string::npos : newline - offset));
        if (!line.empty() && line[0] != '#') return line;
        if (newline == std::string::npos) break;
        offset = newline + 1u;
    }
    return std::string();
}

inline std::string package_name() {
    const std::string cmdline = read_text_file("/proc/self/cmdline", 512u);
    std::string name;
    for (const char c : cmdline) {
        if (c == '\0' || c == ':' || c == '\n') break;
        name.push_back(c);
    }
    return name;
}

inline void resolve_paths() {
    g_package = package_name();
    if (g_package.empty()) g_package = "com.pixel.gun3d";

    // This runs from the ELF constructor, long before the game calls
    // getFilesDir(), so <data>/files usually does not exist yet. Requiring it to
    // exist sent the state to /data/local/tmp, which an app cannot write on a
    // modern Android: create the tree instead, and accept a candidate only once
    // a real write succeeds there.
    const char* candidates[] = {"/data/user/0/", "/data/data/"};
    for (const char* root : candidates) {
        const std::string base = std::string(root) + g_package;
        struct stat info{};
        if (stat(base.c_str(), &info) != 0 || !S_ISDIR(info.st_mode)) continue;
        const std::string files = base + "/files";
        mkdir(files.c_str(), 0771);
        const std::string dir = files + "/opg3d";
        mkdir(dir.c_str(), 0700);
        const std::string probe = dir + "/.writable";
        const int fd = open(probe.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
        if (fd < 0) {
            LOGW("23.1.3-client: %s is not writable (%s)", dir.c_str(),
                 std::strerror(errno));
            continue;
        }
        close(fd);
        unlink(probe.c_str());
        g_dir = dir;
        break;
    }
    if (g_dir.empty()) {
        // Last resort. Only reachable on a rooted or otherwise unusual device;
        // if it fails too, the client still runs, it just forgets its device id
        // between launches.
        g_dir = "/data/local/tmp/opg3d";
        mkdir(g_dir.c_str(), 0700);
        LOGW("23.1.3-client: no writable app directory; falling back to %s",
             g_dir.c_str());
    }

    const char* external = getenv("EXTERNAL_STORAGE");
    const std::string storage = (external != nullptr && *external != '\0')
                                    ? std::string(external)
                                    : std::string("/storage/emulated/0");
    g_external_config = storage + "/Android/data/" + g_package +
                        "/files/opg3d-server.txt";

    LOGI("23.1.3-client: package=%s state_dir=%s manual_server_file=%s",
         g_package.c_str(), g_dir.c_str(), g_external_config.c_str());
}

inline std::string random_hex(size_t bytes) {
    std::string out;
    const int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    for (size_t i = 0u; i < bytes; ++i) {
        uint8_t value = 0u;
        if (fd < 0 || read(fd, &value, 1u) != 1) {
            value = static_cast<uint8_t>((i * 37u) ^ (getpid() & 0xffu));
        }
        char pair[3];
        std::snprintf(pair, sizeof(pair), "%02x", value);
        out += pair;
    }
    if (fd >= 0) close(fd);
    return out;
}

// A stable per-install identity. The game's own account id lives in an
// encrypted pref this port cannot read, so the device id is what ties a player
// to their server-side account and to bans.
inline void load_device_id() {
    const std::string path = g_dir + "/device.txt";
    g_device_id = trim(read_text_file(path, 256u));
    if (g_device_id.size() >= 8u) return;
    g_device_id = random_hex(16u);
    if (write_text_file(path, g_device_id + "\n")) {
        LOGI("23.1.3-client: minted a new device id (%s)", g_device_id.c_str());
    } else {
        LOGW("23.1.3-client: the device id could not be persisted; the server "
             "will see a new account on the next launch");
    }
}

// ------------------------------------------------------------------- JSON
//
// The responses this module consumes are flat objects produced by our own
// server, so a scanner for "key": <value> is enough and cannot be surprised by
// nesting we do not ask for. Anything unparsable is logged verbatim instead of
// being guessed at.

inline size_t find_key(const std::string& body, const std::string& key,
                       size_t from = 0u) {
    const std::string needle = "\"" + key + "\"";
    size_t at = body.find(needle, from);
    if (at == std::string::npos) return std::string::npos;
    at += needle.size();
    while (at < body.size() && (body[at] == ' ' || body[at] == ':')) ++at;
    return at;
}

inline bool json_string(const std::string& body, const std::string& key,
                        std::string* out, size_t from = 0u) {
    const size_t at = find_key(body, key, from);
    if (at == std::string::npos || at >= body.size() || body[at] != '"') return false;
    std::string value;
    for (size_t i = at + 1u; i < body.size(); ++i) {
        const char c = body[i];
        if (c == '\\' && i + 1u < body.size()) {
            ++i;
            switch (body[i]) {
                case 'n': value.push_back('\n'); break;
                case 'r': value.push_back('\r'); break;
                case 't': value.push_back('\t'); break;
                default: value.push_back(body[i]); break;
            }
            continue;
        }
        if (c == '"') {
            if (out != nullptr) *out = value;
            return true;
        }
        value.push_back(c);
    }
    return false;
}

inline bool json_number(const std::string& body, const std::string& key,
                        long long* out) {
    const size_t at = find_key(body, key);
    if (at == std::string::npos || at >= body.size()) return false;
    char* end = nullptr;
    const long long value = std::strtoll(body.c_str() + at, &end, 10);
    if (end == body.c_str() + at) return false;
    if (out != nullptr) *out = value;
    return true;
}

inline bool json_bool(const std::string& body, const std::string& key, bool* out) {
    const size_t at = find_key(body, key);
    if (at == std::string::npos) return false;
    if (body.compare(at, 4u, "true") == 0) { if (out) *out = true; return true; }
    if (body.compare(at, 5u, "false") == 0) { if (out) *out = false; return true; }
    return false;
}

// --------------------------------------------------------------- transport

struct Response {
    bool transport_ok = false;
    int status = 0;
    std::string body;
};

inline std::string preview(const std::string& body) {
    std::string shown = body.substr(0u, kBodyPreview);
    for (char& c : shown) {
        if (c == '\n' || c == '\r' || c == '\t') c = ' ';
    }
    if (body.size() > kBodyPreview) shown += "...<truncated>";
    return shown;
}

inline std::string masked(const std::string& secret) {
    if (secret.size() <= 8u) return std::string("<set>");
    return secret.substr(0u, 6u) + "..." + secret.substr(secret.size() - 4u);
}

inline int connect_to(const std::string& host, int port) {
    char service[16];
    std::snprintf(service, sizeof(service), "%d", port);

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* list = nullptr;
    const int rc = getaddrinfo(host.c_str(), service, &hints, &list);
    if (rc != 0 || list == nullptr) {
        LOGE("23.1.3-client: cannot resolve %s:%d (%s)", host.c_str(), port,
             gai_strerror(rc));
        return -1;
    }

    int fd = -1;
    for (addrinfo* it = list; it != nullptr; it = it->ai_next) {
        fd = socket(it->ai_family, it->ai_socktype | SOCK_CLOEXEC, it->ai_protocol);
        if (fd < 0) continue;
        timeval timeout{};
        timeout.tv_sec = kSocketTimeoutMs / 1000;
        timeout.tv_usec = (kSocketTimeoutMs % 1000) * 1000;
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        const int one = 1;
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
        if (connect(fd, it->ai_addr, it->ai_addrlen) == 0) break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(list);
    if (fd < 0) {
        LOGE("23.1.3-client: cannot reach %s:%d (%s)", host.c_str(), port,
             std::strerror(errno));
    }
    return fd;
}

inline Response request(const char* method, const std::string& path,
                       const std::string& body, const char* content_type,
                       bool authenticated, const std::string& expected_endpoint = {},
                       const std::string& expected_player = {}) {
    Response response;
    std::string host;
    int port = 0;
    bool scoped_diagnostics = false;
    std::string session;
    std::string player;
    std::string device;
    uint64_t number = 0u;
    {
        Guard guard;
        if ((!expected_endpoint.empty() && expected_endpoint != g_host + ":" + std::to_string(g_port)) ||
            (!expected_player.empty() && expected_player != g_player_id)) return response;
        host = g_host;
        port = g_port;
        scoped_diagnostics = path == "/api/v1/client-errors" && !g_diagnostics_token.empty();
        session = scoped_diagnostics ? g_diagnostics_token : g_session;
        player = g_player_id;
        device = g_device_id;
        number = ++g_requests;
    }
    if (host.empty()) {
        LOGW("23.1.3-client: req#%" PRIu64 " %s %s dropped - no server selected",
             number, method, path.c_str());
        return response;
    }

    std::string head;
    head += std::string(method) + " " + path + " HTTP/1.1\r\n";
    head += "Host: " + host + ":" + std::to_string(port) + "\r\n";
    head += "User-Agent: opg3d-thin-client/23.1.3\r\n";
    head += "Connection: close\r\n";
    head += "Accept: application/json\r\n";
    if (!device.empty()) head += "X-OPG3D-Device: " + device + "\r\n";
    if (authenticated && !session.empty()) {
        if (scoped_diagnostics) head += "X-OPG3D-Diagnostics: " + session + "\r\n";
        else head += "Authorization: Bearer " + session + "\r\n";
        head += "X-OPG3D-Session: " + session + "\r\n";
    }
    if (authenticated && !player.empty()) {
        head += "X-OPG3D-Player: " + player + "\r\n";
    }
    if (!body.empty()) {
        head += std::string("Content-Type: ") +
                (content_type != nullptr ? content_type : "application/json") +
                "\r\n";
        head += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    }
    head += "\r\n";

    const uint64_t started = opg3d_log::monotonic_ms();
    LOGI("23.1.3-client: req#%" PRIu64 " -> %s %s http://%s:%d%s bytes=%zu "
         "session=%s body=%s",
         number, method, authenticated ? "(auth)" : "(anon)", host.c_str(), port,
         path.c_str(), body.size(),
         session.empty() ? "<none>" : (path=="/api/v1/client-errors" ? "<set>" : masked(session).c_str()),
         path=="/api/v1/client-errors" ? "<diagnostic body redacted>" : (body.empty() ? "<empty>" : preview(body).c_str()));

    const int fd = connect_to(host, port);
    if (fd < 0) return response;

    const std::string wire = head + body;
    size_t sent = 0u;
    while (sent < wire.size()) {
        const ssize_t chunk = send(fd, wire.data() + sent, wire.size() - sent, 0);
        if (chunk <= 0) {
            LOGE("23.1.3-client: req#%" PRIu64 " send failed after %zu/%zu bytes (%s)",
                 number, sent, wire.size(), std::strerror(errno));
            close(fd);
            return response;
        }
        sent += static_cast<size_t>(chunk);
    }

    std::string raw;
    char buffer[4096];
    ssize_t chunk;
    while ((chunk = recv(fd, buffer, sizeof(buffer), 0)) > 0) {
        raw.append(buffer, static_cast<size_t>(chunk));
        if (raw.size() > kMaxResponseBytes) break;
    }
    close(fd);

    const uint64_t elapsed = opg3d_log::monotonic_ms() - started;
    if (raw.empty()) {
        LOGE("23.1.3-client: req#%" PRIu64 " got no answer in %" PRIu64 "ms",
             number, elapsed);
        return response;
    }

    response.transport_ok = true;
    if (raw.compare(0u, 5u, "HTTP/") == 0) {
        const size_t space = raw.find(' ');
        if (space != std::string::npos) {
            response.status = std::atoi(raw.c_str() + space + 1u);
        }
    }
    const size_t split = raw.find("\r\n\r\n");
    if (split != std::string::npos) {
        response.body = raw.substr(split + 4u);
        // Chunked transfer is not used by our server, but a proxy in front of it
        // may add it; the leading size line would otherwise land in the body.
        if (raw.find("Transfer-Encoding: chunked") != std::string::npos) {
            const size_t first_newline = response.body.find("\r\n");
            if (first_newline != std::string::npos) {
                response.body = response.body.substr(first_newline + 2u);
                const size_t terminator = response.body.rfind("\r\n0\r\n");
                if (terminator != std::string::npos) {
                    response.body = response.body.substr(0u, terminator);
                }
            }
        }
    }

    LOGI("23.1.3-client: req#%" PRIu64 " <- status=%d ms=%" PRIu64
         " bytes=%zu body=%s",
         number, response.status, elapsed, response.body.size(),
         path == "/api/v1/accounts/register" ? "<credentials redacted>" : (response.body.empty() ? "<empty>" : preview(response.body).c_str()));
    return response;
}

// --------------------------------------------------------------- discovery

inline bool discover(std::string* host, int* port) {
    const int fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        LOGE("23.1.3-client: discovery socket failed (%s)", std::strerror(errno));
        return false;
    }
    const int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one));
    timeval timeout{};
    timeout.tv_sec = kDiscoveryWaitMs / 1000;
    timeout.tv_usec = (kDiscoveryWaitMs % 1000) * 1000;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    sockaddr_in target{};
    target.sin_family = AF_INET;
    target.sin_port = htons(static_cast<uint16_t>(kDiscoveryPort));
    target.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    for (int attempt = 1; attempt <= kDiscoveryAttempts; ++attempt) {
        const ssize_t sent =
            sendto(fd, kDiscoveryProbe, std::strlen(kDiscoveryProbe), 0,
                   reinterpret_cast<sockaddr*>(&target), sizeof(target));
        LOGI("23.1.3-client: discovery probe %d/%d sent to 255.255.255.255:%d "
             "(%zd bytes)",
             attempt, kDiscoveryAttempts, kDiscoveryPort, sent);
        if (sent < 0) {
            LOGW("23.1.3-client: discovery probe failed (%s)", std::strerror(errno));
            continue;
        }

        char buffer[1024];
        sockaddr_in from{};
        socklen_t from_size = sizeof(from);
        const ssize_t received =
            recvfrom(fd, buffer, sizeof(buffer) - 1u, 0,
                     reinterpret_cast<sockaddr*>(&from), &from_size);
        if (received <= 0) {
            LOGI("23.1.3-client: no server answered discovery attempt %d", attempt);
            continue;
        }
        buffer[received] = '\0';
        const std::string answer(buffer, static_cast<size_t>(received));

        char address[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &from.sin_addr, address, sizeof(address));
        std::string magic;
        std::string name;
        long long http_port = kDefaultPort;
        (void)json_string(answer, "magic", &magic);
        (void)json_string(answer, "name", &name);
        (void)json_number(answer, "http_port", &http_port);
        LOGI("23.1.3-client: discovery answer from %s: %s", address, answer.c_str());

        if (magic != kDiscoveryProbe) {
            LOGW("23.1.3-client: ignored a discovery answer with magic '%s'",
                 magic.c_str());
            continue;
        }
        if (host != nullptr) *host = address;
        if (port != nullptr) {
            *port = (http_port > 0 && http_port < 65536)
                        ? static_cast<int>(http_port)
                        : kDefaultPort;
        }
        LOGI("23.1.3-client: found '%s' on the local network at %s:%lld",
             name.c_str(), address, http_port);
        close(fd);
        return true;
    }
    close(fd);
    return false;
}

// --------------------------------------------------------------- selection

inline bool parse_endpoint(const std::string& text, std::string* host, int* port) {
    std::string value = trim(text);
    if (value.empty()) return false;
    if (value.compare(0u, 7u, "http://") == 0) value = value.substr(7u);
    else if (value.compare(0u, 8u, "https://") == 0) value = value.substr(8u);
    const size_t slash = value.find('/');
    if (slash != std::string::npos) value = value.substr(0u, slash);
    if (value.empty()) return false;

    int parsed = kDefaultPort;
    const size_t colon = value.rfind(':');
    if (colon != std::string::npos && value.find(']') == std::string::npos) {
        parsed = std::atoi(value.c_str() + colon + 1u);
        value = value.substr(0u, colon);
        if (parsed <= 0 || parsed >= 65536) parsed = kDefaultPort;
    }
    if (value.empty()) return false;
    if (host != nullptr) *host = value;
    if (port != nullptr) *port = parsed;
    return true;
}

inline void remember(const std::string& host, int port) {
    (void)write_text_file(g_dir + "/server.txt",
                          host + ":" + std::to_string(port) + "\n");
}

inline bool select_server() {
    std::string host;
    int port = kDefaultPort;
    const char* source = nullptr;

    const char* environment = getenv("OPG3D_SERVER");
    if (environment != nullptr && parse_endpoint(environment, &host, &port)) {
        source = "environment";
    }
    if (source == nullptr) {
        const std::string manual = first_config_line(read_text_file(g_external_config));
        if (!manual.empty() && parse_endpoint(manual, &host, &port)) {
            source = "manual file";
        }
    }
    if (source == nullptr) {
        const std::string saved = first_config_line(read_text_file(g_dir + "/server.txt"));
        if (!saved.empty() && parse_endpoint(saved, &host, &port)) {
            source = "remembered";
        }
    }
    if (source == nullptr && discover(&host, &port)) {
        source = "lan discovery";
    }
    if (source == nullptr && parse_endpoint(OPG3D_DEFAULT_SERVER, &host, &port)) {
        source = "build default";
    }

    if (source == nullptr) {
        // The hint is printed once. The search keeps running in the background,
        // so the server can be started (or the file written) while the game sits
        // on the loading screen - no restart needed.
        if (!g_search_hint_logged.exchange(true)) {
            LOGE("23.1.3-client: no backend found. Put \"host:port\" into %s, or "
                 "run the server on this network so discovery can see it. The "
                 "search repeats every %d s until one answers; restarting the "
                 "game is not required.",
                 g_external_config.c_str(), kRetryDelayMs / 1000);
        }
        return false;
    }

    {
        Guard guard;
        g_host = host;
        g_port = port;
        g_source = source;
    }
    LOGI("23.1.3-client: backend selected via %s -> %s:%d", source, host.c_str(),
         port);
    remember(host, port);
    return true;
}

// ------------------------------------------------------------------ session

inline std::string json_escape(const std::string& raw) {
    std::string out;
    for (const char c : raw) {
        if (c == '"' || c == '\\') { out.push_back('\\'); out.push_back(c); }
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else out.push_back(c);
    }
    return out;
}

inline bool handle_ban(const Response& response) {
    if (response.status != 403) return false;
    std::string error;
    (void)json_string(response.body, "error", &error);
    if (error != "banned") return false;
    std::string reason;
    std::string until;
    std::string kind;
    (void)json_string(response.body, "reason", &reason);
    (void)json_string(response.body, "until", &until);
    (void)json_string(response.body, "kind", &kind);
    LOGE("23.1.3-client: the server refused this device: banned kind=%s "
         "reason=%s until=%s",
         kind.c_str(), reason.empty() ? "<none>" : reason.c_str(),
         until.empty() ? "permanent" : until.c_str());
    return true;
}

inline bool register_device() {
    std::string device;
    {
        Guard guard;
        device = g_device_id;
    }
    std::string body = "{\"device_id\":\"" + json_escape(device) + "\"}";
    const Response response =
        request("POST", std::string(kApiPrefix) + "/accounts/register", body,
                "application/json", false);
    if (!response.transport_ok) return false;
    if (handle_ban(response)) return false;
    if (response.status != 200) {
        LOGE("23.1.3-client: registration refused with status %d", response.status);
        return false;
    }

    std::string player;
    std::string session;
    std::string diagnostics_token;
    std::string nick;
    if (!json_string(response.body, "player_id", &player) ||
        !json_string(response.body, "session", &session)) {
        LOGE("23.1.3-client: registration answer had no player_id/session; the "
             "server sent: %s", "<credentials redacted>");
        return false;
    }
    (void)json_string(response.body, "nick", &nick);
    (void)json_string(response.body, "diagnostics_token", &diagnostics_token);

    {
        Guard guard;
        g_player_id = player;
        g_session = session;
        g_diagnostics_token = diagnostics_token;
    }
    LOGI("23.1.3-client: signed in as player_id=%s nick=%s session=%s",
         player.c_str(), nick.empty() ? "<unset>" : nick.c_str(),
         masked(session).c_str());
    return true;
}

// -------------------------------------------------------- photon credentials

inline bool fetch_app_id(const char* mode, std::string* out) {
    std::string player;
    {
        Guard guard;
        player = g_player_id;
    }
    if (player.empty()) return false;

    const std::string path = std::string(kApiPrefix) + "/photon/appid?mode=" +
                             mode + "&player_id=" + player;
    const Response response = request("GET", path, std::string(), nullptr, true);
    if (!response.transport_ok) return false;
    if (handle_ban(response)) return false;
    if (response.status != 200) {
        LOGE("23.1.3-client: the server would not issue the %s AppID (status %d)",
             mode, response.status);
        return false;
    }

    std::string algorithm;
    std::string nonce_b64;
    std::string cipher_b64;
    std::string aad;
    const size_t envelope = find_key(response.body, "envelope");
    (void)json_string(response.body, "alg", &algorithm, envelope);
    if (!json_string(response.body, "nonce", &nonce_b64, envelope) ||
        !json_string(response.body, "ciphertext", &cipher_b64, envelope)) {
        LOGE("23.1.3-client: the %s AppID envelope is malformed: %s", mode,
             preview(response.body).c_str());
        return false;
    }
    (void)json_string(response.body, "aad", &aad, envelope);

    const std::string key_material(OPG3D_APPID_KEY);
    if (key_material.empty()) {
        LOGE("23.1.3-client: this build carries no OPG3D_APPID_KEY, so the %s "
             "AppID cannot be opened; rebuild with -POPG3D_APPID_KEY=<the "
             "server's OPG3D_APPID_KEY>", mode);
        return false;
    }

    uint8_t key[32];
    if (!opg3d_crypto::derive_key(key_material, key)) return false;

    std::string nonce;
    std::string ciphertext;
    if (!opg3d_crypto::base64_decode(nonce_b64, &nonce) ||
        !opg3d_crypto::base64_decode(cipher_b64, &ciphertext)) {
        LOGE("23.1.3-client: the %s AppID envelope is not valid base64", mode);
        return false;
    }

    // The server binds each envelope to "<player id>|<mode>"; recomputing it
    // locally means a swapped or replayed envelope fails the tag check instead
    // of connecting the player to the wrong application.
    const std::string expected_aad = player + "|" + mode;
    if (!aad.empty() && aad != expected_aad) {
        LOGE("23.1.3-client: the %s AppID envelope was issued for '%s' but this "
             "client is '%s'; refusing it", mode, aad.c_str(),
             expected_aad.c_str());
        return false;
    }

    std::string app_id;
    if (!opg3d_crypto::aes256gcm_open(key, nonce, ciphertext, expected_aad,
                                      &app_id)) {
        LOGE("23.1.3-client: the %s AppID envelope failed authentication "
             "(alg=%s aad=%s). The build key and the server's OPG3D_APPID_KEY "
             "do not match.",
             mode, algorithm.empty() ? "<unset>" : algorithm.c_str(),
             expected_aad.c_str());
        return false;
    }
    if (app_id.empty()) {
        LOGW("23.1.3-client: the server has no %s AppID configured", mode);
        return false;
    }

    if (out != nullptr) *out = app_id;
    LOGI("23.1.3-client: opened the %s AppID envelope (%zu chars, alg=%s)", mode,
         app_id.size(), algorithm.empty() ? "<unset>" : algorithm.c_str());
    return true;
}

// ------------------------------------------------------------------ startup

inline void handshake() {
    const Response ping = request("GET", std::string(kApiPrefix) + "/ping",
                                  std::string(), nullptr, false);
    if (!ping.transport_ok) {
        std::string host;
        int port = 0;
        {
            Guard guard;
            host = g_host;
            port = g_port;
        }
        LOGE("23.1.3-client: %s:%d did not answer /api/v1/ping. The game will "
             "keep running, every redirected request will fail, and the log "
             "above shows what it tried to send.",
             host.c_str(), port);
        return;
    }
    bool maintenance = false;
    if (json_bool(ping.body, "maintenance", &maintenance) && maintenance) {
        LOGW("23.1.3-client: the server reports maintenance mode");
    }

    if (!register_device()) return;
    std::string pun;
    if (fetch_app_id("pun", &pun)) {
        Guard guard;
        g_appid_pun = pun;
    }
    pthread_cond_broadcast(&g_ready_signal);
}

inline void* bootstrap(void*) {
    resolve_paths();
    load_device_id();
    for (int round = 1; round <= kRetryRounds; ++round) {
        if (select_server()) {
            handshake();
            bool ok = false;
            {
                Guard guard;
                ok = !g_session.empty();
            }
            if (ok) break;
            // The address answered discovery but not the API, or registration
            // failed. Forget it and look again rather than sitting on a server
            // that cannot serve.
            {
                Guard guard;
                g_host.clear();
                g_source = "none";
            }
        }
        pthread_cond_broadcast(&g_ready_signal);
        if (round == kRetryRounds) {
            LOGE("23.1.3-client: gave up looking for a backend after %d "
                 "attempts; restart the game once the server is reachable",
                 kRetryRounds);
            break;
        }
        usleep(static_cast<useconds_t>(kRetryDelayMs) * 1000u);
    }
    pthread_cond_broadcast(&g_ready_signal);
    return nullptr;
}

} // namespace detail

// Starts the backend handshake on its own thread. Called from the loader, so it
// must never block the game's startup: discovery alone can take a few seconds.
inline bool start() {
    {
        detail::Guard guard;
        if (detail::g_started) return true;
        detail::g_started = true;
    }
    pthread_t thread;
    if (pthread_create(&thread, nullptr, &detail::bootstrap, nullptr) != 0) {
        LOGE("23.1.3-client: cannot start the backend thread (%s); running the "
             "handshake inline instead", std::strerror(errno));
        detail::bootstrap(nullptr);
        return true;
    }
    pthread_detach(thread);
    LOGI("23.1.3-client: backend handshake started in the background");
    return true;
}

// "host:port" of the selected server, or an empty string while none is known.
inline std::string endpoint() {
    detail::Guard guard;
    if (detail::g_host.empty()) return std::string();
    return detail::g_host + ":" + std::to_string(detail::g_port);
}

inline bool has_server() {
    detail::Guard guard;
    return !detail::g_host.empty();
}

// The game fires its first analytics request about two seconds after launch,
// which can be sooner than discovery finishes. The redirect hook waits here for
// a short while so those early requests still reach the server instead of the
// retired host. Bounded on purpose: if no server exists, the game must keep
// loading rather than freeze on a hook.
inline bool wait_for_server(int timeout_ms) {
    if (has_server()) return true;
    const uint64_t deadline =
        opg3d_log::monotonic_ms() +
        static_cast<uint64_t>(timeout_ms > 0 ? timeout_ms : 0);
    while (opg3d_log::monotonic_ms() < deadline) {
        usleep(50u * 1000u);
        if (has_server()) return true;
    }
    return false;
}

inline std::string player_id() {
    detail::Guard guard;
    return detail::g_player_id;
}

inline std::string session() {
    detail::Guard guard;
    return detail::g_session;
}

// Blocks up to `timeout_ms` for the handshake, then hands back whatever the
// server issued. Photon's connect path calls this, and waiting there is safe:
// it already runs off a coroutine that tolerates a slow first frame.
inline std::string photon_app_id(int timeout_ms = 8000) {
    const uint64_t deadline = opg3d_log::monotonic_ms() +
                              static_cast<uint64_t>(timeout_ms > 0 ? timeout_ms : 0);
    for (;;) {
        {
            detail::Guard guard;
            if (!detail::g_appid_pun.empty()) return detail::g_appid_pun;
        }
        if (opg3d_log::monotonic_ms() >= deadline) break;
        usleep(50u * 1000u);
    }
    LOGW("23.1.3-client: the PUN AppID was still not available after %dms",
         timeout_ms);
    return std::string();
}


} // namespace backend_client_2313

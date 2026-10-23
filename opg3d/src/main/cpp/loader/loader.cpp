// SPDX-License-Identifier: GPL-3.0-or-later
#include "../backend_client_2313.h"
#include "../elf_sym.h"
#include "../obb_provisioner.h"
#include "jni_ui.h"
#include "policy.h"
#include "strings.h"
#include "ui_embedded.h"
#include <algorithm>
#include <android/dlext.h>
#include <atomic>
#include <condition_variable>
#include <dirent.h>
#include <link.h>
#include <mutex>
#include <poll.h>
#include <shadowhook.h>
#include <sys/random.h>
#include <thread>
#include <vector>
namespace {
void restore_loader_bridges();
}
namespace launcher {
namespace b = backend_client_2313::detail;
struct Server {
  std::string endpoint, name, country;
  bool compatible = false, online = false;
  std::string version;
};
std::mutex lock, work;
std::condition_variable gate;
std::vector<Server> servers;
std::string message = text::searching, dir, token;
std::atomic<bool> busy{true}, launched{false};
std::atomic<int> percent{0};
int listener = -1;
bool released = false;
void *payload = nullptr;
std::mutex ui_lock;
std::string ui_url;
void (*original_poll)(JNIEnv *, jobject, jlong, jint) = nullptr;
void poll_proxy(JNIEnv *e, jobject object, jlong self, jint timeout) {
  if (gettid() == getpid()) {
    std::string url;
    {
      std::lock_guard<std::mutex> g(ui_lock);
      url = ui_url;
    }
    if (!url.empty()) {
      e->PushLocalFrame(128);
      bool shown = show(e, url);
      e->PopLocalFrame(nullptr);
      if (shown) {
        std::lock_guard<std::mutex> g(ui_lock);
        ui_url.clear();
      }
      timeout = 0;
    }
  }
  original_poll(e, object, self, timeout);
}
bool (*original_init)(const char *) = nullptr;
void status(const char *s) {
  std::lock_guard<std::mutex> g(lock);
  message = s;
}
std::string quote(const std::string &s) {
  return "\"" + b::json_escape(s) + "\"";
}
std::string read(const std::string &p, size_t max = 32768) {
  return b::read_text_file(p, max);
}
bool write(const std::string &p, const std::string &data) {
  return b::write_text_file(p, data);
}
std::string hash(const std::string &data) {
  uint8_t h[32];
  opg3d_crypto::sha256(data.data(), data.size(), h);
  std::string s;
  for (auto c : h) {
    char v[3];
    snprintf(v, 3, "%02x", c);
    s += v;
  }
  return s;
}
bool endpoint(const std::string &raw, std::string *host, int *port) {
  return policy::endpoint(b::trim(raw), host, port);
}
bool sendall(int fd, const std::string &s) {
  size_t n = 0;
  while (n < s.size()) {
    auto k = send(fd, s.data() + n, s.size() - n, MSG_NOSIGNAL);
    if (k <= 0)
      return false;
    n += k;
  }
  return true;
}
std::string get(const std::string &ep, const std::string &path, size_t max) {
  std::string host;
  int port;
  if (!endpoint(ep, &host, &port))
    return {};
  int fd = b::connect_to(host, port);
  if (fd < 0)
    return {};
  if (!sendall(fd, "GET " + path + " HTTP/1.0\r\nHost: " + ep +
                       "\r\nConnection: close\r\n\r\n")) {
    close(fd);
    return {};
  }
  std::string data;
  char buf[16384];
  ssize_t n;
  bool bad = false;
  auto start = opg3d_log::monotonic_ms();
  while ((n = recv(fd, buf, sizeof(buf), 0)) > 0) {
    data.append(buf, n);
    if (data.size() > max + 16384 ||
        opg3d_log::monotonic_ms() - start > 60000) {
      bad = true;
      break;
    }
    percent = std::min(
        95, static_cast<int>(data.size() * 90 / std::max<size_t>(max, 1)));
  }
  close(fd);
  if (n < 0 || bad)
    return {};
  auto split = data.find("\r\n\r\n");
  if (split == std::string::npos || split > 16384 ||
      data.substr(0, 13).find(" 200 ") == std::string::npos)
    return {};
  auto head = data.substr(0, split);
  auto body = data.substr(split + 4);
  if (body.size() > max || head.find("Transfer-Encoding:") != std::string::npos)
    return {};
  return body;
}
std::string field(const std::string &j, const std::string &k) {
  std::string s;
  b::json_string(j, k, &s);
  return s;
}
long long number(const std::string &j, const std::string &k) {
  long long n = 0;
  b::json_number(j, k, &n);
  return n;
}
bool boolean(const std::string &j, const std::string &k) {
  bool v = false;
  b::json_bool(j, k, &v);
  return v;
}
void save_servers() {
  std::lock_guard<std::mutex> g(lock);
  std::string s;
  for (auto &v : servers)
    s += v.endpoint + "\n";
  write(dir + "/connections.txt", s);
}
bool probe(const std::string &raw, bool local) {
  std::string host;
  int port;
  if (!endpoint(raw, &host, &port)) {
    status(text::invalid);
    return false;
  }
  auto ep = host + ":" + std::to_string(port);
  auto body = get(ep, "/api/v1/loader/info", 16384);
  if (body.empty()) {
    status(text::unavailable);
    return false;
  }
  Server v;
  v.endpoint = ep;
  v.online = true;
  v.version = field(body, "game_version");
  v.name = field(body, "server_name");
  v.country = policy::is_official(ep) ? policy::official_country
              : local                 ? "Local"
                                      : field(body, "country");
  if (v.country.empty())
    v.country = "ZZ";
  v.compatible = number(body, "protocol") == 1 &&
                 field(body, "game_version") == "23.1.3" &&
                 field(body, "abi") == "arm64-v8a" &&
                 !boolean(body, "maintenance") &&
                 boolean(body, "release_available");
  {
    std::lock_guard<std::mutex> g(lock);
    auto it = std::find_if(servers.begin(), servers.end(),
                           [&](const Server &s) { return s.endpoint == ep; });
    if (it == servers.end()) {
      if (servers.size() < 32)
        servers.push_back(v);
    } else {
      if (it->country == "Local")
        v.country = "Local";
      *it = v;
    }
  }
  save_servers();
  status(v.compatible ? text::ready : text::incompatible);
  return v.compatible;
}
void discover() {
  int fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
  if (fd < 0)
    return;
  int one = 1;
  setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one));
  sockaddr_in dst{};
  dst.sin_family = AF_INET;
  dst.sin_port = htons(47318);
  dst.sin_addr.s_addr = INADDR_BROADCAST;
  sendto(fd, b::kDiscoveryProbe, strlen(b::kDiscoveryProbe), 0,
         reinterpret_cast<sockaddr *>(&dst), sizeof(dst));
  auto until = opg3d_log::monotonic_ms() + 1800;
  std::vector<std::string> found;
  while (opg3d_log::monotonic_ms() < until) {
    pollfd p{fd, POLLIN, 0};
    if (poll(&p, 1, 100) <= 0)
      continue;
    char buf[4096];
    sockaddr_in peer{};
    socklen_t size = sizeof(peer);
    auto n = recvfrom(fd, buf, sizeof(buf), 0,
                      reinterpret_cast<sockaddr *>(&peer), &size);
    if (n <= 0)
      continue;
    std::string j(buf, n);
    if (field(j, "magic") != b::kDiscoveryProbe)
      continue;
    auto port = number(j, "http_port");
    if (port < 1 || port > 65535)
      continue;
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof(ip));
    auto ep = std::string(ip) + ":" + std::to_string(port);
    if (std::find(found.begin(), found.end(), ep) == found.end())
      found.push_back(ep);
    if (found.size() >= 32)
      break;
  }
  close(fd);
  for (auto &ep : found)
    probe(ep, true);
}
void scan() {
  status(text::searching);
  // Seed the pinned official endpoint even on a fresh installation/offline
  // launch. Only this compiled endpoint receives the official UI badge.
  {
    std::lock_guard<std::mutex> g(lock);
    auto it = std::find_if(servers.begin(), servers.end(), [](const Server &v) {
      return policy::is_official(v.endpoint);
    });
    if (it == servers.end()) {
      Server v;
      v.endpoint = policy::official_endpoint;
      v.name = "Open Pixel Gun";
      v.country = policy::official_country;
      servers.insert(servers.begin(), v);
    }
  }
  auto saved = read(dir + "/connections.txt");
  size_t at = 0;
  int count = 0;
  while (at < saved.size() && count++ < 32) {
    auto end = saved.find('\n', at);
    auto ep = saved.substr(at, end - at);
    if (!ep.empty()) {
      std::string h;
      int p;
      if (endpoint(ep, &h, &p)) {
        {
          std::lock_guard<std::mutex> g(lock);
          auto it =
              std::find_if(servers.begin(), servers.end(),
                           [&](const Server &v) { return v.endpoint == ep; });
          if (it == servers.end()) {
            Server v;
            v.endpoint = ep;
            v.name = ep;
            v.country = "ZZ";
            if (servers.size() < 32)
              servers.push_back(v);
          } else {
            it->online = false;
            it->compatible = false;
          }
        }
        if (!policy::is_official(ep))
          probe(ep, false);
      }
    }
    if (end == std::string::npos)
      break;
    at = end + 1;
  }
  probe(policy::official_endpoint, false);
  discover();
  status(text::ready);
}
jbyteArray bytes(JNIEnv *e, const std::string &data) {
  auto a = e->NewByteArray(data.size());
  e->SetByteArrayRegion(a, 0, data.size(),
                        reinterpret_cast<const jbyte *>(data.data()));
  return a;
}
bool verify(const std::string &raw, const std::string &signature) {
  auto e = env();
  if (!e)
    return false;
  e->PushLocalFrame(40);
  std::string key, kh = OPG3D_RELEASE_KEY_HEX;
  if (kh.empty() || kh.size() % 2) {
    e->PopLocalFrame(nullptr);
    return false;
  }
  for (size_t i = 0; i < kh.size(); i += 2)
    key.push_back(
        static_cast<char>(strtol(kh.substr(i, 2).c_str(), nullptr, 16)));
  auto spec = e->FindClass("java/security/spec/X509EncodedKeySpec");
  auto ks = e->NewObject(spec, e->GetMethodID(spec, "<init>", "([B)V"),
                         bytes(e, key));
  auto factory = e->FindClass("java/security/KeyFactory");
  auto f = e->CallStaticObjectMethod(
      factory,
      e->GetStaticMethodID(factory, "getInstance",
                           "(Ljava/lang/String;)Ljava/security/KeyFactory;"),
      str(e, "RSA"));
  auto pk = e->CallObjectMethod(
      f,
      method(e, f, "generatePublic",
             "(Ljava/security/spec/KeySpec;)Ljava/security/PublicKey;"),
      ks);
  auto sc = e->FindClass("java/security/Signature");
  auto s = e->CallStaticObjectMethod(
      sc,
      e->GetStaticMethodID(sc, "getInstance",
                           "(Ljava/lang/String;)Ljava/security/Signature;"),
      str(e, "SHA256withRSA"));
  if (failed(e) || !pk || !s) {
    e->PopLocalFrame(nullptr);
    return false;
  }
  e->CallVoidMethod(
      s, method(e, s, "initVerify", "(Ljava/security/PublicKey;)V"), pk);
  e->CallVoidMethod(s, method(e, s, "update", "([B)V"), bytes(e, raw));
  bool ok = e->CallBooleanMethod(s, method(e, s, "verify", "([B)Z"),
                                 bytes(e, signature));
  if (failed(e))
    ok = false;
  e->PopLocalFrame(nullptr);
  return ok;
}
bool manifest(const std::string &envelope, std::string *raw) {
  std::string sig;
  return opg3d_crypto::base64_decode(field(envelope, "payload"), raw) &&
         opg3d_crypto::base64_decode(field(envelope, "signature"), &sig) &&
         verify(*raw, sig) && number(*raw, "schema") == 1 &&
         number(*raw, "loader_min") == 1 &&
         field(*raw, "game_version") == "23.1.3" &&
         field(*raw, "abi") == "arm64-v8a";
}
std::string candidate_sha;
long long candidate_build = 0;
void payload_ready() {
  std::lock_guard<std::mutex> g(lock);
  auto previous = b::trim(read(dir + "/active.txt", 128));
  if (previous != candidate_sha && !previous.empty())
    write(dir + "/previous.txt", previous);
  if (!write(dir + "/active.txt", candidate_sha) ||
      !write(dir + "/highest.txt", std::to_string(candidate_build)))
    return;
  unlink((dir + "/pending.txt").c_str());
  auto keep = b::trim(read(dir + "/previous.txt", 128));
  auto *d = opendir(dir.c_str());
  if (d) {
    while (auto *e = readdir(d)) {
      std::string n = e->d_name;
      if (n.size() != 67 && n.size() != 72)
        continue;
      auto h = n.substr(0, 64);
      if (h.find_first_not_of("0123456789abcdef") != std::string::npos ||
          h == candidate_sha || h == keep)
        continue;
      if (n.substr(64) == ".so" || n.substr(64) == ".so.json")
        unlink((dir + "/" + n).c_str());
    }
    closedir(d);
  }
  LOGI("loader: payload boot confirmed; build=%lld marked good",
       candidate_build);
}
void play(const std::string &ep) {
  if (!probe(ep, false))
    return;
  status(text::checking);
  auto envelope = get(ep, "/api/v1/frontend/manifest", 16384);
  std::string raw;
  if (!manifest(envelope, &raw)) {
    status(text::signature);
    return;
  }
  auto pending = b::trim(read(dir + "/pending.txt", 128));
  if (!pending.empty() && pending == field(raw, "sha256")) {
    auto good = b::trim(read(dir + "/active.txt", 128));
    std::string fallback;
    auto old = read(dir + "/" + good + ".so.json");
    if (good.size() != 64 || good == pending ||
        good.find_first_not_of("0123456789abcdef") != std::string::npos ||
        !manifest(old, &fallback) || field(fallback, "sha256") != good) {
      status(text::bootfail);
      return;
    }
    raw = fallback;
    envelope = old;
    LOGW("loader: previous boot failed; using signed last-known-good payload");
  }
  auto sha = field(raw, "sha256");
  auto size = number(raw, "size"), build = number(raw, "build");
  if (!policy::valid_hash(sha) || !policy::release_allowed(build, 0, size)) {
    status(text::signature);
    return;
  }
  auto highest = atoll(read(dir + "/highest.txt", 64).c_str());
  if (build < highest) {
    status(text::downgrade);
    return;
  }
  auto path = dir + "/" + sha + ".so";
  auto binary = read(path, size + 1);
  if (static_cast<long long>(binary.size()) == size && hash(binary) == sha) {
    status(text::cache);
    LOGI("loader: signed cached payload build=%lld", build);
  } else {
    status(text::downloading);
    binary = get(ep, "/api/v1/frontend/" + sha + "/library.so", size);
    if (static_cast<long long>(binary.size()) != size || hash(binary) != sha) {
      status(text::damaged);
      return;
    }
    if (!write(path, binary) || !write(path + ".json", envelope)) {
      status(text::storage);
      return;
    }
    LOGI("loader: verified new payload build=%lld bytes=%lld", build, size);
  }
  if (binary.substr(0, 4) != "\x7f"
                             "ELF" ||
      static_cast<unsigned char>(binary[4]) != 2 ||
      static_cast<unsigned char>(binary[18]) != 183) {
    status(text::damaged);
    return;
  }
  // Android 14+ requires dynamically loaded code to be read-only before
  // loading.
  if (chmod(path.c_str(), 0400) != 0) {
    status(text::storage);
    return;
  }
  restore_loader_bridges();
  status(text::loading);
  payload = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
  if (!payload) {
    LOGE("loader: dlopen rejected payload: %s", dlerror());
    status(text::loadfail);
    return;
  }
  auto abi = reinterpret_cast<int (*)()>(dlsym(payload, "opg3d_payload_abi"));
  auto start = reinterpret_cast<int (*)(const char *)>(
      dlsym(payload, "opg3d_payload_start"));
  auto set_ready = reinterpret_cast<void (*)(void (*)())>(
      dlsym(payload, "opg3d_payload_set_ready_callback"));
  if (!abi || abi() != 1 || !start || !set_ready) {
    status(text::loadfail);
    return;
  }
  candidate_sha = sha;
  candidate_build = build;
  set_ready(payload_ready);
  if (!write(dir + "/pending.txt", sha)) {
    status(text::storage);
    return;
  }
  if (!start(ep.c_str())) {
    unlink((dir + "/pending.txt").c_str());
    status(text::loadfail);
    return;
  }
  b::remember(ep.substr(0, ep.rfind(':')),
              atoi(ep.substr(ep.rfind(':') + 1).c_str()));
  {
    std::lock_guard<std::mutex> g(lock);
    released = true;
    launched = true;
    message = text::started;
  }
  percent = 100;
  gate.notify_all();
  dismiss();
  LOGI("loader: Play accepted; native game gate released build=%lld", build);
}
std::string state() {
  std::lock_guard<std::mutex> g(lock);
  std::string j =
      "{\"messageKey\":" + quote(message) +
      ",\"busy\":" + (busy ? "true" : "false") +
      ",\"percent\":" + std::to_string(percent.load()) +
      ",\"launched\":" + (launched ? "true" : "false") +
      ",\"preferred\":" + quote(b::trim(read(b::g_dir + "/server.txt", 512))) +
      ",\"servers\":[";
  bool first = true;
  for (auto &s : servers) {
    if (!first)
      j += ",";
    first = false;
    j += "{\"endpoint\":" + quote(s.endpoint) + ",\"name\":" + quote(s.name) +
         ",\"country\":" + quote(s.country) + ",\"official\":" +
         (policy::is_official(s.endpoint) ? "true" : "false") +
         ",\"version\":" + quote(s.version) +
         ",\"online\":" + (s.online ? "true" : "false") +
         ",\"compatible\":" + (s.compatible ? "true" : "false") + "}";
  }
  return j + "]}";
}
void job(const std::string &name, const std::string &body) {
  if (busy.exchange(true))
    return;
  std::thread([name, body] {
    std::lock_guard<std::mutex> g(work);
    percent = 0;
    if (name == "scan")
      scan();
    else if (name == "probe")
      probe(body, false);
    else if (name == "play")
      play(body);
    if (auto *current = vm.load())
      current->DetachCurrentThread();
    busy = false;
  }).detach();
}
void http() {
  while (!launched) {
    int fd = accept4(listener, nullptr, nullptr, SOCK_CLOEXEC);
    if (fd < 0)
      continue;
    timeval tv{2, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    std::string req;
    char buf[4096];
    size_t split = std::string::npos;
    while (req.size() < 8192 &&
           (split = req.find("\r\n\r\n")) == std::string::npos) {
      auto n = recv(fd, buf, sizeof(buf), 0);
      if (n <= 0)
        break;
      req.append(buf, n);
    }
    int code = 404;
    std::string body, ctype = "application/json";
    auto base = "/" + token + "/";
    if (split != std::string::npos) {
      auto end = req.find(' '), next = req.find(' ', end + 1);
      auto verb = req.substr(0, end),
           path = req.substr(end + 1, next - end - 1);
      if (path.compare(0, base.size(), base) == 0) {
        auto action = path.substr(base.size());
        if (verb == "GET" && action.empty()) {
          code = 200;
          body = html;
          ctype = "text/html; charset=utf-8";
        } else if (verb == "GET" && action == "status") {
          code = 200;
          body = state();
        } else if (verb == "POST" && (action == "scan" || action == "probe" ||
                                      action == "play")) {
          size_t len = 0;
          auto at = req.find("Content-Length:");
          if (at != std::string::npos)
            len = strtoul(req.c_str() + at + 15, nullptr, 10);
          if (len <= 512) {
            while (req.size() < split + 4 + len) {
              auto n = recv(fd, buf, sizeof(buf), 0);
              if (n <= 0)
                break;
              req.append(buf, n);
            }
            if (req.size() >= split + 4 + len) {
              job(action, req.substr(split + 4, len));
              code = 200;
              body = "{}";
            }
          }
        }
      }
    }
    auto head = "HTTP/1.1 " + std::to_string(code) +
                " OK\r\nContent-Type: " + ctype +
                "\r\nContent-Length: " + std::to_string(body.size()) +
                "\r\nConnection: close\r\nCache-Control: "
                "no-store\r\nX-Content-Type-Options: "
                "nosniff\r\nContent-Security-Policy: default-src 'none'; "
                "script-src 'unsafe-inline'; style-src 'unsafe-inline'; "
                "connect-src 'self'; img-src 'none'; form-action 'none'; "
                "frame-ancestors 'none'; base-uri 'none'\r\n\r\n";
    sendall(fd, head + body);
    close(fd);
  }
  close(listener);
}
void bootstrap() {
  b::resolve_paths();
  dir = b::g_dir + "/loader";
  if (mkdir(dir.c_str(), 0700) != 0 && errno != EEXIST) {
    LOGE("loader: private cache unavailable");
    return;
  }
  unsigned char secret[24];
  int rnd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
  if (rnd < 0 || ::read(rnd, secret, sizeof(secret)) != sizeof(secret)) {
    if (rnd >= 0)
      close(rnd);
    return;
  }
  close(rnd);
  for (auto v : secret) {
    char c[3];
    snprintf(c, 3, "%02x", v);
    token += c;
  }
  listener = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
  sockaddr_in local{};
  local.sin_family = AF_INET;
  local.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (bind(listener, reinterpret_cast<sockaddr *>(&local), sizeof(local)) !=
          0 ||
      listen(listener, 8) != 0)
    return;
  socklen_t n = sizeof(local);
  getsockname(listener, reinterpret_cast<sockaddr *>(&local), &n);
  auto url = "http://127.0.0.1:" + std::to_string(ntohs(local.sin_port)) + "/" +
             token + "/";
  std::thread(http).detach();
  std::thread([] {
    std::lock_guard<std::mutex> g(work);
    scan();
    busy = false;
  }).detach();
  {
    std::lock_guard<std::mutex> g(ui_lock);
    ui_url = url;
  }
  LOGI("loader: server picker queued on main Looper");
}
bool init_gate(const char *name) {
  LOGI("loader: game initialization blocked until Play");
  {
    std::unique_lock<std::mutex> g(lock);
    gate.wait(g, [] { return released; });
  }
  return original_init(name);
}
} // namespace launcher
namespace {
std::atomic<bool> gate_installed{false};
void *stub_dlopen = nullptr;
void *stub_android = nullptr;
void restore_loader_bridges() {
  if (stub_dlopen) {
    shadowhook_unhook(stub_dlopen);
    stub_dlopen = nullptr;
  }
  if (stub_android) {
    shadowhook_unhook(stub_android);
    stub_android = nullptr;
  }
  LOGI("loader: temporary dlopen bridges removed before game startup");
}
void *(*real_dlopen)(const char *, int) = nullptr;
void *(*real_android_dlopen)(const char *, int,
                             const android_dlextinfo *) = nullptr;
void install_gate() {
  if (gate_installed.load())
    return;
  auto target = elfsym::find_symbol("libil2cpp.so", "il2cpp_init");
  if (!target)
    return;
  bool expected = false;
  if (!gate_installed.compare_exchange_strong(expected, true))
    return;
  if (!shadowhook_hook_func_addr(
          target, reinterpret_cast<void *>(launcher::init_gate),
          reinterpret_cast<void **>(&launcher::original_init))) {
    LOGE("loader: cannot install mandatory game gate");
    _exit(126);
  }
  LOGI("loader: mandatory il2cpp_init gate installed");
}
void *open_proxy(const char *name, int flags) {
  auto result = real_dlopen(name, flags);
  install_gate();
  return result;
}
void *android_open_proxy(const char *name, int flags,
                         const android_dlextinfo *ext) {
  auto result = real_android_dlopen(name, flags, ext);
  install_gate();
  return result;
}
} // namespace
static void loader_load() {
  if (shadowhook_init(SHADOWHOOK_MODE_UNIQUE, false) != 0) {
    LOGE("loader: hook engine unavailable");
    _exit(126);
  }
  // Hook the registered JNI entry, NOT NativeMessageQueue::pollOnce:
  // optimized Android builds inline the latter in the actual JNI wrapper.
  void *poll = nullptr;
  dl_iterate_phdr(
      [](dl_phdr_info *info, size_t, void *out) -> int {
        if (!info->dlpi_name ||
            !strstr(info->dlpi_name, "/libandroid_runtime.so"))
          return 0;
        auto contains = [&](uintptr_t ptr, size_t n, int flags) {
          for (int i = 0; i < info->dlpi_phnum; i++) {
            auto &h = info->dlpi_phdr[i];
            auto base = info->dlpi_addr + h.p_vaddr;
            if (h.p_type == PT_LOAD &&
                (h.p_flags & flags) == static_cast<unsigned>(flags) &&
                ptr >= base && ptr <= base + h.p_memsz &&
                n <= base + h.p_memsz - ptr)
              return true;
          }
          return false;
        };
        for (int i = 0; i < info->dlpi_phnum; i++) {
          auto &h = info->dlpi_phdr[i];
          if (h.p_type != PT_LOAD || !(h.p_flags & PF_R) || (h.p_flags & PF_X))
            continue;
          auto begin = (info->dlpi_addr + h.p_vaddr + 7) & ~uintptr_t(7),
               end = info->dlpi_addr + h.p_vaddr + h.p_memsz;
          for (auto at = begin; at + 3 * sizeof(uintptr_t) <= end;
               at += sizeof(uintptr_t)) {
            auto *entry = reinterpret_cast<const uintptr_t *>(at);
            if (!contains(entry[0], 15, PF_R) ||
                memcmp(reinterpret_cast<const void *>(entry[0]),
                       "nativePollOnce", 15) != 0)
              continue;
            if (!contains(entry[1], 6, PF_R) ||
                memcmp(reinterpret_cast<const void *>(entry[1]), "(JI)V", 6) !=
                    0 ||
                !contains(entry[2], 4, PF_X))
              continue;
            *static_cast<void **>(out) = reinterpret_cast<void *>(entry[2]);
            return 1;
          }
        }
        return 1;
      },
      &poll);
  if (!poll || !shadowhook_hook_func_addr(
                   poll, reinterpret_cast<void *>(launcher::poll_proxy),
                   reinterpret_cast<void **>(&launcher::original_poll))) {
    LOGE("loader: main Looper JNI bridge unavailable");
    return;
  }
  LOGI("loader: main Looper JNI bridge installed");
  install_gate();
  if (!gate_installed) {
    auto a = dlsym(RTLD_DEFAULT, "dlopen"),
         b = dlsym(RTLD_DEFAULT, "android_dlopen_ext");
    if (!a || !b ||
        !(stub_dlopen = shadowhook_hook_func_addr(
              a, reinterpret_cast<void *>(open_proxy),
              reinterpret_cast<void **>(&real_dlopen))) ||
        !(stub_android = shadowhook_hook_func_addr(
              b, reinterpret_cast<void *>(android_open_proxy),
              reinterpret_cast<void **>(&real_android_dlopen)))) {
      LOGE("loader: early-load bridge failed");
      _exit(126);
    }
  }
  // Expansion provisioning must still precede Unity's asset lookup.
  (void)obb_framework_dir::prepare();
  (void)obb_provisioner::provision();
  std::thread(launcher::bootstrap).detach();
}
namespace {
struct Boot {
  Boot() { loader_load(); }
};
Boot boot;
} // namespace
extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *) {
  launcher::vm = vm;
  return JNI_VERSION_1_6;
}

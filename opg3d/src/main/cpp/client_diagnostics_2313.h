#pragma once
#include "backend_client_2313.h"
#include "client_diagnostics_core.h"
#include "client_diagnostics_sink.h"
#include "hook.h"
#include "il2cpp.h"
#include <atomic>
#include <pthread.h>
namespace client_diagnostics_2313 {
namespace detail {
using LogCallback = void (*)(void *, void *, int, bool, const void *);
using SetCallback = void (*)(bool, const void *);
inline LogCallback original_log = nullptr;
inline SetCallback original_set = nullptr;
inline client_diagnostics_core::Queue queue;
inline std::atomic<bool> enabled{false};
inline std::atomic<uint64_t> sequence{0};
inline std::string nonce;
inline void capture(const char *code, const std::string &message,
                    const std::string &stack, uint64_t revision = 0) {
  if (!enabled.load())
    return;
  client_diagnostics_core::Event e;
  e.id = nonce + "-" + std::to_string(++sequence);
  e.code = code;
  e.level = (e.code == "resync_started" || e.code == "resync_lobby_ready")
                ? "info"
                : "error";
  e.message = message;
  e.stack = stack;
  e.revision = revision;
  e.endpoint = backend_client_2313::endpoint();
  e.player = backend_client_2313::player_id();
  // Reports never migrate to another chosen server/account. Early auth reports
  // may bind to the first account on the already-selected server only.
  if (!e.endpoint.empty())
    queue.push(std::move(e), opg3d_log::monotonic_ms());
}
inline void emit(const char *code, const char *message, uint64_t revision) {
  capture(code, message, "", revision);
}
inline void callback(void *text, void *stack, int type, bool main,
                     const void *info) {
  // Unity LogType: Error=0, Assert=1, Warning=2, Log=3, Exception=4.
  if (enabled.load() && (type == 0 || type == 1 || type == 4)) {
    capture(type == 4   ? "unity_exception"
            : type == 1 ? "unity_assert"
                        : "unity_error",
            text ? il2cpp::to_utf8(text, 4096) : "Unity error without message",
            stack ? il2cpp::to_utf8(stack, 8192) : "");
  }
  if (original_log)
    original_log(text, stack, type, main, info);
}
inline void set_callback(bool value, const void *info) {
  // Keep the native callback enabled even when a stock subscriber unsubscribes.
  if (original_set)
    original_set(enabled.load() ? true : value, info);
}
inline void *worker(void *) {
  for (;;) {
    sleep(5);
    if (backend_client_2313::session().empty())
      continue;
    client_diagnostics_core::Event e;
    if (!queue.pop(e))
      continue;
    const auto endpoint = backend_client_2313::endpoint();
    const auto player = backend_client_2313::player_id();
    if (e.endpoint != endpoint || (!e.player.empty() && e.player != player))
      continue;
    e.player = player;
    using client_diagnostics_core::json;
    const std::string body =
        "{\"events\":[{\"id\":" + json(e.id) + ",\"code\":" + json(e.code) +
        ",\"level\":" + json(e.level) + ",\"message\":" + json(e.message) +
        ",\"stack\":" + json(e.stack) +
        ",\"revision\":" + std::to_string(e.revision) +
        ",\"version\":\"23.1.3\",\"module\":" +
        json(std::string("diagnostics-v1 ") + __DATE__ + " " + __TIME__) +
        "}]}";
    const auto result = backend_client_2313::detail::request(
        "POST", "/api/v1/client-errors", body, "application/json", true,
        e.endpoint, e.player);
    if (!result.transport_ok || result.status >= 500 || result.status == 429) {
      queue.retry(std::move(e));
      if (result.status == 429)
        sleep(60);
    }
  }
  return nullptr;
}
} // namespace detail
inline bool install() {
  using namespace detail;
  if (enabled.load())
    return true;
  const auto set_info = il2cpp::find_method_info("UnityEngine", "Application",
                                                 "SetLogCallbackDefined", 1);
  if (!set_info)
    return false;
  const bool a =
      hook::install({"UnityEngine", "Application", "CallLogCallback", 4},
                    reinterpret_cast<void *>(&callback),
                    reinterpret_cast<void **>(&original_log), true);
  const bool b =
      hook::install({"UnityEngine", "Application", "SetLogCallbackDefined", 1},
                    reinterpret_cast<void *>(&set_callback),
                    reinterpret_cast<void **>(&original_set), true);
  if (!a || !b || !original_log || !original_set)
    return false;
  nonce = backend_client_2313::detail::random_hex(12);
  pthread_t thread;
  if (pthread_create(&thread, nullptr, &worker, nullptr) != 0)
    return false;
  pthread_detach(thread);
  enabled = true;
  client_diagnostics::sink = &emit;
  original_set(true, set_info);
  LOGI("23.1.3-client-diagnostics: Unity errors enabled; bounded asynchronous "
       "delivery");
  return true;
}
} // namespace client_diagnostics_2313

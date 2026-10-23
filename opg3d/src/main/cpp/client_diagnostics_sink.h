#pragma once
#include <atomic>
#include <cstdint>
namespace client_diagnostics {
using Sink = void (*)(const char *, const char *, uint64_t);
inline std::atomic<Sink> sink{nullptr};
inline void emit(const char *code, const char *message, uint64_t revision = 0) {
  if (auto fn = sink.load())
    fn(code, message, revision);
}
} // namespace client_diagnostics

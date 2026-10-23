#include "../opg3d/src/main/cpp/client_diagnostics_core.h"
#include "../opg3d/src/main/cpp/client_diagnostics_sink.h"
#include <cassert>
#include <thread>
using namespace client_diagnostics_core;
int emissions = 0;
void emit(const char *, const char *, uint64_t revision) {
  assert(revision == 5);
  ++emissions;
}
int main() {
  assert(clean("Bearer abc-secret\nNullReferenceException: foo", 1024) ==
         "[sensitive line redacted]\nNullReferenceException: foo");
  assert(clean("at /data/user/file.cs https://example/a?k=x test@example.com",
               1024)
             .find("example") == std::string::npos);
  assert(clean(std::string(100, 'x'), 100) == "[opaque]");
  assert(clean("тест тест", 8) == "тест");
  assert(json("a\n\"b\\") == "\"a\\n\\\"b\\\\\"");
  Queue q;
  Event e;
  e.code = "unity_exception";
  e.message = "NullReferenceException: foo";
  e.endpoint = "server";
  e.player = "a";
  assert(q.push(e, 100));
  assert(!q.push(e, 101));
  assert(q.push(e, 30100));
  Event out;
  assert(q.pop(out));
  assert(out.message == e.message && out.endpoint == e.endpoint &&
         out.player == e.player);
  assert(q.pop(out));
  q.retry(out);
  assert(q.pop(out) && out.attempts == 1);
  q.retry(out);
  assert(q.pop(out) && out.attempts == 2);
  q.retry(out);
  assert(!q.pop(out));
  for (int i = 0; i < 32; i++) {
    e.message = "failure " + std::to_string(i);
    assert(q.push(e, 100000));
  }
  e.message = "overflow";
  assert(!q.push(e, 100000));
  assert(q.pending.size() == 32);
  client_diagnostics::emit("resync_started", "msg", 5);
  assert(emissions == 0);
  client_diagnostics::sink = &emit;
  client_diagnostics::emit("resync_started", "msg", 5);
  assert(emissions == 1);
}

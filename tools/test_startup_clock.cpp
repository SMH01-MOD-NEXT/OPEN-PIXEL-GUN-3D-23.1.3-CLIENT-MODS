#include <cassert>
#include "../opg3d/src/main/cpp/startup_clock_2313.h"
namespace il2cpp {
void* find_field(const char*, const char*, const char*) { return nullptr; }
void* find_method_info(const char*, const char*, const char*, int) { return nullptr; }
void* method_pointer(void*) { return nullptr; }
}
namespace hook {
bool install(const ManagedMethod&, void*, void**, bool) { return false; }
}
struct Clock { bool running; int elapsed; int starts; };
bool running(void* p, const void*) { return static_cast<Clock*>(p)->running; }
void resume(void* p, const void*) {
    auto& c = *static_cast<Clock*>(p); c.running = true; ++c.starts;
}
int calls = 0; int token = 0; bool return_null = false;
void* factory(void* self, const void* method) {
    assert(self == &token && method == &token); ++calls; return return_null ? nullptr : &token;
}
Clock c{false, 1234, 0};
void get_clock(void*, void* output) { *static_cast<void**>(output) = &c; }
int main() {
    using namespace startup_clock_2313::detail;
    assert(!resume_clock(nullptr, running, resume, nullptr, nullptr));
    assert(!resume_clock(&c, nullptr, resume, nullptr, nullptr));
    assert(!resume_clock(&c, running, nullptr, nullptr, nullptr));
    assert(resume_clock(&c, running, resume, nullptr, nullptr));
    assert(c.running && c.elapsed == 1234 && c.starts == 1);
    assert(!resume_clock(&c, running, resume, nullptr, nullptr));
    assert(c.starts == 1 && c.elapsed == 1234);
    original_initialize = factory; clock_field = &token;
    is_running = running; startup_clock_2313::detail::start = ::resume;
    il2cpp::field_static_get_value = get_clock;
    c.running = false;
    assert(initialize(&token, &token) == &token);
    assert(calls == 1 && c.starts == 2 && c.elapsed == 1234);
    assert(initialize(&token, &token) == &token);
    assert(calls == 2 && c.starts == 2); // Exactly one stock call per invocation.
    return_null = true; c.running = false;
    assert(initialize(&token, &token) == nullptr);
    assert(calls == 3 && c.starts == 2); // No clock action after a failed factory.
    return_null = false; il2cpp::field_static_get_value = nullptr;
    assert(initialize(&token, &token) == &token);
    assert(calls == 4 && c.starts == 2); // Missing API preserves stock return.
    original_initialize = nullptr;
    assert(initialize(&token, &token) == nullptr);
    assert(calls == 4 && c.starts == 2);
}

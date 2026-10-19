// Build with -std=c++17 -Wall -Wextra -Werror -Itools/test_support -ldl -pthread.
#include <cassert>
#include <map>
#include "../opg3d/src/main/cpp/post_match_observer_2313.h"

// Runtime metadata and hooks are unavailable on the host. These stubs also
// ensure the tested lifecycle never resolves or invokes a managed method.
namespace il2cpp {
void* find_method_info(const char*, const char*, const char*, int) { assert(false); return nullptr; }
void* method_pointer(void*) { assert(false); return nullptr; }
void* find_method_by_address(const char*, const char*, void*) { assert(false); return nullptr; }
void* find_class(const char*, const char*) { assert(false); return nullptr; }
std::string to_utf8(void*, size_t) { assert(false); return {}; }
}
namespace elfsym {
void* find_symbol(const char*, const char*) { assert(false); return nullptr; }
}
namespace hook {
bool install(const ManagedMethod&, void*, void**, bool) { assert(false); return false; }
}
namespace {
std::map<uint32_t, void*> handles;
uint32_t sequence = 0;
int allocations = 0, frees = 0, updates = 0;
uint32_t create(void* object, bool pinned) {
    assert(!pinned); // Strong reference, not a permanent pinned heap object.
    ++allocations;
    handles[++sequence] = object;
    return sequence;
}
void* target(uint32_t handle) { return handles.at(handle); }
void drop(uint32_t handle) { assert(handles.erase(handle) == 1); ++frees; }
void update(void*, const void*) { ++updates; }
int captures = 0, formats = 0;
void capture(void*, const void*) { ++captures; }
void format(void*, char* output, int capacity) {
    assert(capacity > 40);
    ++formats;
    std::strcpy(output, "ManagedException: test\nTestClass.Method()");
}
}
int main() {
    using namespace post_match_observer_2313;
    using namespace post_match_observer_2313::detail;
    int first = 1, second = 2;
    // Missing GC API fails closed, with no dereference of an iterator.
    observe_iterator(&first, true);
    assert(iterator_handle == 0 && allocations == 0);
    new_handle = create;
    get_target = target;
    free_handle = drop;
    observe_iterator(nullptr, true);
    assert(allocations == 0);
    observe_iterator(&first, true);
    assert(allocations == 1 && handles.size() == 1 && step_count == 1);
    observe_iterator(&first, true);
    assert(allocations == 1 && step_count == 2);
    // A second match must release the previous diagnostic reference.
    observe_iterator(&second, true);
    assert(allocations == 2 && frees == 1 && handles.size() == 1 && step_count == 1);
    observe_iterator(&first, false); // Completion of another iterator is irrelevant.
    assert(handles.size() == 1);
    observe_iterator(&second, false);
    assert(iterator_handle == 0 && handles.empty() && frees == 2);
    release(); // Repeated cleanup must not double-free.
    assert(frees == 2);
    // A stalled/closed scene cannot retain an iterator indefinitely.
    observe_iterator(&first, true);
    started_ms = now_ms() - 120001;
    heartbeat();
    assert(iterator_handle == 0 && handles.empty() && frees == 3);
    original_table_update = update;
    original_controller_update = update;
    table_update(nullptr, nullptr);
    controller_update(nullptr, nullptr);
    assert(updates == 2); // Always preserve both original frame callbacks.
    original_capture = capture;
    format_exception = format;
    format_stack = format;
    capture_exception(nullptr, nullptr);
    assert(captures == 1 && formats == 0); // Non-results throws pass through silently.
    result_scope = "test results";
    capture_exception(nullptr, nullptr);
    assert(captures == 2 && formats == 2 && !formatting && !result_scope);
    result_scope = "abandoned diagnostic scope";
    heartbeat();
    assert(!result_scope); // An unwound callback cannot leak scope into other frames.
}

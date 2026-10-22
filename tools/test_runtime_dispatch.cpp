// g++ -std=c++17 -Wall -Wextra -Werror -pthread -Itools/test_support tools/test_runtime_dispatch.cpp -o /tmp/test-runtime-dispatch
#include <cassert>
#include <cstring>
#include <thread>
#include <vector>
#include "../opg3d/src/main/cpp/runtime_dispatch_2313.h"
namespace {
using namespace runtime_dispatch_2313::detail;
std::atomic<int> jobs{0}, stock_calls{0};
std::thread::id job_thread;
bool allow_install = true;
int installs = 0;
void* self_arg = reinterpret_cast<void*>(0x1230);
const void* method_arg = reinterpret_cast<void*>(0x2340);
void stock(void* self, const void* method) {
    assert(self == self_arg && method == method_arg);
    assert(jobs == 1); // Initialization precedes the original frame's queue.
    ++stock_calls;
}
void work() {
    ++jobs; job_thread = std::this_thread::get_id();
    assert(!run_pending()); // Reentrant frame cannot run this job again.
}
}
namespace hook {
bool install(const ManagedMethod& target, void* replacement, void** out, bool required) {
    ++installs;
    assert(!std::strcmp(target.namespaze,"UnityEngine"));
    assert(!std::strcmp(target.klass,"UnitySynchronizationContext"));
    assert(!std::strcmp(target.method,"Exec") && target.args_count == 0 && required);
    assert(replacement == reinterpret_cast<void*>(&exec));
    if (!allow_install) return false;
    *out = reinterpret_cast<void*>(&stock);
    return true;
}
}
int main() {
    using namespace runtime_dispatch_2313;
    assert(!schedule(nullptr) && installs == 0);
    allow_install = false;
    assert(!schedule(&work));
    assert(!run_pending() && jobs == 0 && !scheduled);
    allow_install = true;
    std::thread worker([] { assert(schedule(&work)); });
    worker.join();
    assert(jobs == 0 && !schedule(&work)); // Worker only schedules.
    exec(self_arg, method_arg);
    assert(jobs == 1 && job_thread == std::this_thread::get_id() && stock_calls == 1);
    exec(self_arg, method_arg);
    assert(jobs == 1 && stock_calls == 2 && !run_pending());
    // Atomic consumption allows only one winner even with competing callers.
    pending.store(&work); jobs=0;
    std::vector<std::thread> contenders;
    for(int i=0;i<16;++i) contenders.emplace_back([] { run_pending(); });
    for(auto& thread:contenders) thread.join();
    assert(jobs == 1 && !pending.load());
}

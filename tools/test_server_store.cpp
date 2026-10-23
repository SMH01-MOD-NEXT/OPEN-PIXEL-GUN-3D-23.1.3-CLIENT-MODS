#include <cassert>
#include <thread>
#include <stdexcept>
#include "../opg3d/src/main/cpp/server_store_2313.h"
namespace hook { bool install(const ManagedMethod&, void*, void**, bool) { return false; } }
namespace {
using namespace server_store_2313::detail;
int object, config, method, calls = 0;
bool stock_result = false, seen = false, should_throw = false, yield_result = true;
bool stock_available(MI* info) {
    assert(info == &method); ++calls; return stock_result;
}
void stock_initialize(void* self, void* value, MI* info) {
    assert(self == &object && value == &config && info == &method);
    seen = available(info);
    if (should_throw) throw std::runtime_error("test");
}
void stock_refresh(void* self, MI* info) {
    assert(self == &object && info == &method); seen = available(info);
}
bool stock_load_step(void* self, MI* info) {
    stock_refresh(self, info); return yield_result;
}
void open() { initialize(&object, &config, &method); }
}
int main() {
    using namespace server_store_2313::detail;
    original_available = stock_available; original_initialize = stock_initialize;
    original_select = stock_initialize; original_refresh = stock_refresh;
    original_load_step = stock_load_step;
    assert(!server_store_2313::install_hooks()); // partial install fails closed
    open(); assert(!seen && calls == 1 && !catalog_scope);
    assert(load_step(&object, &method) && !seen && !catalog_scope);
    const int before = calls;
    enabled = true;
    open(); assert(seen && calls == before && !catalog_scope);
    assert(load_step(&object, &method) && seen && !catalog_scope);
    yield_result = false;
    assert(!load_step(&object, &method) && seen && !catalog_scope);
    refresh(&object, &method); assert(seen && !catalog_scope);
    select(&object, &config, &method); assert(seen && !catalog_scope);
    assert(!available(&method) && calls == before + 1); // real billing remains unavailable
    stock_result = true;
    assert(available(&method) && calls == before + 2); // original success unchanged
    stock_result = false;
    { Scope outer(true);
      { Scope inner(false); assert(!available(&method)); }
      assert(available(&method));
      std::thread other([] { assert(!catalog_scope); assert(!available(&method)); });
      other.join();
    }
    assert(!catalog_scope);
    should_throw = true;
    try { open(); assert(false); } catch (const std::runtime_error&) {}
    assert(!catalog_scope && !available(&method));
    enabled = false;
    { Scope scope(true); assert(!available(&method)); }
}

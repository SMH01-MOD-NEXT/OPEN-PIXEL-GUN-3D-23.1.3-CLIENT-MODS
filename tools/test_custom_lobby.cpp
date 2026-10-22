// g++ -std=c++17 -Wall -Wextra -Werror -Itools/test_support tools/test_custom_lobby.cpp -o /tmp/test-custom-lobby
#include <cassert>
#include <string>
#include <thread>
#include "../opg3d/src/main/cpp/custom_lobby_2313.h"
namespace il2cpp {
void* find_method_info(const char*, const char*, const char*, int) { return nullptr; }
void* method_pointer(void*) { return nullptr; }
void* find_class(const char*, const char*) { return nullptr; }
void* find_field(const char*, const char*, const char*) { return nullptr; }
std::string to_utf8(void* p, size_t) { return *static_cast<std::string*>(p); }
}
namespace hook { bool install(const ManagedMethod&, void*, void**, bool) { return false; } }
namespace {
using namespace custom_lobby_2313::detail;
std::string stock_name = "PixelGun3D", other_name = "Other", replacement;
std::string sentinel = "stock-passwordless-marker", secret = "test-password", empty;
int string_type, other_type, marker_key;
void* marker = nullptr;
int ctor_calls = 0;
uint8_t seen_type = 255;
std::string seen_name;
void* fake_string(const char* p) { replacement = p; return &replacement; }
void ctor(void*, void* name, uint8_t type, MI*) {
    ++ctor_calls; seen_type = type;
    seen_name = name ? *static_cast<std::string*>(name) : "";
}
void* fake_item(void* table, void* key, MI*) {
    assert(table == &marker && key == &marker_key); return marker;
}
void fake_static(void*, void* out) { *static_cast<void**>(out) = &marker_key; }
void* fake_class(void* p) { return p == &other_type ? &other_type : &string_type; }
int32_t length(void* p) { return static_cast<std::string*>(p)->size(); }
bool stock_connect(int group, MI* info) {
    assert(group == 0 && !info); lobby_ctor(nullptr, &stock_name, 2, nullptr); return true;
}
void stock_create(void* name, bool v, bool o, int cap, void* props, void* listed,
                  bool join, bool rejoin, void* users, bool squad, MI* info) {
    assert(name == &other_name && v && o && cap == 10 && props == &marker);
    assert(listed == &marker_key && !join && !rejoin && users == &other_type && !squad && !info);
    lobby_ctor(nullptr, &stock_name, 2, nullptr);
}
void call_create() {
    create(&other_name, true, true, 10, &marker, &marker_key, false, false, &other_type, false, nullptr);
}
}
int main() {
    using namespace custom_lobby_2313::detail;
    original_ctor = ctor; original_connect = stock_connect; original_create = stock_create;
    il2cpp::string_new = fake_string; il2cpp::string_length = length;
    il2cpp::object_get_class = fake_class; il2cpp::field_static_get_value = fake_static;
    get_item = fake_item; password_key_field = &marker_key; string_class = &string_type;
    assert(!custom_scope);
    assert(connect(0, nullptr)); // Disabled/partial hooks must pass through.
    assert(seen_type == 2 && seen_name == "PixelGun3D");
    enabled = true;
    assert(connect(0, nullptr));
    assert(seen_type == 0 && seen_name == "OPG2313Customs" && !custom_scope);
    marker = nullptr; call_create(); assert(seen_type == 2); // Ordinary SQL room.
    marker = &empty; call_create(); assert(seen_type == 2);
    marker = &other_type; call_create(); assert(seen_type == 2); // Wrong type fails closed.
    marker = &sentinel; call_create(); assert(seen_type == 0 && !custom_scope);
    marker = &secret; call_create(); assert(seen_type == 0 && secret == "test-password");
    // Nested contexts restore the previous state; other lobbies remain unchanged.
    { Scope outer(true);
      { Scope inner(false); assert(!custom_scope); }
      assert(custom_scope);
      lobby_ctor(nullptr, &other_name, 2, nullptr); assert(seen_type == 2 && seen_name == "Other");
      lobby_ctor(nullptr, &stock_name, 3, nullptr); assert(seen_type == 3);
      lobby_ctor(nullptr, nullptr, 2, nullptr); assert(seen_type == 2);
      std::thread other([] { assert(!custom_scope); }); other.join();
    }
    assert(!custom_scope && ctor_calls == 10);
}

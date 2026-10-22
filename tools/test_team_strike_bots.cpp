#include <cassert>
#include <string>
#include "../opg3d/src/main/cpp/team_strike_bots_2313.h"
namespace il2cpp {
void* find_class(const char*, const char*) { return nullptr; }
void* find_field(const char*, const char*, const char*) { return nullptr; }
void* find_method_info(const char*, const char*, const char*, int) { return nullptr; }
void* find_method_by_address(const char*, const char*, void*) { return nullptr; }
void* method_pointer(void*) { return nullptr; }
}
namespace hook { bool install(const ManagedMethod&, void*, void**, bool) { return false; } }
namespace elfsym { bool find_library(const char*, uintptr_t*) { return false; } }
int updates = 0, fills = 0;
void stock_update(void*, const void*) { ++updates; }
void stock_fill(void*, const void*) { ++fills; }
int main() {
    using namespace team_strike_bots;
    Snapshot s{1, true, true, true, 1, 8, 1, 0, 1, 0};
    s.online = true;
    Cadence c;
    assert(!c.due(s, 0)); assert(!c.due(s, 9999)); assert(c.due(s, 10000));
    assert(!c.due(s, 10000)); assert(!c.due(s, 19999)); assert(c.due(s, 20000));
    assert(c.due(s, 600000)); assert(!c.due(s, 600001)); // Long pause: never catch up in a burst.
    s.roster_ready = false; assert(!c.due(s, 620000));
    s.roster_ready = true; assert(c.due(s, 620001));
    s.humans = 8; assert(!c.due(s, 700000)); // Human-filled room.
    s.humans = 1; s.bots = 7; s.blue = s.red = 4;
    assert(!c.due(s, 700001));
    s.humans = 2; assert(overflow_team(s) == -1); // Replace a bot, never a human.
    s.blue = 5; assert(overflow_team(s) == 1);
    s.blue = 3; s.red = 5; assert(overflow_team(s) == 2);
    s.bots = 0; s.blue = 1; s.red = 0; s.humans = 1;
    s.status = 4; assert(!c.due(s, 710000)); // No creation during round-reset countdown.
    s.status = 2; assert(c.due(s, 710001)); // Registration only; stock avatar waits next round.
    s.master = false; assert(!c.due(s, 800000));
    s.master = true; assert(!c.due(s, 800001)); assert(!c.due(s, 810000)); assert(c.due(s, 810001));
    s.room = 2; assert(!c.due(s, 900000)); assert(c.due(s, 910000)); // New room resets cadence.
    s.status = 5; assert(!c.due(s, 1000000));
    s.status = 6; assert(!c.due(s, 1000000));
    s.status = -1; assert(!c.due(s, 1000000));
    s.status = 1; assert(!c.due(s, 1000001));
    s.online = false; assert(!c.due(s, 1100000));
    s.online = true; assert(!c.due(s, 1100001));
    s.enabled = false; assert(!c.due(s, 1200000));
    s.enabled = true; assert(!c.due(s, 1200001)); assert(c.due(s, 1210001));
    s.capacity = 0; assert(!c.due(s, 1300000));
    s.capacity = 8; s.humans = 0; assert(!c.due(s, 1300001));
    assert(capacity(0) == 0 && capacity(1) == 0 && capacity(3) == 2 && capacity(6) == 6 && capacity(20) == 8);
    assert(may_remove(1) && may_remove(3) && !may_remove(2) && !may_remove(4) && !may_remove(5));
    using namespace team_strike_bots_2313::detail;
    original_update = stock_update; original_initial_fill = stock_fill;

    // Missing exact-build prerequisites / partial installation must preserve native behavior.
    assert(!team_strike_bots_2313::install()); assert(!active);
    update(nullptr, nullptr); initial_fill(nullptr, nullptr);
    assert(updates == 1 && fills == 1);
    // A destroyed Unity singleton is a non-null managed wrapper, not a live manager.
    static int old_manager;
    static void* manager_wrapper = nullptr;
    static bool native_alive = false;
    il2cpp::field_static_get_value = [](void*, void* out) { *static_cast<void**>(out) = manager_wrapper; };
    manager_field = reinterpret_cast<void*>(99);
    object_alive.code = reinterpret_cast<void*>(+[](void*, const void*) { return native_alive; });
    assert(!manager_alive());
    manager_wrapper = &old_manager;
    assert(!manager_alive()); // Second scene: Destroy left its wrapper in static storage.
    native_alive = true; assert(manager_alive()); // Never add a duplicate to a live scene.
    native_alive = false; assert(!manager_alive());

    // Scene transitions reset scheduling even if Photon reuses its room pointer.
    s = Snapshot{1, true, true, true, 1, 6, 1, 0, 1, 0}; s.online = true;
    active.store(true);
    original_duel_awake = stock_update;
    for (int scene = 0; scene < 3; ++scene) {
        manager_attempt_room = s.room;
        previous_status = 2; previous_bots = 5;
        duel_awake(nullptr, nullptr);
        assert(manager_attempt_room == 0 && previous_status == -1 && previous_bots == -1);
        const int64_t now = 100000 * scene;
        assert(!cadence.due(s, now));
        assert(!cadence.due(s, now + 9999));
        assert(cadence.due(s, now + 10000));
        assert(!cadence.due(s, now + 10001));
    }
    assert(updates == 4); // Every Awake still delegates to the native implementation.
    active.store(false);
    // Pending avatar must not keep a round alive; existing/remote bots unchanged.
    struct MockBot { void* nst; void* controller; void* view; };
    struct MockNST { bool dead; };
    MockNST nst{false}; int avatar, view;
    MockBot bot{&nst, nullptr, &view};
    bot_controller_field = reinterpret_cast<void*>(1);
    bot_view_field = reinterpret_cast<void*>(2);
    dead_field = reinterpret_cast<void*>(3);
    il2cpp::field_get_value = [](void* obj, void* member, void* out) {
        if (member == reinterpret_cast<void*>(1)) *static_cast<void**>(out) = static_cast<MockBot*>(obj)->controller;
        else if (member == reinterpret_cast<void*>(2)) *static_cast<void**>(out) = static_cast<MockBot*>(obj)->view;
        else *static_cast<bool*>(out) = static_cast<MockNST*>(obj)->dead;
    };
    il2cpp::field_set_value.raw = [](void* obj, void*, void* value) { static_cast<MockNST*>(obj)->dead = *static_cast<bool*>(value); };
    view_is_mine.code = reinterpret_cast<void*>(+[](void*, const void*) { return true; });
    mark_unspawned(&bot, &nst, 4); assert(!nst.dead); // Native reset owns countdown.
    mark_unspawned(&bot, &nst, 2); assert(nst.dead); // Mid-round registration.
    nst.dead = false; bot.controller = &avatar;
    mark_unspawned(&bot, &nst, 2); assert(!nst.dead); // Never kill an existing avatar.
    bot.controller = nullptr;
    view_is_mine.code = reinterpret_cast<void*>(+[](void*, const void*) { return false; });
    mark_unspawned(&bot, &nst, 2); assert(!nst.dead); // Remote NST is not ours to modify.
    // The real online predicate must reject synthetic offline Joined rooms.
    offline_field = reinterpret_cast<void*>(1);
    il2cpp::field_static_get_value = [](void*, void* out) { *static_cast<bool*>(out) = true; };
    state_get.code = reinterpret_cast<void*>(+[](const void*) { return 9; });
    assert(!online_joined());
    il2cpp::field_static_get_value = [](void*, void* out) { *static_cast<bool*>(out) = false; };
    assert(online_joined());
    for (int state : {0, 4, 6, 7, 8, 10, 15, 16}) {
        static int current_state;
        current_state = state;
        state_get.code = reinterpret_cast<void*>(+[](const void*) { return current_state; });
        assert(!online_joined());
    }
}

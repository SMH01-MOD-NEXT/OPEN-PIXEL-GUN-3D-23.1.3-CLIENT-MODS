#pragma once
#include <atomic>
#include <chrono>
#include "elf_sym.h"
#include "hook.h"
#include "il2cpp.h"
#include "log.h"
#include "team_strike_bots_policy.h"

// Exact 23.1.3 only. Photon remains authoritative for ownership/replication;
// stock BotInstance/NST and TeamDuel code retain teams, AI, deaths and rounds.
namespace team_strike_bots_2313 {
namespace detail {
using Void = void (*)(void*, const void*);
using Bool = bool (*)(const void*);
using Get = void* (*)(const void*);
using ObjGet = void* (*)(void*, const void*);
using Spawn = void* (*)(void*, void*, const void*);
using Remove = void (*)(void*, void*, const void*);
using Int = int (*)(void*, const void*);
struct Binding { void* code = nullptr; const void* info = nullptr; };
inline Binding feature_get, feature_flag, private_room, room_get, master_get;
inline Binding state_get, component_object, add_component, view_is_mine, object_alive;
inline void *offline_field, *manager_field, *manager_class;
inline Binding max_get, humans_get, properties_get, spawn_bot, remove_bot, roster_refresh;
inline void *mode_field, *duel_field, *status_field, *blue_field, *red_field;
inline void *connecting_blue_field, *connecting_red_field, *instances_field;
inline void *bot_nst_field, *bot_team_field, *dead_field, *bot_controller_field, *bot_view_field;
inline Void original_update = nullptr, original_initial_fill = nullptr;
inline Void original_duel_update = nullptr, original_duel_awake = nullptr;
inline std::atomic<bool> active{false};
inline team_strike_bots::Cadence cadence;
inline int previous_status = -1, previous_bots = -1;
inline uintptr_t manager_attempt_room = 0;

template <typename T> inline T field(void* object, void* member) {
    T result{};
    if (object && member) il2cpp::field_get_value(object, member, &result);
    return result;
}
inline void* singleton(void* member) {
    void* result = nullptr;
    if (member) il2cpp::field_static_get_value(member, &result);
    return result;
}
inline bool team_strike() {
    int mode = -1;
    if (mode_field) il2cpp::field_static_get_value(mode_field, &mode);
    return mode == 16;
}
inline bool enabled() {
    void* flags = reinterpret_cast<Get>(feature_get.code)(feature_get.info);
    return flags && reinterpret_cast<bool (*)(void*, int, const void*)>(feature_flag.code)(flags, 1100, feature_flag.info)
        && !reinterpret_cast<Bool>(private_room.code)(private_room.info);
}
inline bool online_joined() {
    bool offline = true;
    il2cpp::field_static_get_value(offline_field, &offline);
    return !offline && reinterpret_cast<int (*)(const void*)>(state_get.code)(state_get.info) == 9;
}
inline void reset_match() {
    cadence = {};
    previous_status = previous_bots = -1;
    manager_attempt_room = 0;
}
inline void duel_awake(void* self, const void* info) {
    // A new scene may reuse the same Photon room wrapper/address. Do not carry
    // the old scene's failed allocation latch or scheduled deadline into it.
    if (active.load()) reset_match();
    if (original_duel_awake) original_duel_awake(self, info);
}
inline bool manager_alive() {
    void* manager = singleton(manager_field);
    // Unity leaves a non-null managed wrapper after Destroy. Native truth
    // semantics check its underlying object; raw pointer truth breaks re-entry.
    return manager && reinterpret_cast<bool (*)(void*, const void*)>(object_alive.code)(manager, object_alive.info);
}
inline void duel_update(void* self, const void* info) {
    if (original_duel_update) original_duel_update(self, info);
    // Do not alter GameConnect's bot-enable predicate: it also chooses its
    // OFFLINE matchmaking route. Only extend an already JOINED ONLINE room.
    if (!active.load() || !team_strike() || !online_joined()) {
        manager_attempt_room = 0;
        return;
    }
    if (!enabled() || !reinterpret_cast<Bool>(master_get.code)(master_get.info) || manager_alive()) return;
    const auto room = reinterpret_cast<uintptr_t>(reinterpret_cast<Get>(room_get.code)(room_get.info));
    if (!room || manager_attempt_room == room) return;
    manager_attempt_room = room; // A missing Unity component must not cause an allocation loop.
    void* object = reinterpret_cast<ObjGet>(component_object.code)(self, component_object.info);
    void* type = il2cpp::type_get_object(il2cpp::class_get_type(manager_class));
    if (object && type) {
        reinterpret_cast<void* (*)(void*, void*, const void*)>(add_component.code)(object, type, add_component.info);
        LOGI("23.1.3-team-strike-bots: created stock manager in JOINED ONLINE room");
    }
}
struct List {
    void* object = nullptr;
    void* count_info = nullptr;
    void* item_info = nullptr;
    Int count_fn = nullptr;
    void* (*item_fn)(void*, int, const void*) = nullptr;
    explicit List(void* value) : object(value) {
        if (!value) return;
        void* klass = il2cpp::object_get_class(value);
        if (!klass) return;
        count_info = il2cpp::class_get_method_from_name(klass, "get_Count", 0);
        item_info = il2cpp::class_get_method_from_name(klass, "get_Item", 1);
        count_fn = reinterpret_cast<Int>(il2cpp::method_pointer(count_info));
        item_fn = reinterpret_cast<decltype(item_fn)>(il2cpp::method_pointer(item_info));
    }
    int count() const { return count_fn ? count_fn(object, count_info) : -1; }
    void* item(int i) const { return item_fn ? item_fn(object, i, item_info) : nullptr; }
};
inline void initial_fill(void* self, const void* info) {
    // Stock legacy initialization spawns a group in one frame. This mode uses
    // only the scheduler below, never both fill paths simultaneously.
    if (active.load() && team_strike() && online_joined()) return;
    if (original_initial_fill) original_initial_fill(self, info);
}
inline void mark_unspawned(void* bot, void* nst, int status) {
    // NST registration can precede the stock bot initializer. Check the actual
    // avatar rather than assuming its initializer has already been called.
    if (!bot || !nst || status == 4 || field<void*>(bot, bot_controller_field)) return;
    void* view = field<void*>(bot, bot_view_field);
    if (!view || !reinterpret_cast<bool (*)(void*, const void*)>(view_is_mine.code)(view, view_is_mine.info)) return;
    if (!field<bool>(nst, dead_field)) {
        bool dead = true;
        il2cpp::field_set_value(nst, dead_field, &dead);
        LOGI("23.1.3-team-strike-bots: registered; waiting for stock round spawn");
    }
}
inline void update(void* self, const void* info) {
    if (!active.load() || !team_strike() || !online_joined()) {
        cadence = {}; previous_status = previous_bots = -1;
        if (original_update) original_update(self, info);
        return;
    }
    // Unity main thread. Do not let the normal 3–6-second deathmatch filler run.
    void* room = reinterpret_cast<Get>(room_get.code)(room_get.info);
    void* duel = singleton(duel_field);
    team_strike_bots::Snapshot s;
    s.online = online_joined();
    s.room = reinterpret_cast<uintptr_t>(room);
    s.master = room && reinterpret_cast<Bool>(master_get.code)(master_get.info);
    s.enabled = duel && enabled();
    s.status = field<int>(duel, status_field);
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    if (!s.online || !s.master || !s.enabled || !room || !duel || s.status < 1 || s.status > 4) {
        cadence.due(s, now); return;
    }
    reinterpret_cast<Void>(roster_refresh.code)(duel, roster_refresh.info);
    List blue(field<void*>(duel, blue_field)), red(field<void*>(duel, red_field));
    List cb(field<void*>(duel, connecting_blue_field)), cr(field<void*>(duel, connecting_red_field));
    List bots(field<void*>(self, instances_field));
    s.bots = bots.count();
    s.capacity = team_strike_bots::capacity(reinterpret_cast<Int>(max_get.code)(room, max_get.info));
    s.humans = reinterpret_cast<Int>(humans_get.code)(room, humans_get.info);
    s.blue = blue.count(); s.red = red.count();
    s.roster_ready = s.bots >= 0 && s.blue >= 0 && s.red >= 0 && cb.count() >= 0 && cr.count() >= 0;
    s.blue += std::max(0, cb.count()); s.red += std::max(0, cr.count());
    // A pending Photon BotInstance occupies a slot even before its NST exists.
    // Never compound a failed/slow spawn by filling phantom free seats.
    for (int i = 0; i < s.bots; ++i) {
        void* bot = bots.item(i);
        void* nst = field<void*>(bot, bot_nst_field);
        if (!bot || !nst) s.roster_ready = false;
        else mark_unspawned(bot, nst, s.status);
    }
    if (s.status != previous_status || s.bots != previous_bots) {
        LOGI("23.1.3-team-strike-bots: ONLINE status=%d humans=%d bots=%d teams=%d/%d cap=%d ready=%d",
             s.status, s.humans, s.bots, s.blue, s.red, s.capacity, s.roster_ready);
        previous_status = s.status; previous_bots = s.bots;
    }
    if (s.roster_ready && s.capacity && team_strike_bots::may_remove(s.status)) {
        const int overflow = team_strike_bots::overflow_team(s);
        for (int i = s.bots - 1; overflow && i >= 0; --i) {
            void* bot = bots.item(i);
            if (bot && (overflow == -1 || field<int>(bot, bot_team_field) == overflow)) {
                reinterpret_cast<Remove>(remove_bot.code)(self, bot, remove_bot.info);
                LOGI("23.1.3-team-strike-bots: freed bot seat for human player");
                return; // One removal per frame; never alter a live round.
            }
        }
    }
    if (!cadence.due(s, now)) return;
    void* properties = reinterpret_cast<ObjGet>(properties_get.code)(self, properties_get.info);
    if (!properties) return;
    void* created = reinterpret_cast<Spawn>(spawn_bot.code)(self, properties, spawn_bot.info);
    LOGI("23.1.3-team-strike-bots: one spawn requested success=%d", created != nullptr);
}
inline Binding bind(uintptr_t base, const char* ns, const char* klass, uintptr_t rva) {
    void* mi = il2cpp::find_method_by_address(ns, klass, reinterpret_cast<void*>(base + rva));
    return {il2cpp::method_pointer(mi), mi};
}
} // namespace detail

inline bool install() {
    using namespace detail;
    uintptr_t base = 0;
    if (!elfsym::find_library("libil2cpp.so", &base) || !il2cpp::field_get_value ||
        !il2cpp::field_static_get_value || !il2cpp::field_set_value ||
        !il2cpp::object_get_class || !il2cpp::class_get_method_from_name ||
        !il2cpp::class_get_type || !il2cpp::type_get_object) return false;
    feature_get = bind(base, "PGCompany", u8"不东上丘丞丌丕丈丆", 0x2BABE54);
    feature_flag = bind(base, "PGCompany", u8"不东上丘丞丌丕丈丆", 0x2BACC44);
    private_room = bind(base, "", u8"丙一业三丅万丂丙丞", 0x2B71728);
    room_get = bind(base, "", u8"丟丝专丄丑世丞世丒", 0x4465F68);
    master_get = bind(base, "", u8"丟丝专丄丑世丞世丒", 0x4467420);
    state_get = bind(base, "", u8"丟丝专丄丑世丞世丒", 0x4463A90);
    component_object = bind(base, "UnityEngine", "Component", 0x4436F54);
    add_component = bind(base, "UnityEngine", "GameObject", 0x44218CC);
    view_is_mine = bind(base, "", "PhotonView", 0x44720A8);
    object_alive = bind(base, "UnityEngine", "Object", 0x44081FC);
    max_get = bind(base, "", u8"丂丗丄丆七与丆下丐", 0x4474D20);
    humans_get = bind(base, "", u8"丂丗丄丆七与丆下丐", 0x4474F98);
    properties_get = bind(base, "", "PlayerBotsManager", 0x239E1DC);
    spawn_bot = bind(base, "", "PlayerBotsManager", 0x239E53C);
    remove_bot = bind(base, "", "PlayerBotsManager", 0x239EA2C);
    roster_refresh = bind(base, "", "TeamDuelController", 0x1F2D974);
    for (const Binding& b : {feature_get, feature_flag, private_room, room_get, master_get,
            state_get, component_object, add_component, view_is_mine, object_alive, max_get, humans_get, properties_get, spawn_bot, remove_bot, roster_refresh}) if (!b.code) return false;
    offline_field = il2cpp::find_field("", u8"丟丝专丄丑世丞世丒", u8"专丐上下专丌丗丙丄");
    manager_field = il2cpp::find_field("", "PlayerBotsManager", u8"<专专专丞丅丞丕丑丐>k__BackingField");
    manager_class = il2cpp::find_class("", "PlayerBotsManager");
    mode_field = il2cpp::find_field("", "GameConnect", u8"万丘丌丝一不丒万一");
    duel_field = il2cpp::find_field("", "TeamDuelController", u8"丛世丌丛丒丝丂丟丟");
    status_field = il2cpp::find_field("", "TeamDuelController", "roomStatus");
    blue_field = il2cpp::find_field("", "TeamDuelController", "blueTeam");
    red_field = il2cpp::find_field("", "TeamDuelController", "redTeam");
    connecting_blue_field = il2cpp::find_field("", "TeamDuelController", "connectingBlueTeam");
    connecting_red_field = il2cpp::find_field("", "TeamDuelController", "connectingRedTeam");
    instances_field = il2cpp::find_field("", "PlayerBotsManager", u8"专丈丏七丄丛丙下世");
    bot_nst_field = il2cpp::find_field("", "PlayerBotInstance", u8"三丌丆丂与丑丟丂上");
    bot_team_field = il2cpp::find_field("", "PlayerBotInstance", u8"三丝丌一丘丕东丈丙");
    bot_controller_field = il2cpp::find_field("", "PlayerBotInstance", u8"丝与丞丗专丟七与丑");
    bot_view_field = il2cpp::find_field("", "PlayerBotInstance", u8"丌丅丝下丈世丈丐且");
    dead_field = il2cpp::find_field("", "NetworkStartTable", u8"上一下丁丝丏丏专丙");
    for (void* f : {offline_field, manager_field, manager_class, mode_field, duel_field, status_field, blue_field, red_field,
            connecting_blue_field, connecting_red_field, instances_field, bot_nst_field,
            bot_team_field, bot_controller_field, bot_view_field, dead_field}) if (!f) return false;
    struct Target { hook::ManagedMethod method; uintptr_t rva; void* replacement; void** original; };
    Target targets[] = {
        {{"", "TeamDuelController", "Awake", 0}, 0x1F2D614, reinterpret_cast<void*>(&duel_awake), reinterpret_cast<void**>(&original_duel_awake)},
        {{"", "TeamDuelController", "Update", 0}, 0x1F2DE78, reinterpret_cast<void*>(&duel_update), reinterpret_cast<void**>(&original_duel_update)},
        {{"", "PlayerBotsManager", "Update", 0}, 0x239EAA0, reinterpret_cast<void*>(&update), reinterpret_cast<void**>(&original_update)},
        {{"", "PlayerBotsManager", u8"丅丙一丞且不万丐丂", 0}, 0x239EB40, reinterpret_cast<void*>(&initial_fill), reinterpret_cast<void**>(&original_initial_fill)}
    };
    for (const Target& t : targets) {
        auto b = bind(base, t.method.namespaze, t.method.klass, t.rva);
        auto* named = il2cpp::find_method_info(t.method.namespaze, t.method.klass, t.method.method, 0);
        if (!b.code || il2cpp::method_pointer(named) != b.code) return false;
    }
    for (const Target& t : targets) if (!hook::install(t.method, t.replacement, t.original, true)) return false;
    active.store(true); // Partial installation always delegates to stock.
    LOGI("23.1.3-team-strike-bots: ONLINE ONLY, one per 10s; new avatars join next round");
    return true;
}
} // namespace team_strike_bots_2313

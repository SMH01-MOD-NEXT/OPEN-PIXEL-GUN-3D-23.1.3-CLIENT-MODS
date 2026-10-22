#pragma once
#include "team_strike_bots_2313.h"
#include "escort_bots_policy.h"

// Exact owned 23.1.3 only. Siege=17 is the UI Escort mode; ClanSiege=18
// remains untouched. The stock controllers retain distances, gates and winner.
namespace escort_bots_2313 {
namespace detail {
using namespace team_strike_bots_2313::detail;
using escort_bots::Vector3;
using escort_bots::Bounds;
inline std::atomic<bool> escort_active{false};
inline Void stock_siege_update = nullptr, stock_siege_awake = nullptr, stock_tram_update = nullptr;
inline Void stock_choose_point = nullptr, stock_rosters = nullptr;
inline void *online_mode_field, *all_nsts_field, *nst_bot_field, *nst_team_field;
inline Void stock_walk_tick=nullptr, stock_search_tick=nullptr, stock_skirmish_tick=nullptr;
inline Binding path_to, nav_speed, nav_acceleration, ai_speed;
inline void *behavior_nav, *behavior_hold, *behavior_path_aim, *controller_view, *skirmish_acceleration;
inline escort_bots::PathCadence path_cadence;
inline int64_t next_goal_log=0, next_proximity_log=0;
inline bool (*stock_may_spawn)(void*,const void*) = nullptr;
inline void *behavior_ai, *behavior_point;
inline Binding tram_add, tram_remove, component_transform, position, inverse_point, world_point;
inline void *siege_instance, *siege_status, *team1_field, *team2_field, *ct1_field, *ct2_field;
inline void *tram_instance, *tram_object, *tram_players, *detect_bounds;
inline void *entity_field, *entity_player, *pmc_bot, *pmc_dead, *pmc_nst;
inline void *ai_controller, *controllers_field;
inline escort_bots::Cadence escort_cadence;
inline uintptr_t attempted_room = 0;
inline int logged_status = -1, logged_bots = -1;
inline bool escort() {
    int mode = -1;
    if (mode_field) il2cpp::field_static_get_value(mode_field, &mode);
    return mode == 17;
}
inline bool handles() { return escort_active.load() && escort() && online_joined(); }
inline bool alive(void* p) {
    return p && reinterpret_cast<bool (*)(void*, const void*)>(object_alive.code)(p, object_alive.info);
}
inline Vector3 transform_call(const Binding& b, void* t, Vector3 p) {
    return reinterpret_cast<Vector3 (*)(void*, Vector3, const void*)>(b.code)(t, p, b.info);
}
inline void* transform(void* p) { return reinterpret_cast<ObjGet>(component_transform.code)(p, component_transform.info); }
inline Vector3 pos(void* t) { return reinterpret_cast<Vector3 (*)(void*, const void*)>(position.code)(t, position.info); }
// The stock Siege roster filters NST's human-ready flag (0x74), unlike
// TeamDuel. Bots have real replicated NSTs but never set that human flag.
// Supplement the roster from the same stock global NST list on EVERY peer;
// do not fake human readiness or create synthetic Photon actors.
inline bool append_unique(List& list, void* value) {
    if (!value || list.count() < 0) return false;
    for (int i=0;i<list.count();++i) if (list.item(i)==value) return true;
    void* klass=il2cpp::object_get_class(list.object);
    void* method=il2cpp::class_get_method_from_name(klass,"Add",1);
    void* fn=il2cpp::method_pointer(method);
    if (!fn) return false;
    reinterpret_cast<void (*)(void*,void*,const void*)>(fn)(list.object,value,method);
    return true;
}
inline void rosters(void* self, const void* info) {
    if (stock_rosters) stock_rosters(self,info);
    if (!handles() || !enabled()) return;
    void* online=singleton(online_mode_field);
    if (!alive(online)) return;
    List nsts(field<void*>(online,all_nsts_field));
    List t1(field<void*>(self,team1_field)), t2(field<void*>(self,team2_field));
    for (int i=0;i<nsts.count();++i) {
        void* nst=nsts.item(i);
        if (!alive(nst) || !alive(field<void*>(nst,nst_bot_field))) continue;
        int team=field<int>(nst,nst_team_field);
        if (team==1) append_unique(t1,nst);
        else if (team==2) append_unique(t2,nst);
    }
}
inline bool may_spawn(void* self, const void* info) {
    // This is BotInstance's avatar/respawn gate, NOT GameConnect's offline
    // matchmaking predicate. Its caller retains PhotonView ownership, dead
    // state and stock respawn delay handling.
    if (handles() && enabled()) {
        void* siege=singleton(siege_instance);
        // Although metadata declares an instance method, AOT callers at
        // 2395990/23959E4 omit `this`: the stock body uses only global state.
        // x0 may contain the previous bool result (1), NOT a BotInstance.
        // Never dereference self here. Stock caller retains the ownership gate.
        return alive(siege) && escort_bots::playing(field<int>(siege,siege_status));
    }
    return stock_may_spawn ? stock_may_spawn(self,info) : false;
}
inline void awake(void* self, const void* info) {
    escort_cadence = {}; path_cadence = {}; next_goal_log=next_proximity_log=0; attempted_room = 0; logged_status = logged_bots = -1;
    if (stock_siege_awake) stock_siege_awake(self, info);
}
inline void siege_update(void* self, const void* info) {
    if (stock_siege_update) stock_siege_update(self, info);
    if (!handles()) { attempted_room = 0; return; }
    if (!enabled() || !reinterpret_cast<Bool>(master_get.code)(master_get.info) || manager_alive()) return;
    void* room = reinterpret_cast<Get>(room_get.code)(room_get.info);
    if (!room || attempted_room == reinterpret_cast<uintptr_t>(room)) return;
    attempted_room = reinterpret_cast<uintptr_t>(room);
    void* object = reinterpret_cast<ObjGet>(component_object.code)(self, component_object.info);
    void* type = il2cpp::type_get_object(il2cpp::class_get_type(manager_class));
    if (object && type) {
        reinterpret_cast<void* (*)(void*, void*, const void*)>(add_component.code)(object, type, add_component.info);
        LOGI("23.1.3-escort-bots: created stock manager in JOINED ONLINE room");
    }
}
inline bool manager_update(void* self, const void*) {
    if (!handles()) { escort_cadence = {}; return false; }
    void* room = reinterpret_cast<Get>(room_get.code)(room_get.info);
    void* siege = singleton(siege_instance);
    escort_bots::Snapshot s;
    s.room = reinterpret_cast<uintptr_t>(room); s.online = true;
    s.master = room && reinterpret_cast<Bool>(master_get.code)(master_get.info);
    s.enabled = alive(siege) && enabled(); s.status = field<int>(siege, siege_status);
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    if (!s.master || !s.enabled || !escort_bots::stage(s.status)) {
        escort_cadence.due(s, now); return true;
    }
    List t1(field<void*>(siege, team1_field)), t2(field<void*>(siege, team2_field));
    List c1(field<void*>(siege, ct1_field)), c2(field<void*>(siege, ct2_field));
    List bots(field<void*>(self, instances_field));
    s.capacity = reinterpret_cast<Int>(max_get.code)(room, max_get.info);
    s.humans = reinterpret_cast<Int>(humans_get.code)(room, humans_get.info);
    s.bots = bots.count(); s.team1 = t1.count(); s.team2 = t2.count();
    s.ready = s.bots >= 0 && s.team1 >= 0 && s.team2 >= 0 && c1.count() >= 0 && c2.count() >= 0;
    s.team1 += std::max(0,c1.count()); s.team2 += std::max(0,c2.count());
    for (int i = 0; i < s.bots; ++i) {
        void* bot = bots.item(i);
        if (!bot || !field<void*>(bot, bot_nst_field)) s.ready = false;
    }
    if (s.status != logged_status || s.bots != logged_bots) {
        LOGI("23.1.3-escort-bots: status=%d humans=%d bots=%d teams=%d/%d cap=%d ready=%d",
             s.status,s.humans,s.bots,s.team1,s.team2,s.capacity,s.ready);
        for (int i=0;i<s.bots;++i) {
            void* bot=bots.item(i); void* nst=field<void*>(bot,bot_nst_field);
            LOGI("23.1.3-escort-bots: participant=%d botTeam=%d nstTeam=%d avatar=%d",
                i,field<int>(bot,bot_team_field),field<int>(nst,nst_team_field),
                alive(field<void*>(bot,bot_controller_field)));
        }
        logged_status=s.status; logged_bots=s.bots;
    }
    // Escort has respawns, unlike Team Strike; human seats can be freed during play.
    const int excess = s.ready ? escort_bots::overflow(s) : 0;
    for (int i=s.bots-1; excess && i>=0; --i) {
        void* bot=bots.item(i);
        if (bot && (excess == -1 || field<int>(bot,bot_team_field)==excess)) {
            reinterpret_cast<Remove>(remove_bot.code)(self,bot,remove_bot.info);
            return true;
        }
    }
    if (escort_cadence.due(s,now)) {
        void* props=reinterpret_cast<ObjGet>(properties_get.code)(self,properties_get.info);
        void* bot=props ? reinterpret_cast<Spawn>(spawn_bot.code)(self,props,spawn_bot.info) : nullptr;
        LOGI("23.1.3-escort-bots: one spawn requested success=%d",bot!=nullptr);
    }
    return true;
}
inline bool near(void* pmc, void* tram, void* tram_transform) {
    if (!alive(pmc) || field<bool>(pmc,pmc_dead) || !field<void*>(pmc,pmc_nst)) return false;
    void* t=transform(pmc);
    return t && escort_bots::contains(field<Bounds>(tram,detect_bounds),
        transform_call(inverse_point,tram_transform,pos(t)));
}
inline void tram_update(void* self, const void* info) {
    if (handles()) {
        void* tram=field<void*>(self,tram_object);
        void* manager=singleton(manager_field);
        void* tt=alive(tram) ? transform(tram) : nullptr;
        if (tt) {
            // Bot avatars really contain Player_move_c; never cast a bot wrapper
            // into that type or invent an NST. Use stock managed list helpers.
            List nearby(field<void*>(self,tram_players));
            for (int i=nearby.count()-1;i>=0;--i) {
                void* p=nearby.item(i);
                if (p && field<void*>(p,pmc_bot) && (!alive(manager) || !near(p,tram,tt)))
                    reinterpret_cast<bool (*)(void*,void*,const void*)>(tram_remove.code)(self,p,tram_remove.info);
            }
            const auto now=std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            const bool report=now>=next_proximity_log;
            int living[3]{}, close[3]{}; float distances[3]{};
            List controllers(alive(manager) ? field<void*>(manager,controllers_field) : nullptr);
            for (int i=0;i<controllers.count();++i) {
                void* c=controllers.item(i);
                void* e=alive(c) ? field<void*>(c,entity_field) : nullptr;
                void* p=alive(e) ? field<void*>(e,entity_player) : nullptr;
                const bool nearby=p && near(p,tram,tt);
                if (nearby)
                    reinterpret_cast<bool (*)(void*,void*,const void*)>(tram_add.code)(self,p,tram_add.info);
                if (report && alive(p) && !field<bool>(p,pmc_dead)) {
                    void* nst=field<void*>(p,pmc_nst);
                    const int team=field<int>(nst,nst_team_field);
                    void* pt=transform(p);
                    if (pt && (team==1 || team==2)) {
                        auto local=transform_call(inverse_point,tt,pos(pt));
                        auto center=field<Bounds>(tram,detect_bounds).center;
                        if (escort_bots::finite(local)) {
                            ++living[team];close[team]+=nearby;
                            distances[team]+=std::hypot(local.x-center.x,local.z-center.z);
                        }
                    }
                }
            }
            if (report && handles()) {
                LOGI("23.1.3-escort-proximity: team1 alive=%d near=%d meanDistance=%.1f team2 alive=%d near=%d meanDistance=%.1f",
                    living[1],close[1],living[1]?distances[1]/living[1]:0,
                    living[2],close[2],living[2]?distances[2]/living[2]:0);
                const auto bounds=field<Bounds>(tram,detect_bounds);
                LOGI("23.1.3-escort-volume: center=%.1f/%.1f/%.1f extents=%.1f/%.1f/%.1f",
                    bounds.center.x,bounds.center.y,bounds.center.z,bounds.extents.x,bounds.extents.y,bounds.extents.z);
                next_proximity_log=now+10000;
            }
        }
    }
    if (stock_tram_update) stock_tram_update(self,info);
}
inline bool objective_tick(void* self, int behavior) {
    if (!handles()) { path_cadence={}; return false; }
    if (!enabled()) return false;
    void* siege=singleton(siege_instance);
    if (!alive(siege)) return false;
    const int phase=field<int>(siege,siege_status);
    if (!escort_bots::playing(phase)) return false;
    void* ai=field<void*>(self,behavior_ai);
    void* controller=ai ? field<void*>(ai,ai_controller) : nullptr;
    if (!alive(controller)) return false;
    void* view=field<void*>(controller,controller_view);
    // Every peer may have avatar components, but only its actual Photon owner
    // can steer this bot. Do not substitute a global master-only assumption.
    if (!view || !reinterpret_cast<bool (*)(void*,const void*)>(view_is_mine.code)(view,view_is_mine.info)) return false;
    void* e=field<void*>(controller,entity_field);
    void* p=alive(e) ? field<void*>(e,entity_player) : nullptr;
    void* tramc=singleton(tram_instance);
    void* tram=alive(tramc) ? field<void*>(tramc,tram_object) : nullptr;
    void* nav=field<void*>(self,behavior_nav);
    if (!alive(p) || field<bool>(p,pmc_dead) || !alive(tram) || !alive(nav)) return false;
    void* tt=transform(tram); void* pt=transform(p);
    if (!tt || !pt) return false;
    const Bounds bounds=field<Bounds>(tram,detect_bounds);
    const Vector3 local=transform_call(inverse_point,tt,pos(pt));
    if (!escort_bots::finite(local) || !escort_bots::contains(bounds,bounds.center)) return false;
    // Inside the objective volume, retain stock scanning/roaming as well as
    // search cancellation, enemy approach, strafing and dodge/jump selection. Suppressing these entirely
    // can leave opposing teams stationary behind cover with no useful fight.
    // As soon as a combat maneuver leaves the ram volume, approach wins again.
    if (escort_bots::contains(bounds,local)) return false;
    const auto now=std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    const auto room=reinterpret_cast<uintptr_t>(reinterpret_cast<Get>(room_get.code)(room_get.info));
    if (!room) return false;
    // Locomotion only. Stock target detector, state transitions, weapon choice,
    // AIBotController.Update (fire) and LateUpdate (enemy aim) stay untouched.
    bool hold=false, path_aim=behavior!=3;
    il2cpp::field_set_value(self,behavior_hold,&hold);
    il2cpp::field_set_value(self,behavior_path_aim,&path_aim);
    // A fresh behavior starts with a zero waypoint. Refresh its logical target
    // immediately even if this bot's next expensive path calculation is pending.
    const auto candidates=escort_bots::objective_points(bounds,local);
    auto primary=transform_call(world_point,tt,candidates[0]);
    if (escort_bots::finite(primary)) il2cpp::field_set_value(self,behavior_point,&primary);
    if (!path_cadence.due(room,reinterpret_cast<uintptr_t>(ai),phase,now)) return true;
    float speed=reinterpret_cast<float (*)(void*,const void*)>(ai_speed.code)(ai,ai_speed.info);
    if (std::isfinite(speed) && speed>0)
        reinterpret_cast<void (*)(void*,float,const void*)>(nav_speed.code)(nav,speed,nav_speed.info);
    if (behavior==3) {
        // Stock Skirmish.Init caches pre-combat acceleration at 0x4C, then
        // reduces it to 0.5 for dodging. Keep the saved locomotion value while
        // approaching the objective; stock Exit still restores its own cache.
        float acceleration=field<float>(self,skirmish_acceleration);
        if (std::isfinite(acceleration) && acceleration>0)
            reinterpret_cast<void (*)(void*,float,const void*)>(nav_acceleration.code)(nav,acceleration,nav_acceleration.info);
    }
    bool success=false;
    Vector3 selected{};
    for (auto candidate:candidates) {
        Vector3 world=transform_call(world_point,tt,candidate);
        if (!escort_bots::finite(world)) continue;
        // Exact stock helper: CalculatePath, complete-path check, off-mesh-link
        // guard, then SetPath. Never teleport, fabricate proximity or score.
        success=reinterpret_cast<bool (*)(void*,Vector3,const void*)>(path_to.code)(nav,world,path_to.info);
        if (success) {
            selected=world;
            il2cpp::field_set_value(self,behavior_point,&world);
            break;
        }
    }
    if (now>=next_goal_log) {
        LOGI("23.1.3-escort-goal: phase=%d behavior=%d path=%d near=%d local=%.1f/%.1f/%.1f goal=%.1f/%.1f/%.1f",
             phase,behavior,success,escort_bots::contains(bounds,local),local.x,local.y,local.z,selected.x,selected.y,selected.z);
        next_goal_log=now+5000;
    }
    // Do not revert to ranged combat camping when a path temporarily fails.
    // Keep the previous stock path and retry at a bounded cadence.
    return true;
}
inline void choose_point(void* self, const void* info) {
    if (!objective_tick(self,1) && stock_choose_point) stock_choose_point(self,info);
}
inline void walk_tick(void* self,const void* info) {
    if (!objective_tick(self,1) && stock_walk_tick) stock_walk_tick(self,info);
}
inline void search_tick(void* self,const void* info) {
    if (!objective_tick(self,2) && stock_search_tick) stock_search_tick(self,info);
}
inline void skirmish_tick(void* self,const void* info) {
    if (!objective_tick(self,3) && stock_skirmish_tick) stock_skirmish_tick(self,info);
}
} // namespace detail
inline bool install() {
    using namespace detail;
    if (!active.load()) return false; // Shared hooks/bindings must be fully installed first.
    uintptr_t base=0;
    if (!elfsym::find_library("libil2cpp.so",&base)) return false;
    struct F {void** out;const char* klass;const char* name;};
    F fields[]={
        {&online_mode_field,"GameController","onlineMode"},
        {&all_nsts_field,"OnlineModeController",u8"不丝万丕业世丕上不"},
        {&nst_bot_field,"NetworkStartTable",u8"与丅丒专丆世上丆七"},
        {&nst_team_field,"NetworkStartTable",u8"丁丏丅丒丌世丒丑业"},
        {&siege_instance,"SiegeController",u8"丛世丌丛丒丝丂丟丟"},{&siege_status,"SiegeController","roomStatus"},
        {&team1_field,"SiegeController","team1"},{&team2_field,"SiegeController","team2"},
        {&ct1_field,"SiegeController","connectingTeam1"},{&ct2_field,"SiegeController","connectingTeam2"},
        {&tram_instance,"TramController",u8"丛世丌丛丒丝丂丟丟"},{&tram_object,"TramController","tramObject"},
        {&tram_players,"TramController",u8"业丂丟丝与丂丁丂丛"},{&detect_bounds,"TramObject","detectBound"},
        {&entity_field,"PlayerBotController","player"},{&entity_player,"PlayerBotEntity",u8"不世三丌丄下丗与丂"},
        {&pmc_bot,"Player_move_c",u8"丅七业丛丛丁丗丘丂"},{&pmc_dead,"Player_move_c",u8"不不与丏丑丗丛东丙"},
        {&pmc_nst,"Player_move_c",u8"丄丘丛丁丗丝一七丗"},
        {&controller_view,"PlayerBotController",u8"丌丅丝下丈世丈丐且"},
        {&controllers_field,"PlayerBotsManager",u8"丒且丕丘丏下丏丐丒"}
    };
    for (auto& f:fields) { *f.out=il2cpp::find_field("",f.klass,f.name); if (!*f.out) return false; }
    ai_controller=il2cpp::find_field("PlayerBot","AIBotController","playerBotController");
    behavior_ai=il2cpp::find_field("PlayerBot",u8"专丆不丘丞丗一丙丌",u8"丂与丌且丏丁丗三丟");
    behavior_point=il2cpp::find_field("PlayerBot",u8"专丆不丘丞丗一丙丌",u8"丑三丄七丘丛不丝丌");
    behavior_nav=il2cpp::find_field("PlayerBot",u8"专丆不丘丞丗一丙丌",u8"丞丈丛丂世丈丂丐丗");
    behavior_hold=il2cpp::find_field("PlayerBot",u8"专丆不丘丞丗一丙丌",u8"丂丆三世丑丒丗万丞");
    behavior_path_aim=il2cpp::find_field("PlayerBot",u8"专丆不丘丞丗一丙丌",u8"丁三丗丆丟丆世丗丟");
    skirmish_acceleration=il2cpp::find_field("PlayerBot",u8"丆丅万丗丘丘一三丟",u8"丄东丟丗三丅丐与专");
    if (!ai_controller || !behavior_ai || !behavior_point || !behavior_nav || !behavior_hold || !behavior_path_aim || !skirmish_acceleration) return false;
    path_to=bind(base,"PlayerBot","NavMeshAgentController",0x1F4D348);
    nav_speed=bind(base,"PlayerBot","NavMeshAgentController",0x1F4D5BC);
    nav_acceleration=bind(base,"PlayerBot","NavMeshAgentController",0x1F4D5F4);
    ai_speed=bind(base,"PlayerBot","AIBotController",0x2143AFC);
    for (auto b:{path_to,nav_speed,nav_acceleration,ai_speed}) if (!b.code) return false;
    tram_add=bind(base,"","TramController",0x1DE897C); tram_remove=bind(base,"","TramController",0x1DE8A64);
    component_transform=bind(base,"UnityEngine","Component",0x4436F18);
    position=bind(base,"UnityEngine","Transform",0x4442A58);
    inverse_point=bind(base,"UnityEngine","Transform",0x44442B4);
    world_point=bind(base,"UnityEngine","Transform",0x44441F4);
    for (auto b:{tram_add,tram_remove,component_transform,position,inverse_point,world_point}) if (!b.code) return false;
    struct T {hook::ManagedMethod target;uintptr_t rva;void* replacement;void** original;};
    T targets[]={
        {{"","PlayerBotInstance",u8"一丒且上丝丐丂业丟",0},0x2396D48,reinterpret_cast<void*>(&may_spawn),reinterpret_cast<void**>(&stock_may_spawn)},
        {{"","SiegeController",u8"东丈丕万世丈丄专丛",0},0x5088A3C,reinterpret_cast<void*>(&rosters),reinterpret_cast<void**>(&stock_rosters)},
        {{"","SiegeController","Awake",0},0x5088540,reinterpret_cast<void*>(&awake),reinterpret_cast<void**>(&stock_siege_awake)},
        {{"","SiegeController","Update",0},0x5089974,reinterpret_cast<void*>(&siege_update),reinterpret_cast<void**>(&stock_siege_update)},
        {{"","TramController","Update",0},0x1DE8374,reinterpret_cast<void*>(&tram_update),reinterpret_cast<void**>(&stock_tram_update)},
        {{"PlayerBot",u8"业丅丏丙丐丈丑一丝",u8"三丒三丗业七七东万",0},0x214AD14,reinterpret_cast<void*>(&choose_point),reinterpret_cast<void**>(&stock_choose_point)},
        {{"PlayerBot",u8"业丅丏丙丐丈丑一丝",u8"上丞丐专七丟下丙不",0},0x214B3DC,reinterpret_cast<void*>(&walk_tick),reinterpret_cast<void**>(&stock_walk_tick)},
        {{"PlayerBot",u8"下丘丅丑丌且且丄丁",u8"上丞丐专七丟下丙不",0},0x1F4B854,reinterpret_cast<void*>(&search_tick),reinterpret_cast<void**>(&stock_search_tick)},
        {{"PlayerBot",u8"丆丅万丗丘丘一三丟",u8"上丞丐专七丟下丙不",0},0x1F4BB18,reinterpret_cast<void*>(&skirmish_tick),reinterpret_cast<void**>(&stock_skirmish_tick)}
    };
    for (auto& t:targets) {
        auto b=bind(base,t.target.namespaze,t.target.klass,t.rva);
        if (!b.code || il2cpp::method_pointer(il2cpp::find_method_info(t.target.namespaze,t.target.klass,t.target.method,0))!=b.code) return false;
    }
    for (auto& t:targets) if (!hook::install(t.target,t.replacement,t.original,true)) return false;
    additional_update=&manager_update; additional_initial_fill=&handles;
    escort_active.store(true);
    LOGI("23.1.3-escort-bots: online only; stock ram proximity, side swaps and results");
    return true;
}
} // namespace escort_bots_2313

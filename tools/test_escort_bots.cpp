#include <cassert>
#include <limits>
#include <vector>
#include <cstring>
#include "../opg3d/src/main/cpp/escort_bots_2313.h"
namespace movement_harness {
using escort_bots::Vector3; using escort_bots::Bounds;
int keys[24], objects[10];
bool mine=true,dead=false,path_ok=true,offline=false; int paths=0,sets=0,phase=4,game_mode=17;
Vector3 destination{}; bool hold=true,path_aim=false;
void* obj(int i){return &objects[i];} void* key(int i){return &keys[i];}
void statics(void* k,void* out) {
    using namespace escort_bots_2313::detail;
    if(k==mode_field)*static_cast<int*>(out)=game_mode;
    else if(k==offline_field)*static_cast<bool*>(out)=offline;
    else *static_cast<void**>(out)=k==siege_instance?obj(0):obj(5);
}
void fields(void* o,void* k,void* out) {
    using namespace escort_bots_2313::detail;
    if(k==siege_status)*static_cast<int*>(out)=phase;
    else if(k==pmc_dead)*static_cast<bool*>(out)=dead;
    else if(k==detect_bounds)*static_cast<Bounds*>(out)={{0,0,0},{3,3,3}};
    else if(k==skirmish_acceleration)*static_cast<float*>(out)=8;
    else {
        int index=-1;
        if(k==behavior_ai)index=1; else if(k==ai_controller)index=2;
        else if(k==controller_view)index=3; else if(k==entity_field)index=4;
        else if(k==entity_player)index=6; else if(k==tram_object)index=7;
        else if(k==behavior_nav)index=8;
        assert(index>=0 && o);*static_cast<void**>(out)=obj(index);
    }
}
void set(void*,void* k,void* p) {
    using namespace escort_bots_2313::detail;
    ++sets;
    if(k==behavior_point)destination=*static_cast<Vector3*>(p);
    else if(k==behavior_hold)hold=*static_cast<bool*>(p);
    else {assert(k==behavior_path_aim);path_aim=*static_cast<bool*>(p);}
}
bool owner(void*,const void*) {return mine;}
void* room(const void*) {return obj(9);}
void* transform(void* p,const void*) {return p;}
Vector3 bot_position{100,0,100};
Vector3 position(void*,const void*) {return bot_position;}
Vector3 point(void*,Vector3 v,const void*) {return v;}
bool path(void*,Vector3 v,const void*) {++paths;assert(escort_bots::finite(v));return path_ok;}
float speed(void*,const void*) {return 5;}
void set_float(void*,float v,const void*) {assert(v>0 && std::isfinite(v));}
}
namespace harness {
int mode=17,status=4,state=9,mode_key,offline_key,siege_key,status_key;
bool offline=false,flag=true,custom=false;
int dummy;
void* siege=&dummy;
void statics(void* key,void* out) {
    if(key==&mode_key) *static_cast<int*>(out)=mode;
    else if(key==&offline_key) *static_cast<bool*>(out)=offline;
    else {assert(key==&siege_key);*static_cast<void**>(out)=siege;}
}
void fields(void* object,void* key,void* out) {
    assert(object==siege); // AOT may pass x0=1 to the context-free spawn gate.
    assert(key==&status_key);*static_cast<int*>(out)=status;
}
bool alive(void* p,const void*) {return p!=nullptr;}
int get_state(const void*) {return state;}
void* flags(const void*) {return &dummy;}
bool feature(void*,int key,const void*) {assert(key==1100);return flag;}
bool private_room(const void*) {return custom;}
using Items=std::vector<void*>;
int size(void* o,const void*) {return static_cast<int>(static_cast<Items*>(o)->size());}
void* item(void* o,int i,const void*) {return static_cast<Items*>(o)->at(i);}
void add(void* o,void* value,const void*) {static_cast<Items*>(o)->push_back(value);}
void* klass(void* o) {return o;}
void* method(void*,const char* name,int) {
    if(!std::strcmp(name,"get_Count")) return reinterpret_cast<void*>(&size);
    if(!std::strcmp(name,"get_Item")) return reinterpret_cast<void*>(&item);
    if(!std::strcmp(name,"Add")) return reinterpret_cast<void*>(&add);
    return nullptr;
}
}
namespace il2cpp {
void* find_class(const char*, const char*) { return nullptr; }
void* find_field(const char*, const char*, const char*) { return nullptr; }
void* find_method_info(const char*, const char*, const char*, int) { return nullptr; }
void* find_method_by_address(const char*, const char*, void*) { return nullptr; }
void* method_pointer(void* p) { return p; }
}
namespace hook { bool install(const ManagedMethod&, void*, void**, bool) { return false; } }
namespace elfsym { bool find_library(const char*, uintptr_t*) { return false; } }
int stock_calls=0;
void stock(void*,const void*) { ++stock_calls; }
int main() {
    using namespace escort_bots;
    Bounds b{{2,3,4},{1,2,3}};
    assert(contains(b,{2,3,4})); assert(contains(b,{3,5,7}));
    assert(!contains(b,{3.01f,5,7})); assert(!contains(b,{2,5.01f,4}));
    assert(!contains(b,{2,3,7.01f}));
    assert(!contains(b,{std::numeric_limits<float>::quiet_NaN(),0,0}));
    assert(!contains({{},{-1,1,1}},{}));
    for(auto from: {Vector3{100,1,100},Vector3{-100,-50,-100},Vector3{2,3,4}})
        for(auto p:objective_points(b,from)) assert(contains(b,p));
    PathCadence path_policy;
    assert(!path_policy.due(0,1,4,0));assert(!path_policy.due(1,0,4,0));
    assert(!path_policy.due(1,1,3,0));assert(path_policy.due(1,1,4,0));
    assert(!path_policy.due(1,1,4,499));assert(path_policy.due(1,1,4,500));
    assert(path_policy.due(1,2,4,500));assert(path_policy.due(1,1,6,501));
    assert(path_policy.due(2,1,6,502));
    PathCadence bounded;
    for(uintptr_t bot=1;bot<=32;++bot)assert(bounded.due(1,bot,4,0));
    assert(!bounded.due(1,33,4,0));assert(bounded.due(1,33,4,10500));
    Snapshot s{1,true,true,true,true,4,10,1,0,1,0};
    Cadence c;
    assert(!c.due(s,0)); assert(!c.due(s,9999)); assert(c.due(s,10000));
    assert(!c.due(s,10000)); assert(c.due(s,600000)); assert(!c.due(s,600001));
    s.status=5; assert(!c.due(s,610000)); s.status=6; assert(c.due(s,610001));
    s.ready=false; assert(!c.due(s,630000)); s.ready=true;
    s.humans=10; assert(!c.due(s,630000)); s.humans=2; s.bots=9;
    assert(overflow(s)==-1); assert(!c.due(s,630000));
    s.bots=0; s.team1=6; assert(overflow(s)==1);
    s.team1=1; s.team2=6; assert(overflow(s)==2); s.team2=0;
    s.status=3; assert(c.due(s,640000));
    for(int status:{0,1,2,7,8,-1}) {s.status=status;assert(!c.due(s,640000));}
    s.status=4;s.humans=1;assert(!c.due(s,650000));assert(c.due(s,660000));
    s.master=false;assert(!c.due(s,670000));s.master=true;assert(!c.due(s,670001));
    s.online=false;assert(!c.due(s,700000));s.online=true;assert(!c.due(s,700001));
    s.room=2;assert(!c.due(s,720000));assert(c.due(s,730000));
    s.enabled=false;assert(!c.due(s,740000));
    using namespace escort_bots_2313::detail;
    assert(!escort_bots_2313::install());
    stock_rosters=stock;rosters(nullptr,nullptr);assert(stock_calls==1);stock_calls=0;
    stock_siege_update=stock;stock_siege_awake=stock;stock_tram_update=stock;stock_choose_point=stock;
    siege_update(nullptr,nullptr);awake(nullptr,nullptr);tram_update(nullptr,nullptr);
    assert(stock_calls==3); choose_point(nullptr,nullptr); assert(stock_calls==4);
    assert(!may_spawn(nullptr,nullptr));
    stock_may_spawn=+[](void*,const void*){return true;};
    assert(may_spawn(nullptr,nullptr));
    assert(!handles()); assert(!manager_update(nullptr,nullptr));
    // AOT spawn callers omit this. Never inspect the poisoned pseudo-instance.
    {
        using namespace harness;
        mode_field=&mode_key;offline_field=&offline_key;siege_instance=&siege_key;siege_status=&status_key;
        il2cpp::field_static_get_value=&statics;il2cpp::field_get_value=&fields;
        object_alive={reinterpret_cast<void*>(&harness::alive),nullptr};
        state_get={reinterpret_cast<void*>(&get_state),nullptr};
        feature_get={reinterpret_cast<void*>(&flags),nullptr};
        feature_flag={reinterpret_cast<void*>(&feature),nullptr};
        team_strike_bots_2313::detail::private_room={reinterpret_cast<void*>(&harness::private_room),nullptr};
        escort_active=true;
        for (int phase=0;phase<=8;++phase) {
            status=phase;
            assert(may_spawn(reinterpret_cast<void*>(1),nullptr)==playing(phase));
        }
        status=4;siege=nullptr;assert(!may_spawn(reinterpret_cast<void*>(1),nullptr));siege=&dummy;
        stock_may_spawn=+[](void*,const void*){return false;};
        mode=16;assert(!may_spawn(reinterpret_cast<void*>(1),nullptr));mode=17;
        offline=true;assert(!may_spawn(reinterpret_cast<void*>(1),nullptr));offline=false;
        custom=true;assert(!may_spawn(reinterpret_cast<void*>(1),nullptr));custom=false;
        flag=false;assert(!may_spawn(reinterpret_cast<void*>(1),nullptr));flag=true;
        Items values;
        il2cpp::object_get_class=&klass;il2cpp::class_get_method_from_name=&method;
        List list(&values);int a=0,b=0;
        assert(append_unique(list,&a));assert(append_unique(list,&a));assert(list.count()==1);
        assert(append_unique(list,&b));assert(list.count()==2);assert(!append_unique(list,nullptr));
        List missing(nullptr);assert(!append_unique(missing,&a));
        escort_active=false;
        mode_field=offline_field=siege_instance=siege_status=nullptr;
    }
    // Real movement hook dispatch: both rounds, all three behaviors, ownership,
    // dead-avatar/phase guards, bounded complete-path retry, stock fallbacks.
    {
        using namespace movement_harness;
        mode_field=key(0);offline_field=key(1);siege_instance=key(2);siege_status=key(3);
        behavior_ai=key(4);ai_controller=key(5);controller_view=key(6);entity_field=key(7);
        entity_player=key(8);tram_instance=key(9);tram_object=key(10);behavior_nav=key(11);
        pmc_dead=key(12);detect_bounds=key(13);skirmish_acceleration=key(14);
        behavior_point=key(15);behavior_hold=key(16);behavior_path_aim=key(17);
        il2cpp::field_static_get_value=&movement_harness::statics;
        il2cpp::field_get_value=&movement_harness::fields;
        il2cpp::field_set_value.raw=&movement_harness::set;
        view_is_mine={reinterpret_cast<void*>(&owner),nullptr};
        room_get={reinterpret_cast<void*>(&room),nullptr};
        component_transform={reinterpret_cast<void*>(&movement_harness::transform),nullptr};
        escort_bots_2313::detail::position={reinterpret_cast<void*>(&movement_harness::position),nullptr};
        inverse_point=world_point={reinterpret_cast<void*>(&point),nullptr};
        path_to={reinterpret_cast<void*>(&path),nullptr};ai_speed={reinterpret_cast<void*>(&speed),nullptr};
        nav_speed=nav_acceleration={reinterpret_cast<void*>(&set_float),nullptr};
        escort_active=true;
        for(int stage:{4,6})for(int behavior:{1,2,3}) {
            phase=stage;path_cadence={};paths=sets=0;hold=true;
            assert(objective_tick(obj(0),behavior));assert(paths==1 && !hold);
            assert(path_aim==(behavior!=3));assert(contains({{},{3,3,3}},destination));
            assert(objective_tick(obj(0),behavior));assert(paths==1);
        }
        path_ok=false;paths=0;path_cadence={};assert(objective_tick(obj(0),3));assert(paths==5);path_ok=true;
        bot_position={1,0,1};
        assert(!objective_tick(obj(0),3));assert(!objective_tick(obj(0),2));
        assert(!objective_tick(obj(0),1));
        bot_position={100,0,100};
        mine=false;assert(!objective_tick(obj(0),3));mine=true;
        dead=true;assert(!objective_tick(obj(0),3));dead=false;
        for(int other_mode:{0,2,16,18}) {game_mode=other_mode;assert(!objective_tick(obj(0),3));}game_mode=17;
        movement_harness::offline=true;assert(!objective_tick(obj(0),3));movement_harness::offline=false;
        harness::custom=true;assert(!objective_tick(obj(0),3));harness::custom=false;
        harness::flag=false;assert(!objective_tick(obj(0),3));harness::flag=true;
        harness::state=8;assert(!objective_tick(obj(0),3));harness::state=9;
        for(int phase_value:{0,1,2,3,5,7,8}) {phase=phase_value;assert(!objective_tick(obj(0),3));}
        escort_active=false;
        stock_walk_tick=stock_search_tick=stock_skirmish_tick=stock;
        int previous=stock_calls;walk_tick(nullptr,nullptr);search_tick(nullptr,nullptr);skirmish_tick(nullptr,nullptr);
        assert(stock_calls==previous+3);stock_calls=4;
        mode_field=offline_field=nullptr;
    }
    // Shared manager hook dispatch does not double-hook or swallow other modes.
    using namespace team_strike_bots_2313::detail;
    original_update=stock; original_initial_fill=stock;
    additional_update=+[](void*,const void*) {return false;};
    additional_initial_fill=+[](){return false;};
    update(nullptr,nullptr);initial_fill(nullptr,nullptr);assert(stock_calls==6);
    additional_update=+[](void*,const void*){return true;};
    additional_initial_fill=+[](){return true;};
    update(nullptr,nullptr);initial_fill(nullptr,nullptr);assert(stock_calls==6);
}

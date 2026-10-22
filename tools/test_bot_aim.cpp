#include <cassert>
#include <limits>
#include <cstring>
#include "../opg3d/src/main/cpp/bot_aim_2313.h"
namespace il2cpp {
void* find_class(const char*,const char*) {return nullptr;}
void* find_field(const char*,const char*,const char*) {return nullptr;}
void* find_method_info(const char*,const char*,const char*,int) {return nullptr;}
void* find_method_by_address(const char*,const char*,void*) {return nullptr;}
void* method_pointer(void* p) {return p;}
}
namespace hook {bool install(const ManagedMethod&,void*,void**,bool) {return false;}}
namespace elfsym {bool find_library(const char*,uintptr_t*) {return false;}}
namespace test {
int keys[10],objects[7],mode=17,late_calls=0,destroy_calls=0,point_calls=0,writes=0;
bool mine=true,dead=false,testing=false,has_target=true,has_player=true;
void* o(int i){return &objects[i];}void* k(int i){return &keys[i];}
void fields(void*,void* key,void* out) {
    using namespace bot_aim_2313::detail;
    if(key==test_field){*static_cast<bool*>(out)=testing;return;}
    if(key==dead_field){*static_cast<bool*>(out)=dead;return;}
    void* v=nullptr;
    if(key==controller_field)v=o(1);else if(key==view_field)v=o(2);
    else if(key==entity_field)v=o(3);else if(key==player_field)v=has_player?o(4):nullptr;
    else if(key==detector_field)v=o(5);else if(key==target_field)v=has_target?o(6):nullptr;
    else assert(false);
    *static_cast<void**>(out)=v;
}
void statics(void*,void* out){*static_cast<int*>(out)=mode;}
void set(void*,void* key,void* v){assert(key==bot_aim_2313::detail::cached_distance_field);assert(*static_cast<float*>(v)==-1);++writes;}
bool alive(void* v,const void*){return v!=nullptr;}
bool owner(void*,const void*){return mine;}
float delta(const void*){return 1.0f/60;}
float stock(void*,const void*){return 2;}
bot_aim_2313::detail::Vector3 point(void*,const void*){++point_calls;return {1,2,3};}
void destroy(void*,const void*){++destroy_calls;}
void late(void* self,const void*){
    using namespace bot_aim_2313::detail;
    ++late_calls;
    const bool enhanced=active.load() && mine && !dead && !testing && has_target && has_player;
    const float speed=tracking(self,nullptr),e=error(self,nullptr);
    assert(enhanced ? speed>2 && speed*delta(nullptr)<0.36f : speed==2);
    assert(enhanced ? e>0 && e<2 : e==2);
    auto p=bot_aim_2313::detail::point(self,nullptr);assert(p.x==1 && p.y==2 && p.z==3);
}
}
int main(){
    using namespace bot_aim;
    const float nan=std::numeric_limits<float>::quiet_NaN();
    assert(std::isnan(tracking(nan,17,0.01f)));assert(std::isnan(error(nan,0)));
    assert(tracking(0,17,.02f)==0);assert(tracking(-1,17,.02f)==-1);
    assert(tracking(2,17,0)==2);assert(tracking(2,17,nan)==2);
    assert(error(0,17)==0);assert(error(-1,17)==-1);
    for(int mode:{0,2,16,17,18}){
        assert(error(.1f,mode)>0 && error(.1f,mode)<.1f);
        assert(error(.001f,mode)==.001f);
        for(float dt:{1.f/120,1.f/60,1.f/30,.1f,1.f}){
            float a=tracking(2,mode,dt)*dt;assert(a>0 && a<=.350001f);
        }
        float remain[3]{};int n=0;
        for(int fps:{30,60,120}) {float r=1,dt=1.f/fps;for(int i=0;i<fps;i++)r*=1-tracking(2,mode,dt)*dt;remain[n++]=r;}
        assert(std::abs(remain[0]-remain[1])<1e-5f && std::abs(remain[1]-remain[2])<1e-5f);
    }
    assert(tracking(2,16,.016f)>tracking(2,0,.016f));
    assert(error(.1f,17)<error(.1f,0));
    Refresh r;assert(!r.due(0,1,17,0));assert(!r.due(1,0,17,0));
    assert(r.due(1,2,17,0));assert(!r.due(1,2,17,64));assert(r.due(1,2,17,65));
    assert(r.due(1,3,17,66));assert(!r.due(1,3,0,165));assert(r.due(1,3,0,166));
    r.forget(1);assert(r.due(1,3,0,167));assert(r.due(1,3,0,0));
    Refresh full;for(int i=1;i<=64;i++)assert(full.due(i,1,0,0));
    assert(!full.due(65,1,0,1));assert(full.due(65,1,0,10001));
    using namespace bot_aim_2313::detail;
    controller_field=test::k(0);view_field=test::k(1);entity_field=test::k(2);player_field=test::k(3);
    dead_field=test::k(4);detector_field=test::k(5);target_field=test::k(6);
    cached_distance_field=test::k(7);mode_field=test::k(8);test_field=test::k(9);
    il2cpp::field_get_value=&test::fields;il2cpp::field_static_get_value=&test::statics;il2cpp::field_set_value.raw=&test::set;
    alive_get={reinterpret_cast<void*>(&test::alive),nullptr};mine_get={reinterpret_cast<void*>(&test::owner),nullptr};delta_get={reinterpret_cast<void*>(&test::delta),nullptr};
    stock_tracking=&test::stock;stock_error=&test::stock;stock_point=&test::point;stock_late=&test::late;stock_destroy=&test::destroy;
    assert(!bot_aim_2313::install());active.store(true);
    assert(tracking(test::o(0),nullptr)==2);assert(error(test::o(0),nullptr)==2);
    for(int mode:{0,2,16,17,18}){test::mode=mode;late(test::o(0),nullptr);assert(!context.bot);}
    assert(test::writes>=1);
    const int before=test::writes;
    test::mine=false;late(test::o(0),nullptr);test::mine=true;
    test::dead=true;late(test::o(0),nullptr);test::dead=false;
    test::testing=true;late(test::o(0),nullptr);test::testing=false;
    test::has_target=false;late(test::o(0),nullptr);test::has_target=true;
    test::has_player=false;late(test::o(0),nullptr);test::has_player=true;
    active.store(false);late(test::o(0),nullptr);
    assert(test::writes==before);assert(test::late_calls==11);assert(test::point_calls==11);
    destroy(test::o(0),nullptr);assert(test::destroy_calls==1);
}

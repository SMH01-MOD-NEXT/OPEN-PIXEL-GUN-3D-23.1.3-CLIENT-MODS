#pragma once
#include <atomic>
#include <chrono>
#include "bot_aim_policy.h"
#include "elf_sym.h"
#include "hook.h"
#include "il2cpp.h"
#include "log.h"
namespace bot_aim_2313 {
namespace detail {
using Void=void (*)(void*,const void*);
using Float=float (*)(void*,const void*);
struct Vector3 {float x,y,z;};
using Point=Vector3 (*)(void*,const void*);
struct Binding {void* code=nullptr;const void* info=nullptr;};
inline Binding alive_get,mine_get,delta_get;
inline void *controller_field,*view_field,*entity_field,*player_field,*dead_field;
inline void *detector_field,*target_field,*cached_distance_field,*mode_field,*test_field;
inline Void stock_late=nullptr,stock_destroy=nullptr;
inline Float stock_tracking=nullptr,stock_error=nullptr;
inline Point stock_point=nullptr;
inline std::atomic<bool> active{false};
inline bot_aim::Refresh refresh;
// Only getters reached from this bot's real stock combat LateUpdate are tuned.
// Unrelated callers, idle path following, player aim and remote avatars pass through.
struct Context {void* bot=nullptr;void* target=nullptr;int mode=0;};
inline thread_local Context context;
inline int64_t next_report=0,next_tracking_report=0;
inline unsigned refreshed=0;
inline int64_t now_ms() {return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
template<class T> inline T field(void* o,void* f) {T v{};if(o&&f)il2cpp::field_get_value(o,f,&v);return v;}
inline bool alive(void* o) {return o && reinterpret_cast<bool (*)(void*,const void*)>(alive_get.code)(o,alive_get.info);}
inline Context eligible(void* self) {
    if(!active.load() || !self || field<bool>(self,test_field)) return {};
    void* controller=field<void*>(self,controller_field);
    if(!alive(controller)) return {};
    void* view=field<void*>(controller,view_field);
    if(!alive(view) || !reinterpret_cast<bool (*)(void*,const void*)>(mine_get.code)(view,mine_get.info)) return {};
    void* entity=field<void*>(controller,entity_field);
    void* player=alive(entity)?field<void*>(entity,player_field):nullptr;
    if(!alive(player) || field<bool>(player,dead_field)) return {};
    void* detector=field<void*>(self,detector_field);
    void* target=alive(detector)?field<void*>(detector,target_field):nullptr;
    // Target is an interface, not necessarily a Unity Object: never use
    // Object.op_Implicit on it. Stock LateUpdate validates target type/death/LOS.
    if(!target) return {};
    int mode=0;il2cpp::field_static_get_value(mode_field,&mode);
    return {self,target,mode};
}
inline void late(void* self,const void* info) {
    const Context previous=context;
    context=eligible(self);
    if(stock_late) stock_late(self,info);
    context=previous;
}
inline float tracking(void* self,const void* info) {
    const float stock=stock_tracking?stock_tracking(self,info):0;
    if(!active.load() || context.bot!=self || !context.target) return stock;
    const float dt=reinterpret_cast<float (*)(const void*)>(delta_get.code)(delta_get.info);
    const float tuned=bot_aim::tracking(stock,context.mode,dt);
    const auto now=now_ms();
    if(now>=next_tracking_report) {
        LOGI("23.1.3-bot-aim: tracking mode=%d stock=%.3f tuned=%.3f dt=%.4f fraction=%.3f",context.mode,stock,tuned,dt,tuned*dt);
        next_tracking_report=now+10000;
    }
    return tuned;
}
inline float error(void* self,const void* info) {
    const float stock=stock_error?stock_error(self,info):0;
    return active.load() && context.bot==self && context.target?bot_aim::error(stock,context.mode):stock;
}
inline Vector3 point(void* self,const void* info) {
    if(active.load() && context.bot==self && context.target) {
        const auto now=now_ms();
        if(refresh.due(reinterpret_cast<uintptr_t>(self),reinterpret_cast<uintptr_t>(context.target),context.mode,now)) {
            // 214541C otherwise reuses B0 when its scalar AC key is unchanged.
            // -1 cannot equal the valid nonnegative distance/radius sum; stock
            // recomputes its own target point, random error and weapon prediction.
            float invalid=-1;il2cpp::field_set_value(self,cached_distance_field,&invalid);
            ++refreshed;
            if(now>=next_report) {
                LOGI("23.1.3-bot-aim: stock target refreshes=%u mode=%d interval=%dms; bounded tracking, nonzero stock error",refreshed,context.mode,bot_aim::refresh_ms(context.mode));
                next_report=now+10000;
            }
        }
    }
    return stock_point?stock_point(self,info):Vector3{};
}
inline void destroy(void* self,const void* info) {
    refresh.forget(reinterpret_cast<uintptr_t>(self));
    if(stock_destroy)stock_destroy(self,info);
}
inline Binding bind(uintptr_t base,const char* ns,const char* cl,uintptr_t rva) {
    void* m=il2cpp::find_method_by_address(ns,cl,reinterpret_cast<void*>(base+rva));
    return m?Binding{il2cpp::method_pointer(m),m}:Binding{};
}
} // namespace detail
inline bool install() {
    using namespace detail;
    uintptr_t base=0;if(!elfsym::find_library("libil2cpp.so",&base))return false;
    if(!il2cpp::field_get_value || !il2cpp::field_set_value || !il2cpp::field_static_get_value)return false;
    struct F {void** out;const char* ns;const char* cl;const char* name;};
    F fields[]={
        {&controller_field,"PlayerBot","AIBotController","playerBotController"},
        {&test_field,"PlayerBot","AIBotController","useTestBehavior"},
        {&detector_field,"PlayerBot","AIBotController","_targetDetector"},
        {&target_field,"PlayerBot","AIBotTargetDetector",u8"丙丂丙丏丈上丏丑丗"},
        {&cached_distance_field,"PlayerBot","AIBotController",u8"七丌丙丁丝丝与与万"},
        {&view_field,"","PlayerBotController",u8"丌丅丝下丈世丈丐且"},
        {&entity_field,"","PlayerBotController","player"},
        {&player_field,"","PlayerBotEntity",u8"不世三丌丄下丗与丂"},
        {&dead_field,"","Player_move_c",u8"不不与丏丑丗丛东丙"},
        {&mode_field,"","GameConnect",u8"万丘丌丝一不丒万一"}
    };
    for(auto& f:fields){*f.out=il2cpp::find_field(f.ns,f.cl,f.name);if(!*f.out)return false;}
    alive_get=bind(base,"UnityEngine","Object",0x44081FC);
    mine_get=bind(base,"","PhotonView",0x44720A8);
    delta_get=bind(base,"UnityEngine","Time",0x441F890);
    for(auto b:{alive_get,mine_get,delta_get})if(!b.code)return false;
    struct H {const char* name;uintptr_t rva;void* replacement;void** original;};
    H hooks[]={
        {"LateUpdate",0x2144D84,reinterpret_cast<void*>(&late),reinterpret_cast<void**>(&stock_late)},
        {u8"万丞丞下东丌下丞七",0x2143A64,reinterpret_cast<void*>(&tracking),reinterpret_cast<void**>(&stock_tracking)},
        {u8"丝丏丟丘与丐丌且与",0x2146880,reinterpret_cast<void*>(&error),reinterpret_cast<void**>(&stock_error)},
        {u8"丄上丘丄丏且七丐丑",0x214541C,reinterpret_cast<void*>(&point),reinterpret_cast<void**>(&stock_point)},
        {"OnDestroy",0x21459CC,reinterpret_cast<void*>(&destroy),reinterpret_cast<void**>(&stock_destroy)}
    };
    for(auto& h:hooks){auto b=bind(base,"PlayerBot","AIBotController",h.rva);if(!b.code || il2cpp::method_pointer(il2cpp::find_method_info("PlayerBot","AIBotController",h.name,0))!=b.code)return false;}
    for(auto& h:hooks)if(!hook::install({"PlayerBot","AIBotController",h.name,0},h.replacement,h.original,true))return false;
    active.store(true);
    LOGI("23.1.3-bot-aim: exact-build owned stock combat tracking; stronger Team Strike/Escort");
    return true;
}
} // namespace bot_aim_2313

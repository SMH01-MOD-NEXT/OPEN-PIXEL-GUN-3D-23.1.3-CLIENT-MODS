#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
namespace bot_aim {
inline bool stronger(int mode) { return mode==16 || mode==17; }
inline int refresh_ms(int mode) { return stronger(mode)?65:100; }
// Stock Slerp still performs the actual rotation. Bound its per-frame fraction
// even during a stalled frame: never turn this into an instantaneous target lock.
inline float tracking(float stock,int mode,float dt) {
    if (!std::isfinite(stock) || stock<=0 || !std::isfinite(dt) || dt<=0) return stock;
    const float rate=std::clamp(stock*(stronger(mode)?2.8f:1.8f),
                              stronger(mode)?9.0f:6.0f,stronger(mode)?12.0f:9.0f);
    return std::min(-std::expm1(-rate*dt),0.35f)/dt;
}
// Scale stock random aim error, not weapon spread/damage. A positive stock
// error stays positive, and an already very accurate preset is not made worse.
inline float error(float stock,int mode) {
    if (!std::isfinite(stock) || stock<=0) return stock;
    return std::min(stock,std::max(0.002f,stock*(stronger(mode)?0.25f:0.45f)));
}
class Refresh {
    struct Slot { uintptr_t bot=0,target=0; int64_t last=0; };
    std::array<Slot,64> slots_{};
public:
    bool due(uintptr_t bot,uintptr_t target,int mode,int64_t now) {
        if(!bot || !target) return false;
        Slot* p=nullptr;
        for(auto& s:slots_) if(s.bot==bot) {p=&s;break;}
        if(!p) for(auto& s:slots_) if(!s.bot || now-s.last>10000) {p=&s;break;}
        if(!p) return false;
        if(p->bot==bot && p->target==target && now>=p->last && now-p->last<refresh_ms(mode)) return false;
        *p={bot,target,now}; return true;
    }
    void forget(uintptr_t bot) {for(auto& s:slots_) if(s.bot==bot) {s={};break;}}
};
} // namespace bot_aim

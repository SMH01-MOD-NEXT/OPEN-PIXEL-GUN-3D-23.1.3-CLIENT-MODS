#pragma once
#include <cmath>
#include <array>
#include <algorithm>
#include <cstdint>
namespace escort_bots {
struct Vector3 { float x = 0, y = 0, z = 0; };
struct Bounds { Vector3 center, extents; };
inline bool finite(Vector3 p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }
inline bool contains(Bounds b, Vector3 p) {
    return finite(p) && finite(b.center) && finite(b.extents) &&
        b.extents.x >= 0 && b.extents.y >= 0 && b.extents.z >= 0 &&
        std::abs(p.x-b.center.x) <= b.extents.x &&
        std::abs(p.y-b.center.y) <= b.extents.y && std::abs(p.z-b.center.z) <= b.extents.z;
}
inline bool playing(int status) { return status == 4 || status == 6; }
inline bool stage(int status) { return status >= 3 && status <= 6; }
// All candidates remain inside the real stock proximity volume. Start at the
// nearest inset point, not the ram's potentially unwalkable/colliding centre.
inline std::array<Vector3,5> objective_points(Bounds b, Vector3 from) {
    const float x=b.extents.x*0.65f, z=b.extents.z*0.65f;
    const float y=std::clamp(from.y,b.center.y-b.extents.y,b.center.y+b.extents.y);
    return {{{std::clamp(from.x,b.center.x-x,b.center.x+x),y,
              std::clamp(from.z,b.center.z-z,b.center.z+z)},
             {b.center.x+x,y,b.center.z}, {b.center.x-x,y,b.center.z},
             {b.center.x,y,b.center.z+z}, {b.center.x,y,b.center.z-z}}};
}
// Fixed-size, unmanaged keys only: no GC roots, allocations or unbounded maps.
// A behavior transition must not multiply the pathfinding frequency per bot.
class PathCadence {
    struct Slot { uintptr_t bot=0; int phase=0; int64_t next=0; };
    std::array<Slot,32> slots_{};
    uintptr_t room_=0;
public:
    bool due(uintptr_t room,uintptr_t bot,int phase,int64_t now) {
        if (!room || !bot || !playing(phase)) return false;
        if (room_!=room) { slots_={}; room_=room; }
        Slot* slot=nullptr;
        for (auto& s:slots_) if (s.bot==bot) {slot=&s;break;}
        if (!slot) for (auto& s:slots_) if (!s.bot || now>=s.next+10000) {slot=&s;break;}
        if (!slot) return false; // bounded fail-closed under unexpected capacity
        if (slot->bot==bot && slot->phase==phase && now<slot->next) return false;
        *slot={bot,phase,now+500}; return true;
    }
};
struct Snapshot {
    uintptr_t room = 0;
    bool online = false, master = false, enabled = false, ready = false;
    int status = 0, capacity = 0, humans = 0, bots = 0, team1 = 0, team2 = 0;
};
inline int overflow(const Snapshot& s) {
    if (s.capacity < 2) return 0;
    const int half = (s.capacity + 1) / 2;
    if (s.team1 > half) return 1;
    if (s.team2 > half) return 2;
    return s.humans + s.bots > s.capacity ? -1 : 0;
}
class Cadence {
    uintptr_t room_ = 0;
    int64_t next_ = 0;
public:
    bool due(const Snapshot& s, int64_t now) {
        if (!s.online || !s.master || !s.enabled || !s.room || !stage(s.status)) {
            room_ = 0; next_ = 0; return false;
        }
        if (room_ != s.room) { room_ = s.room; next_ = now + 10000; return false; }
        // Initial StartDefend waits for enough participants: allow its roster fill.
        // Do not spawn during StartAttack, the stock side-change transaction.
        if ((s.status != 3 && !playing(s.status)) || !s.ready || s.capacity < 2 || s.humans < 1 || s.bots < 0 ||
            s.humans+s.bots >= s.capacity || overflow(s) || now < next_) return false;
        next_ = now + 10000;
        return true;
    }
};
} // namespace escort_bots

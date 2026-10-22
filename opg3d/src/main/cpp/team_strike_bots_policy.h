#pragma once
#include <algorithm>
#include <cstdint>

namespace team_strike_bots {
// Pure policy: no managed references, no wall-clock catch-up bursts.
struct Snapshot {
    uintptr_t room = 0;
    bool master = false, enabled = false, roster_ready = false;
    int status = 0, capacity = 0, humans = 0, bots = 0;
    int blue = 0, red = 0;
    bool online = false;
};
class Cadence {
    uintptr_t room_ = 0;
    bool master_ = false;
    int64_t next_ = 0;
public:
    bool due(const Snapshot& s, int64_t now) {
        if (!s.online || !s.room || !s.master || !s.enabled || s.status < 1 || s.status > 4) {
            room_ = 0; master_ = false; next_ = 0; return false;
        }
        if (room_ != s.room || !master_) {
            room_ = s.room; master_ = true; next_ = now + 10000; return false;
        }
        // NewRound is the stock five-second spawn/countdown transaction. Do not
        // race its roster walk. Playing additions have no avatar until next round.
        if (s.status == 4 || !s.roster_ready || s.capacity < 2 || s.humans < 1 ||
            s.humans + s.bots >= s.capacity ||
            (s.blue >= s.capacity / 2 && s.red >= s.capacity / 2)) return false;
        if (now < next_) return false;
        next_ = now + 10000; // Reserve before Photon calls; no re-entrant duplicate.
        return true;
    }
};
inline int capacity(int room_max) { return room_max < 2 ? 0 : std::min(room_max / 2, 4) * 2; }
inline bool may_remove(int status) { return status == 1 || status == 3; }
inline int overflow_team(const Snapshot& s) {
    const int half = s.capacity / 2;
    if (s.blue > half) return 1;
    if (s.red > half) return 2;
    return s.humans + s.bots > s.capacity ? -1 : 0;
}
} // namespace team_strike_bots

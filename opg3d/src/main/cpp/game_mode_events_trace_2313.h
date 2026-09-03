#pragma once

#include <array>
#include <atomic>
#include <climits>
#include <cstdint>
#include <mutex>

#include "elf_sym.h"
#include "hook.h"
#include "log.h"

// Passive trace for the three GameModeEvents config readers started by the
// singleton constructor (ConfigId 4, 125 and 126). It observes the iterator's
// state and the stock static ready flag; it never changes an argument, field,
// return value or control-flow decision.
namespace game_mode_events_trace_2313 {
namespace detail {

using MethodInfo = void;
using MoveNextFn = bool (*)(void* self, const MethodInfo* method);

constexpr uintptr_t kMoveNextRva = 0x2BC00E8u;
constexpr uintptr_t kClassSlotRva = 0x6844888u;
constexpr uintptr_t kClassStaticFieldsOffset = 0xB8u;
constexpr uintptr_t kStateOffset = 0x10u;
constexpr uintptr_t kCurrentOffset = 0x18u;
constexpr uintptr_t kOwnerOffset = 0x20u;
constexpr uintptr_t kConfigIdOffset = 0x28u;
constexpr uintptr_t kClosureOffset = 0x30u;
constexpr uintptr_t kRootEventsOffset = 0x48u;
constexpr uintptr_t kLoopIndexOffset = 0x50u;
constexpr uintptr_t kEnumeratorOffset = 0x58u;
constexpr uintptr_t kListSizeOffset = 0x18u;
constexpr uintptr_t kReadyOffset = 0x10u;
constexpr uintptr_t kMinPointer = 0x100000000ull;
constexpr std::size_t kTrackCapacity = 8;

struct Track {
    void* self = nullptr;
    int32_t last_state = INT32_MIN;
    bool last_ready = false;
    bool ready_valid = false;
    uint64_t calls = 0;
};

inline uintptr_t g_il2cpp_base = 0;
inline MoveNextFn g_move_next = nullptr;
inline std::array<Track, kTrackCapacity> g_tracks{};
inline std::mutex g_tracks_mutex;

inline bool plausible(const void* pointer, uintptr_t alignment_mask = 0x7u) {
    const uintptr_t value = reinterpret_cast<uintptr_t>(pointer);
    return value >= kMinPointer && (value & alignment_mask) == 0u;
}

inline int32_t read_i32(void* object, uintptr_t offset) {
    if (!plausible(object)) return INT32_MIN;
    return *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(object) +
                                      offset);
}

inline void* read_ref(void* object, uintptr_t offset) {
    if (!plausible(object)) return nullptr;
    return *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(object) +
                                     offset);
}

inline bool read_ready(bool* out) {
    if (out == nullptr || g_il2cpp_base == 0) return false;
    void** cell = *reinterpret_cast<void***>(g_il2cpp_base + kClassSlotRva);
    if (!plausible(cell)) return false;
    void* klass = *cell;
    if (!plausible(klass)) return false;
    void* statics = *reinterpret_cast<void**>(
        reinterpret_cast<uint8_t*>(klass) + kClassStaticFieldsOffset);
    if (!plausible(statics, 0x3u)) return false;
    *out = *(reinterpret_cast<uint8_t*>(statics) + kReadyOffset) != 0;
    return true;
}

inline const char* config_name(int32_t id) {
    switch (id) {
        case 4: return "GameModeEvents";
        case 125: return "CompetionGameModeEvents";
        case 126: return "RotationGameModeEvents";
        default: return "<other>";
    }
}

inline Track* find_track_locked(void* self) {
    Track* empty = nullptr;
    for (Track& track : g_tracks) {
        if (track.self == self) return &track;
        if (track.self == nullptr && empty == nullptr) empty = &track;
    }
    if (empty != nullptr) empty->self = self;
    return empty;
}

inline bool checkpoint(uint64_t calls) {
    return calls <= 3 || calls == 10 || calls == 100 || calls == 1000 ||
           calls == 5000;
}

inline bool hook_move_next(void* self, const MethodInfo* method) {
    const int32_t before = read_i32(self, kStateOffset);
    const int32_t config_id = read_i32(self, kConfigIdOffset);
    void* closure_before = read_ref(self, kClosureOffset);
    void* root_events_before = read_ref(self, kRootEventsOffset);
    const int32_t root_count_before =
        read_i32(root_events_before, kListSizeOffset);
    const int32_t loop_index_before = read_i32(self, kLoopIndexOffset);
    void* enumerator_before = read_ref(self, kEnumeratorOffset);
    bool ready_before = false;
    const bool ready_before_valid = read_ready(&ready_before);

    uint64_t calls = 0;
    int32_t last_state = INT32_MIN;
    bool last_ready = false;
    bool last_ready_valid = false;
    {
        std::lock_guard<std::mutex> lock(g_tracks_mutex);
        Track* track = find_track_locked(self);
        if (track != nullptr) {
            calls = ++track->calls;
            last_state = track->last_state;
            last_ready = track->last_ready;
            last_ready_valid = track->ready_valid;
        }
    }

    const bool result = g_move_next != nullptr ? g_move_next(self, method) : false;
    const int32_t after = read_i32(self, kStateOffset);
    void* current = read_ref(self, kCurrentOffset);
    void* owner = read_ref(self, kOwnerOffset);
    void* closure_after = read_ref(self, kClosureOffset);
    void* root_events_after = read_ref(self, kRootEventsOffset);
    const int32_t root_count_after =
        read_i32(root_events_after, kListSizeOffset);
    const int32_t loop_index_after = read_i32(self, kLoopIndexOffset);
    void* enumerator_after = read_ref(self, kEnumeratorOffset);
    bool ready_after = false;
    const bool ready_after_valid = read_ready(&ready_after);

    const bool ready_changed =
        ready_after_valid && (!last_ready_valid || ready_after != last_ready);
    const bool state_changed = last_state == INT32_MIN || after != last_state ||
                               before != after;
    if (checkpoint(calls) || state_changed || ready_changed || !result) {
        LOGI("23.1.3-gme: self=%p id=%d(%s) call=%llu state=%d->%d "
             "ret=%d current=%p owner=%p root=%p->%p rootCount=%d->%d "
             "loopIndex=%d->%d closure=%p->%p enumerator=%p->%p "
             "ready=%s%d->%s%d -- passive",
             self, config_id, config_name(config_id),
             static_cast<unsigned long long>(calls), before, after,
             result ? 1 : 0, current, owner, root_events_before,
             root_events_after, root_count_before, root_count_after,
             loop_index_before, loop_index_after, closure_before, closure_after,
             enumerator_before, enumerator_after,
             ready_before_valid ? "" : "?", ready_before ? 1 : 0,
             ready_after_valid ? "" : "?", ready_after ? 1 : 0);
    }

    {
        std::lock_guard<std::mutex> lock(g_tracks_mutex);
        Track* track = find_track_locked(self);
        if (track != nullptr) {
            track->last_state = after;
            if (ready_after_valid) {
                track->last_ready = ready_after;
                track->ready_valid = true;
            }
        }
    }
    return result;
}

}  // namespace detail

inline bool install_hooks() {
    uintptr_t base = 0;
    if (!elfsym::find_library("libil2cpp.so", &base) || base == 0) {
        LOGW("23.1.3-gme: libil2cpp.so base not found");
        return false;
    }
    detail::g_il2cpp_base = base;
    const bool armed = hook::install_address(
        reinterpret_cast<void*>(base + detail::kMoveNextRva),
        "GameModeEvents config iterator MoveNext",
        reinterpret_cast<void*>(&detail::hook_move_next),
        reinterpret_cast<void**>(&detail::g_move_next), false);
    LOGI("23.1.3-gme: passive iterator trace %s", armed ? "armed" : "FAILED");
    return armed;
}

}  // namespace game_mode_events_trace_2313

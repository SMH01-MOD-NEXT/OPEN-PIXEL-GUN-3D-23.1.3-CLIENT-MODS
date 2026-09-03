#pragma once

// Loading-bar widget attribution for PG3D 23.1.3.
//
// Why this exists. The boot freezes with the on-screen bar at 90%, and every
// candidate owner of that number has now been eliminated statically:
//
//   * Switcher's own bar field (Switcher+0x138, the value its iterators yield)
//     tops out at 0.750 at InitializeSwitcher state 45 and is never written
//     again -- 75%, not 90%.
//   * Unity's AsyncOperation.progress cannot be it either. That property caps
//     at 0.9 only while allowSceneActivation is false, and
//     AsyncOperation in this build exposes exactly Finalize,
//     InvokeCompletionEvent, InternalDestroy, get_isDone, get_progress and
//     .ctor. allowSceneActivation was stripped, so nothing can hold a scene
//     load at 0.9.
//   * Its only two readers, at 0x3C948AC and 0x3C94924, both sit inside
//     Google.Play.Common.LoadingScreen.LoadingBar.<FillUntilDone>d__12, the
//     Play Asset Delivery screen, which this boot never shows.
//   * LoadingNGUIController is the map-loading overlay, not the boot one: its
//     nine call sites are ConnectScene, JoinToFriendRoomController,
//     GameController.ShowLoadingGUI, CustomMiniGameController,
//     MainMenuController.ShowConnectionToMap, JoinRoomFromFrends,
//     LoadConnectScene.Awake and LoadingInAfterGame.Awake.
//
// So the widget showing 90% has never been named, and guessing has cost
// several runs. NGUI funnels every bar through UIProgressBar.value (UISlider
// derives from it) and every caption through UILabel.text, so hooking those
// two setters names the widget directly instead of by inference.
//
// This trace is logging only. Both hooks call straight through to the original
// setter and alter no value, no timing and no control flow.
//
// Volume control. A boot drives these setters tens of thousands of times, so
// each line is emitted once per distinct (widget, quantised value) or distinct
// caption, behind a hard budget. Captions are filtered to short strings that
// contain a digit, which is what a percentage looks like.

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>

#include "hook.h"
#include "il2cpp.h"
#include "log.h"

namespace ui_progress_trace_2313 {
namespace detail {

using MethodInfo = void;
using SetValueFn = void (*)(void* self, float value, const MethodInfo* method);
using SetTextFn  = void (*)(void* self, void* text, const MethodInfo* method);

// Filled by hook::install at runtime.
inline SetValueFn g_progress_set_value = nullptr;
inline SetTextFn  g_label_set_text     = nullptr;

static constexpr size_t kSeenSlots = 4096u;

inline std::atomic<uint64_t> g_seen_value[kSeenSlots];
inline std::atomic<uint64_t> g_seen_text[kSeenSlots];

// A stalled boot would otherwise log the same idle repaint forever.
inline std::atomic<int> g_value_budget{800};
inline std::atomic<int> g_text_budget{800};

static uint64_t mix(uint64_t h) noexcept {
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdULL;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53ULL;
    h ^= h >> 33;
    return h;
}

// Single-slot-per-key dedup. A collision costs one extra line, never a missed
// widget, which is the right trade for an attribution trace.
static bool first_time(std::atomic<uint64_t>* table, uint64_t key) noexcept {
    if (key == 0u) key = 1u;
    const size_t slot = static_cast<size_t>(mix(key) % kSeenSlots);
    const uint64_t prev = table[slot].exchange(key, std::memory_order_relaxed);
    return prev != key;
}

static bool take(std::atomic<int>& budget) noexcept {
    return budget.fetch_sub(1, std::memory_order_relaxed) > 0;
}

// UISlider, UIProgressBar and UILabel subclasses all arrive through the same
// setter, so the runtime class name is what separates them.
static const char* class_name_of(void* obj) noexcept {
    if (obj == nullptr) return "<null>";
    if (il2cpp::object_get_class == nullptr ||
        il2cpp::class_get_name == nullptr) {
        return "<no-api>";
    }
    void* klass = il2cpp::object_get_class(obj);
    if (klass == nullptr) return "<no-class>";
    const char* name = il2cpp::class_get_name(klass);
    return name != nullptr ? name : "<anon>";
}

// UIProgressBar.set_value(float). UISlider inherits this, so every NGUI bar in
// the build reports through here.
void hook_progress_set_value(void* self, float value,
                             const MethodInfo* method) {
    const bool finite = std::isfinite(value);
    const int32_t quantised =
        finite ? static_cast<int32_t>(value * 1000.0f) : INT32_MIN;
    const uint64_t key =
        (reinterpret_cast<uint64_t>(self) * 0x9e3779b97f4a7c15ULL) ^
        static_cast<uint64_t>(static_cast<uint32_t>(quantised));
    if (first_time(g_seen_value, key) && take(g_value_budget)) {
        LOGI("23.1.3-uibar: bar %s@%p value=%.3f",
             class_name_of(self), self, static_cast<double>(value));
    }
    if (g_progress_set_value != nullptr) {
        g_progress_set_value(self, value, method);
    }
}

// UILabel.set_text(string). Filtered to short captions containing a digit,
// which is what "90%" and its localised spellings look like.
void hook_label_set_text(void* self, void* text, const MethodInfo* method) {
    if (text != nullptr) {
        const std::string as_text = il2cpp::to_utf8(text, 64);
        bool has_digit = false;
        for (const char c : as_text) {
            if (c >= '0' && c <= '9') {
                has_digit = true;
                break;
            }
        }
        if (has_digit && as_text.size() <= 48u) {
            uint64_t hash = 1469598103934665603ULL;
            for (const char c : as_text) {
                hash ^= static_cast<unsigned char>(c);
                hash *= 1099511628211ULL;
            }
            if (first_time(g_seen_text, hash) && take(g_text_budget)) {
                LOGI("23.1.3-uibar: label %s@%p text='%s'",
                     class_name_of(self), self, as_text.c_str());
            }
        }
    }
    if (g_label_set_text != nullptr) {
        g_label_set_text(self, text, method);
    }
}

template <typename Fn>
static void* repl(Fn fn) { return reinterpret_cast<void*>(fn); }
template <typename Fn>
static void** orig(Fn* p) { return reinterpret_cast<void**>(p); }

} // namespace detail

// Call once after Assembly-CSharp.dll is ready (from main.cpp init_thread).
inline bool install_hooks() {
    int installed = 0;

    if (hook::install({"", "UIProgressBar", "set_value", 1},
                      detail::repl(&detail::hook_progress_set_value),
                      detail::orig(&detail::g_progress_set_value), false)) {
        ++installed;
        LOGI("23.1.3-uibar: UIProgressBar.set_value armed "
             "(covers UISlider and every NGUI bar)");
    } else {
        LOGW("23.1.3-uibar: UIProgressBar.set_value hook failed "
             "-- the 90%% widget will stay unnamed");
    }

    if (hook::install({"", "UILabel", "set_text", 1},
                      detail::repl(&detail::hook_label_set_text),
                      detail::orig(&detail::g_label_set_text), false)) {
        ++installed;
        LOGI("23.1.3-uibar: UILabel.set_text armed (digit-bearing captions)");
    } else {
        LOGW("23.1.3-uibar: UILabel.set_text hook failed "
             "-- a percentage caption will not be reported");
    }

    LOGI("23.1.3-uibar: %d/2 UI progress hooks installed", installed);
    return installed > 0;
}

} // namespace ui_progress_trace_2313

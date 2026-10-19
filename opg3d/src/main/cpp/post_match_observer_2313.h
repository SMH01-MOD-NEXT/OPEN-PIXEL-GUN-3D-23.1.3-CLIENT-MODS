#pragma once

#include <cstdint>
#include <cstring>
#include <ctime>

#include "elf_sym.h"
#include "hook.h"
#include "il2cpp.h"
#include "log.h"

// Read-only observer, invoked only by Unity's main-thread callbacks. A bounded
// strong GC handle keeps an observed iterator safe across heartbeat snapshots;
// it does not schedule or step the iterator. Handles expire after 120 seconds.
namespace post_match_observer_2313 {
namespace detail {
struct Call {
    void* info = nullptr;
    void* code = nullptr;
    explicit operator bool() const { return info && code; }
};
inline Call resolve(const char* ns, const char* cls, const char* name, int argc) {
    Call c;
    c.info = il2cpp::find_method_info(ns, cls, name, argc);
    if (c.info) c.code = il2cpp::method_pointer(c.info);
    return c;
}
inline Call game_object, transform, parent, active_self, active_tree, alive;
inline Call object_name, enabled, panel_alpha, widget_alpha, children;
inline uint32_t (*new_handle)(void*, bool) = nullptr;
inline void* (*get_target)(uint32_t) = nullptr;
inline void (*free_handle)(uint32_t) = nullptr;
inline uint32_t iterator_handle = 0;
inline uint64_t started_ms = 0, stepped_ms = 0, reported_ms = 0;
inline uint64_t step_count = 0;
using Update = void (*)(void*, const void*);
inline Update original_table_update = nullptr;
inline Update original_controller_update = nullptr;
// IL2CPP captures its own managed stack before throwing. Observe AFTER that
// stock capture, using its format APIs, never a native _Unwind_Backtrace.
using Capture = void (*)(void*, const void*);
inline Capture original_capture = nullptr;
inline void (*format_exception)(void*, char*, int) = nullptr;
inline void (*format_stack)(void*, char*, int) = nullptr;
inline thread_local const char* result_scope = nullptr;
inline thread_local bool formatting = false;
inline void capture_exception(void* exception, const void* method) {
    original_capture(exception, method);
    if (!result_scope || formatting) return;
    const char* scope = result_scope;
    result_scope = nullptr; // A throw bypasses the iterator's normal epilogue.
    formatting = true;
    char message[1024]{}, stack[8192]{};
    if (format_exception) format_exception(exception, message, sizeof(message)-1);
    if (format_stack) format_stack(exception, stack, sizeof(stack)-1);
    LOGE("23.1.3-post-match-exception: scope=%s exception=%s", scope, message);
    // Split the managed stack so logcat's individual-message cap cannot hide it.
    for (char* start = stack; *start;) {
        char* end = std::strchr(start, '\n');
        if (end) *end = 0;
        LOGE("23.1.3-post-match-exception: stack %s", start);
        if (!end) break;
        start = end + 1;
    }
    formatting = false;
}

inline uint64_t now_ms() {
    timespec t{};
    clock_gettime(CLOCK_MONOTONIC, &t);
    return uint64_t(t.tv_sec) * 1000 + uint64_t(t.tv_nsec) / 1000000;
}
template <typename T>
inline bool field(void* obj, const char* name, T* value) {
    static_assert(sizeof(T) <= 8);
    if (!obj || !il2cpp::object_get_class || !il2cpp::class_get_field_from_name ||
        !il2cpp::field_get_value) return false;
    void* cls = il2cpp::object_get_class(obj);
    void* f = cls ? il2cpp::class_get_field_from_name(cls, name) : nullptr;
    if (!f) return false;
    alignas(8) uint8_t scratch[16]{};
    il2cpp::field_get_value(obj, f, scratch);
    std::memcpy(value, scratch, sizeof(T));
    return true;
}
inline void* ref(void* obj, const char* name) {
    void* result = nullptr;
    field(obj, name, &result);
    return result;
}
inline void* object(Call c, void* self) {
    return c && self ? reinterpret_cast<void* (*)(void*, void*)>(c.code)(self, c.info) : nullptr;
}
inline int boolean(Call c, void* self) {
    return c && self ? int(reinterpret_cast<bool (*)(void*, void*)>(c.code)(self, c.info)) : -1;
}
inline bool valid(void* obj) {
    return obj && alive && reinterpret_cast<bool (*)(void*, void*)>(alive.code)(obj, alive.info);
}
inline const char* class_name(void* obj) {
    if (!obj) return "null";
    void* cls = il2cpp::object_get_class(obj);
    const char* n = cls ? il2cpp::class_get_name(cls) : nullptr;
    return n ? n : "?";
}
struct Array {
    void* klass;
    void* monitor;
    void* bounds;
    uintptr_t length;
    void* items[1];
};
inline void component_stats(void* go, const char* cls_name, Call alpha) {
    if (!children || !alpha || !il2cpp::class_get_type || !il2cpp::type_get_object) return;
    void* cls = il2cpp::find_class("", cls_name);
    if (!cls) return;
    void* type = il2cpp::type_get_object(il2cpp::class_get_type(cls));
    if (!type) return;
    using Fn = void* (*)(void*, void*, bool, void*);
    auto* a = static_cast<Array*>(reinterpret_cast<Fn>(children.code)(go, type, true, children.info));
    if (!a || a->length > 4096) return;
    size_t active = 0, disabled = 0, transparent = 0;
    float minimum = 1.f, maximum = 0.f;
    const size_t limit = a->length < 512 ? a->length : 512;
    for (size_t i = 0; i < limit; ++i) {
        void* v = a->items[i];
        if (!valid(v)) continue;
        void* child_go = object(game_object, v);
        if (!valid(child_go) || boolean(active_tree, child_go) != 1) continue;
        ++active;
        if (boolean(enabled, v) == 0) ++disabled;
        float av = reinterpret_cast<float (*)(void*, void*)>(alpha.code)(v, alpha.info);
        if (av <= .01f) ++transparent;
        if (av < minimum) minimum = av;
        if (av > maximum) maximum = av;
    }
    LOGI("23.1.3-post-match-observe: %s total=%zu inspected=%zu active=%zu disabled=%zu zero-alpha=%zu alpha=[%.3f,%.3f]",
         cls_name, size_t(a->length), limit, active, disabled, transparent, minimum, maximum);
}
inline void snapshot_impl(void* controller, const char* reason, bool ui) {
    if (!valid(controller)) return;
    void* go = object(game_object, controller);
    bool waiting = false;
    bool have_wait = field(controller, u8"与东丞丙丏丝丗与业", &waiting);
    LOGI("23.1.3-post-match-observe: %s controller=%p enabled=%d host-self=%d host-tree=%d wait-tables=%d",
         reason, controller, boolean(enabled, controller), boolean(active_self, go),
         boolean(active_tree, go), have_wait ? int(waiting) : -1);
    void* tr = object(transform, controller);
    for (int i = 0; i < 6 && valid(tr); ++i) {
        void* ancestor_go = object(game_object, tr);
        if (!valid(ancestor_go)) break;
        LOGI("23.1.3-post-match-observe: ancestor[%d] '%s' self=%d tree=%d", i,
             il2cpp::to_utf8(object(object_name, ancestor_go), 80).c_str(),
             boolean(active_self, ancestor_go), boolean(active_tree, ancestor_go));
        tr = object(parent, tr);
    }
    void* rewards = ref(controller, u8"丁丗且丘与业丙下丆");
    LOGI("23.1.3-post-match-observe: rewards=%p alive=%d mainReward=%p animator=%p",
         rewards, int(valid(rewards)), rewards ? ref(rewards, "mainReward") : nullptr,
         ref(controller, "interfaceAnimator"));
    if (!ui) return;
    constexpr const char* fields[] = {"allInterfaceContainer", "ranksInterface",
        "finishedInterface", "endInterfacePanel", "allInterfacePanel",
        "ProgressInterfaceGo", "Continue_GoToLobbyButton", "listOfPlayers"};
    for (const char* name : fields) {
        void* root = ref(controller, name);
        if (!valid(root)) {
            LOGI("23.1.3-post-match-observe: root %s absent/destroyed", name);
            continue;
        }
        LOGI("23.1.3-post-match-observe: root %s self=%d tree=%d", name,
             boolean(active_self, root), boolean(active_tree, root));
        component_stats(root, "UIPanel", panel_alpha);
        component_stats(root, "UIWidget", widget_alpha);
    }
}
inline void release() {
    if (iterator_handle && free_handle) free_handle(iterator_handle);
    iterator_handle = 0;
}
inline void heartbeat() {
    result_scope = nullptr;
    if (!iterator_handle) return;
    const uint64_t now = now_ms();
    if (now - started_ms > 120000) { release(); return; }
    if (now - reported_ms < 2000) return;
    reported_ms = now;
    void* iterator = get_target(iterator_handle);
    void* controller = ref(iterator, "<>4__this");
    if (!iterator || !valid(controller)) { release(); return; }
    int32_t state = INT32_MIN;
    field(iterator, "<>1__state", &state);
    void* current = ref(iterator, "<>2__current");
    LOGI("23.1.3-post-match-observe: heartbeat state=%d steps=%llu step-age-ms=%llu yield-type=%s",
         state, (unsigned long long)step_count, (unsigned long long)(now-stepped_ms), class_name(current));
    snapshot_impl(controller, "heartbeat", now - started_ms <= 10000);
}
inline void table_update(void* self, const void* method) {
    if (original_table_update) original_table_update(self, method);
    heartbeat();
}
inline void controller_update(void* self, const void* method) {
    if (original_controller_update) original_controller_update(self, method);
    heartbeat();
}
} // namespace detail
inline void snapshot(void* controller, const char* reason) {
    detail::snapshot_impl(controller, reason, true);
}
inline void observe_iterator(void* iterator, bool running) {
    using namespace detail;
    if (!new_handle || !get_target || !free_handle || !iterator) return;
    if (!running) {
        if (iterator_handle && get_target(iterator_handle) == iterator) release();
        return;
    }
    if (!iterator_handle || get_target(iterator_handle) != iterator) {
        release();
        iterator_handle = new_handle(iterator, false);
        started_ms = now_ms();
        reported_ms = 0;
        step_count = 0;
        LOGI("23.1.3-post-match-observe: iterator observed; GC handle=%s (no scheduling changes)", iterator_handle ? "OK" : "FAILED");
    }
    stepped_ms = now_ms();
    ++step_count;
}
inline bool install_hooks(uintptr_t base) {
    using namespace detail;
    game_object = resolve("UnityEngine", "Component", "get_gameObject", 0);
    transform = resolve("UnityEngine", "Component", "get_transform", 0);
    parent = resolve("UnityEngine", "Transform", "get_parent", 0);
    active_self = resolve("UnityEngine", "GameObject", "get_activeSelf", 0);
    active_tree = resolve("UnityEngine", "GameObject", "get_activeInHierarchy", 0);
    alive = resolve("UnityEngine", "Object", "op_Implicit", 1);
    object_name = resolve("UnityEngine", "Object", "get_name", 0);
    enabled = resolve("UnityEngine", "Behaviour", "get_enabled", 0);
    panel_alpha = resolve("", "UIPanel", "get_alpha", 0);
    widget_alpha = resolve("", "UIWidget", "get_alpha", 0);
    // Generic overload shares argc; require this exact build's non-generic method.
    children.info = il2cpp::find_method_by_address("UnityEngine", "GameObject",
                      reinterpret_cast<void*>(base + 0x442152C));
    if (children.info) children.code = il2cpp::method_pointer(children.info);
    new_handle = reinterpret_cast<uint32_t (*)(void*, bool)>(elfsym::find_symbol("libil2cpp.so", "il2cpp_gchandle_new"));
    get_target = reinterpret_cast<void* (*)(uint32_t)>(elfsym::find_symbol("libil2cpp.so", "il2cpp_gchandle_get_target"));
    free_handle = reinterpret_cast<void (*)(uint32_t)>(elfsym::find_symbol("libil2cpp.so", "il2cpp_gchandle_free"));
    const bool api = game_object && transform && parent && active_self && active_tree && alive && enabled && object_name && children && panel_alpha && widget_alpha && new_handle && get_target && free_handle && il2cpp::class_get_name;
    if (!api) {
        LOGE("23.1.3-post-match-observe: incomplete managed API; refusing heartbeat hooks");
        return false;
    }
    // Verify the exact-build BL chain from the exported raise function to
    // Exception::Raise, then to its returning stack-capture helper. No guessed
    // address is patched on another runtime.
    auto* raise = static_cast<uint8_t*>(elfsym::find_symbol("libil2cpp.so", "il2cpp_raise_exception"));
    auto branch_target = [](uintptr_t pc) -> uintptr_t {
        uint32_t word = 0;
        std::memcpy(&word, reinterpret_cast<void*>(pc), sizeof(word));
        if ((word & 0xfc000000u) != 0x94000000u) return 0;
        int64_t imm = word & 0x03ffffffu;
        if (imm & 0x02000000u) imm -= 0x04000000ll;
        return pc + imm * 4;
    };
    if (!raise || reinterpret_cast<uintptr_t>(raise) != base + 0x124e4b0 ||
        branch_target(reinterpret_cast<uintptr_t>(raise) + 8) != base + 0x12cd8a8 ||
        branch_target(base + 0x12cd8b0) != base + 0x12cd660) {
        LOGE("23.1.3-post-match-exception: runtime layout mismatch; not installing");
        return false;
    }
    format_exception = reinterpret_cast<void (*)(void*, char*, int)>(elfsym::find_symbol("libil2cpp.so", "il2cpp_format_exception"));
    format_stack = reinterpret_cast<void (*)(void*, char*, int)>(elfsym::find_symbol("libil2cpp.so", "il2cpp_format_stack_trace"));
    if (!format_exception || !format_stack) return false;
    bool exceptions = hook::install_address(reinterpret_cast<void*>(base + 0x12cd660),
        "23.1.3 Exception::CaptureStackTrace (validated raise BL chain)",
        reinterpret_cast<void*>(&capture_exception), reinterpret_cast<void**>(&original_capture), false);
    bool a = hook::install({"", "NetworkStartTable", "Update", 0}, reinterpret_cast<void*>(&table_update), reinterpret_cast<void**>(&original_table_update), false);
    bool b = hook::install({"", "NetworkStartTableNGUIController", "Update", 0}, reinterpret_cast<void*>(&controller_update), reinterpret_cast<void**>(&original_controller_update), false);
    LOGI("23.1.3-post-match-observe: passive heartbeat table=%d controller=%d UI=%d", int(a), int(b), int(bool(children)));
    LOGI("23.1.3-post-match-exception: scoped managed stack observer=%d", int(exceptions));
    return exceptions && a && b;
}
} // namespace post_match_observer_2313

#pragma once

#include <atomic>
#include <cstdint>
#include <limits>
#include <mutex>
#include <unordered_map>
#include <string>

#include "elf_sym.h"
#include "hook.h"
#include "il2cpp.h"
#include "log.h"

// A reconnect in an already-open lobby does not run the stock boot coroutine.
// This bridge only restarts that stock flow; it never applies invented progress,
// acknowledges a server revision, deletes preferences or mutates balances.
// The marker is accepted only in the authenticated progress_status callback.
namespace admin_resync_2313 {
namespace detail {
using VoidFn = void (*)(void*, const void*);
using ReplyFn = void (*)(void*, void*, void*, const void*);
using LoadFn = void* (*)(void*, int, const void*);
using ContainsFn = bool (*)(void*, void*, const void*);
using ItemFn = void* (*)(void*, void*, const void*);
inline VoidFn original_menu_enable = nullptr;
inline VoidFn original_update = nullptr;
inline ReplyFn original_reply = nullptr;
inline LoadFn load_scene = nullptr;
inline const void* load_info = nullptr;
inline void* string_class = nullptr;
inline std::atomic<bool> boot_completed{false};
struct Marker { std::string player; uint64_t revision = 0; };
inline std::mutex request_mutex;
inline Marker requested;
inline std::unordered_map<std::string, uint64_t> started_revisions;

inline uint64_t parse_revision(const std::string& text) {
    if (text.empty() || text.size() > 20 || text.front() == '0') return 0;
    uint64_t result = 0;
    for (char c : text) {
        if (c < '0' || c > '9') return 0;
        const auto digit = static_cast<uint64_t>(c - '0');
        if (result > (std::numeric_limits<uint64_t>::max() - digit) / 10) return 0;
        result = result * 10 + digit;
    }
    return result;
}

inline Marker parse_marker(const std::string& text) {
    const auto separator = text.find(':');
    if (separator == std::string::npos || separator == 0 || separator > 64) return {};
    std::string player = text.substr(0, separator);
    for (char c : player) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
              (c >= 'A' && c <= 'Z') || c == '_' || c == '-')) return {};
    }
    const auto revision = parse_revision(text.substr(separator + 1));
    return revision ? Marker{player, revision} : Marker{};
}

inline void request(const Marker& marker) {
    // Cold boot already owns AuthSceneController and must not be restarted.
    if (!marker.revision || marker.player.empty() || !boot_completed.load()) return;
    std::lock_guard<std::mutex> lock(request_mutex);
    auto previous = started_revisions.find(marker.player);
    if (previous != started_revisions.end() && marker.revision <= previous->second) return;
    // Bound process-local memory without evicting replay protection.
    if (previous == started_revisions.end() && started_revisions.size() >= 1024) return;
    if (requested.player != marker.player || requested.revision < marker.revision) requested = marker;
}

inline Marker take_request() {
    std::lock_guard<std::mutex> lock(request_mutex);
    Marker result = requested;
    requested = {};
    if (!result.revision || !boot_completed.load()) return {};
    const auto previous = started_revisions.find(result.player);
    if (previous != started_revisions.end() && result.revision <= previous->second) return {};
    started_revisions[result.player] = result.revision;
    boot_completed.store(false);
    return result;
}

inline Marker read_marker(void* reply) {
    if (!reply || !string_class || !il2cpp::object_get_class ||
        !il2cpp::class_get_method_from_name || !il2cpp::string_new) return {};
    auto* klass = il2cpp::object_get_class(reply);
    if (!klass) return {};
    // Resolve the actual inflated Dictionary<string,object> methods, not a
    // guessed generic RVA or a different dictionary instantiation.
    auto* contains_info = il2cpp::class_get_method_from_name(klass, "ContainsKey", 1);
    auto* item_info = il2cpp::class_get_method_from_name(klass, "get_Item", 1);
    auto contains = reinterpret_cast<ContainsFn>(il2cpp::method_pointer(contains_info));
    auto item = reinterpret_cast<ItemFn>(il2cpp::method_pointer(item_info));
    if (!contains || !item) return {};
    void* key = il2cpp::string_new("opg3d_admin_resync");
    if (!key || !contains(reply, key, contains_info)) return {};
    void* value = item(reply, key, item_info);
    if (!value || il2cpp::object_get_class(value) != string_class) return {};
    // 86 code units rejects oversize inputs without accepting a truncated ID.
    return parse_marker(il2cpp::to_utf8(value, 86));
}

inline void reply(void* data, void* context, void* attachments, const void* info) {
    const Marker marker = read_marker(data);
    if (original_reply) original_reply(data, context, attachments, info);
    request(marker); // No scene calls from a possible transport thread.
}

inline void menu_enable(void* self, const void* info) {
    if (original_menu_enable) original_menu_enable(self, info);
    boot_completed.store(true);
}

inline void update(void* self, const void* info) {
    if (original_update) original_update(self, info);
    // MonoBehaviour.Update is Unity's main thread, after BestHTTP dispatch.
    if (!load_scene || !boot_completed.load()) return;
    const Marker marker = take_request();
    if (!marker.revision) return;
    void* scene = il2cpp::string_new ? il2cpp::string_new("AuthorizationScene") : nullptr;
    if (!scene) { boot_completed.store(true); return; }
    LOGI("23.1.3-admin-resync: entering stock authorization scene revision=%llu",
         static_cast<unsigned long long>(marker.revision));
    void* operation = load_scene(scene, 0 /* LoadSceneMode.Single */, load_info);
    if (!operation) {
        // No polling/restart loop on a missing or unavailable scene.
        boot_completed.store(true);
        LOGE("23.1.3-admin-resync: scene load unavailable; durable server flag retained");
    }
}
} // namespace detail

inline bool install() {
    using namespace detail;
    uintptr_t base = 0;
    if (!elfsym::find_library("libil2cpp.so", &base)) return false;
    string_class = il2cpp::find_class("System", "String");
    // There are string/int/struct overloads with the same name/argc. Match the
    // exact 23.1.3 metadata entry; fail closed on any other build.
    load_info = il2cpp::find_method_by_address("UnityEngine.SceneManagement", "SceneManager",
        reinterpret_cast<void*>(base + 0x47A85D0u));
    load_scene = reinterpret_cast<LoadFn>(il2cpp::method_pointer(const_cast<void*>(load_info)));
    if (!string_class || !load_scene || !il2cpp::object_get_class ||
        !il2cpp::class_get_method_from_name || !il2cpp::string_new) return false;
    bool ok = hook::install({"", "MainMenuController", "OnEnable", 0},
        reinterpret_cast<void*>(&menu_enable), reinterpret_cast<void**>(&original_menu_enable), true);
    ok = hook::install({"BestHTTP", "HTTPUpdateDelegator", "Update", 0},
        reinterpret_cast<void*>(&update), reinterpret_cast<void**>(&original_update), true) && ok;
    if (!ok) return false;
    return hook::install({"Progress", u8"丛丏丅专丑丈丗丆七", u8"丝丄丄丅专丑丝一丐", 3},
        reinterpret_cast<void*>(&reply), reinterpret_cast<void**>(&original_reply), true);
}
} // namespace admin_resync_2313

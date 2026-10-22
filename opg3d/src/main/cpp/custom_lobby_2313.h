#pragma once

#include <atomic>
#include "hook.h"
#include "il2cpp.h"
#include "log.h"

// Stock 23.1.3 creates/joins PixelGun3D as SqlLobby (2), but its
// PrivateGamesPanel consumes PhotonNetwork.GetRoomList(), which needs a
// Default lobby's pushed game-list events. Route ONLY custom browsing and
// rooms carrying stock C1 (password or passwordless sentinel) to a dedicated
// Default lobby. Keep normal SQL matchmaking, properties and password UI intact.
namespace custom_lobby_2313 {
namespace detail {
using MI = const void;
using Connect = bool (*)(int, MI*);
using Create = void (*)(void*, bool, bool, int, void*, void*, bool, bool,
                       void*, bool, MI*);
using LobbyCtor = void (*)(void*, void*, uint8_t, MI*);
using Item = void* (*)(void*, void*, MI*);
inline Connect original_connect = nullptr;
inline Create original_create = nullptr;
inline LobbyCtor original_ctor = nullptr;
inline Item get_item = nullptr;
inline MI* item_info = nullptr;
inline void* password_key_field = nullptr;
inline void* string_class = nullptr;
inline thread_local bool custom_scope = false;
inline std::atomic<bool> enabled{false};

struct Scope {
    bool previous;
    explicit Scope(bool custom) : previous(custom_scope) { custom_scope = custom; }
    ~Scope() { custom_scope = previous; }
};

inline bool has_custom_marker(void* properties) {
    if (!properties || !get_item || !password_key_field) return false;
    void* key = nullptr;
    il2cpp::field_static_get_value(password_key_field, &key);
    if (!key) return false;
    void* marker = get_item(properties, key, item_info);
    return marker && il2cpp::object_get_class(marker) == string_class &&
           il2cpp::string_length(marker) > 0;
}

inline bool connect(int group, MI* info) {
    Scope scope(enabled);
    return original_connect(group, info);
}

inline void create(void* name, bool visible, bool open, int capacity,
                   void* properties, void* listed_properties, bool join_or_create,
                   bool rejoin, void* expected_users, bool squad, MI* info) {
    Scope scope(enabled && has_custom_marker(properties));
    original_create(name, visible, open, capacity, properties, listed_properties,
                    join_or_create, rejoin, expected_users, squad, info);
}

inline void lobby_ctor(void* self, void* name, uint8_t type, MI* info) {
    // Do not alter unrelated named lobbies, even within a custom call.
    if (enabled && custom_scope && type == 2 && name &&
        il2cpp::to_utf8(name, 32) == "PixelGun3D") {
        void* replacement = il2cpp::string_new("OPG2313Customs");
        if (replacement) {
            name = replacement;
            type = 0;
            LOGI("23.1.3-custom-lobby: custom route Default/OPG2313Customs");
        }
    }
    original_ctor(self, name, type, info);
}
} // namespace detail

inline bool install_hooks() {
    using namespace detail;
#if defined(__ANDROID__)
    static_assert(sizeof(void*) == 8, "23.1.3 requires arm64-v8a");
#endif
    if (!il2cpp::field_static_get_value || !il2cpp::object_get_class ||
        !il2cpp::string_length || !il2cpp::string_new) return false;
    password_key_field = il2cpp::find_field("", "GameConnect", u8"业万丘不丌丄丛世下");
    string_class = il2cpp::find_class("System", "String");
    item_info = il2cpp::find_method_info("ExitGames.Client.Photon", "Hashtable", "get_Item", 1);
    get_item = reinterpret_cast<Item>(il2cpp::method_pointer(const_cast<void*>(item_info)));
    if (!password_key_field || !string_class || !get_item) return false;
    // Enable only after every hook is installed. A partial installation is a
    // pass-through, not a split create/browse lobby configuration.
    const bool a = hook::install({"", "GameConnect", u8"丏丅丐上万业丆丅丛", 1},
        reinterpret_cast<void*>(&connect), reinterpret_cast<void**>(&original_connect), true);
    const bool b = hook::install({"", "GameConnect", u8"丘三丒丙丟丈下丁丛", 10},
        reinterpret_cast<void*>(&create), reinterpret_cast<void**>(&original_create), true);
    const bool c = hook::install({"", u8"丝丑丆丒丌丅丆业丕", ".ctor", 2},
        reinterpret_cast<void*>(&lobby_ctor), reinterpret_cast<void**>(&original_ctor), true);
    enabled = a && b && c;
    LOGI("23.1.3-custom-lobby: compatibility hooks %s", enabled ? "installed" : "FAILED (pass-through)");
    return enabled;
}
} // namespace custom_lobby_2313

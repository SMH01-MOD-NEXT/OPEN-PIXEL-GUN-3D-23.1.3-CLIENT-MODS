#pragma once
#include <atomic>
#include "hook.h"
#include "log.h"

// Stock 23.1.3 MainStoreView initialization (RVA 0x42933e8) uses
// coinsShop availability (0x32b6da0) to show the Google Play login overlay.
// Allow loading/rendering the self-hosted catalog ONLY during its coroutine
// steps and synchronous UI updates. Billing/authentication outside them remain stock; no
// tokens, inventory, receipts or purchase completion callbacks are fabricated.
namespace server_store_2313 {
namespace detail {
using MI = const void;
using Initialize = void (*)(void*, void*, MI*);
using Available = bool (*)(MI*);
using Refresh = void (*)(void*, MI*);
using LoadStep = bool (*)(void*, MI*);
inline Initialize original_initialize = nullptr;
inline Available original_available = nullptr;
inline Refresh original_refresh = nullptr;
inline Initialize original_select = nullptr;
inline LoadStep original_load_step = nullptr;
inline std::atomic<bool> enabled{false};
inline thread_local bool catalog_scope = false;
struct Scope {
    bool previous;
    explicit Scope(bool value) : previous(catalog_scope) { catalog_scope = value; }
    ~Scope() { catalog_scope = previous; }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
};
inline bool available(MI* info) {
    if (enabled.load() && catalog_scope) return true;
    return original_available(info);
}
// Each MoveNext scope ends before yielding; it never spans Unity frames.
// Preserve the original clock/provider readiness checks and coroutine result.
inline bool load_step(void* self, MI* info) {
    Scope scope(enabled.load());
    return original_load_step(self, info);
}
inline void initialize(void* self, void* config, MI* info) {
    Scope scope(enabled.load());
    original_initialize(self, config, info);
}
inline void refresh(void* self, MI* info) {
    Scope scope(enabled.load());
    original_refresh(self, info);
}
inline void select(void* self, void* section, MI* info) {
    Scope scope(enabled.load());
    original_select(self, section, info);
}
} // namespace detail
inline bool install_hooks() {
    using namespace detail;
    const bool a = hook::install({"", "coinsShop", u8"东丅丘不上世丟丘一", 0},
        reinterpret_cast<void*>(&available), reinterpret_cast<void**>(&original_available), true);
    const bool b = hook::install({"", "MainStoreView", u8"丝万不丘下丄丄三丟", 1},
        reinterpret_cast<void*>(&initialize), reinterpret_cast<void**>(&original_initialize), true);
    // The coroutine redraw is outside initialization. Without its own scope
    // the stock catalog manager clears every offer again after opening.
    const bool c = hook::install({"", "MainStoreView", u8"丏丅丘丐三丈丘专丙", 0},
        reinterpret_cast<void*>(&refresh), reinterpret_cast<void**>(&original_refresh), true);
    const bool d = hook::install({"", "MainStoreView", u8"世丏业下业七丑丕丌", 1},
        reinterpret_cast<void*>(&select), reinterpret_cast<void**>(&original_select), true);
    const bool e = hook::install({"PGCompany.MainStore", u8"丑丏丟丟丒丂一丈业/丁世丞丆东业专丌丄", "MoveNext", 0},
        reinterpret_cast<void*>(&load_step), reinterpret_cast<void**>(&original_load_step), true);
    enabled = a && b && c && d && e && original_available && original_initialize &&
              original_refresh && original_select && original_load_step;
    LOGI("23.1.3-server-store: catalog load/UI hooks %s; billing/auth unchanged",
         enabled ? "installed" : "FAILED (pass-through)");
    return enabled.load();
}
} // namespace server_store_2313

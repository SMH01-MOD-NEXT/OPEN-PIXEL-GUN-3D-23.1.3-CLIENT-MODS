#include <cinttypes>
#include <cstddef>
#include <cstdint>

#include <jni.h>
#include <pthread.h>
#include <unistd.h>

#include "anomaly_trace_2313.h"
#include "backend_client_2313.h"
#include "backend_redirect_2313.h"
#include "config.h"
#include "post_match_trace_2313.h"
#include "config_ingest_trace_2313.h"
#include "config_store_trace_2313.h"
#include "debug_log_trace_2313.h"
#include "config_system_trace_2313.h"
#include "game_mode_events_trace_2313.h"
#include "socket_dispatch_trace_2313.h"
#include "auth_scene_trace_2313.h"
#include "auth_state_trace_2313.h"
#include "activity_indicator_trace_2313.h"
#include "gate_key_trace_2313.h"
#include "progress_flag_trace_2313.h"
#include "progress_sync_balance_trace_2313.h"
#include "auth_state_watch_2313.h"
#include "auth_scene_sweep_2313.h"
#include "info_window_trace_2313.h"
#include "await_trace_2313.h"
#include "coroutine_name_trace_2313.h"
#include "progress_apply_trace_2313.h"
#include "progress_gate_trace_2313.h"
#include "quiet_auth_trace_2313.h"
#include "slot_config_trace_2313.h"
#include "elf_sym.h"
#include "il2cpp.h"
#include "il2cpp_runtime_2313.h"
#include "log.h"
#include "obb_framework_dir.h"
#include "obb_provisioner.h"
#include "photon_2313.h"
#include "photon_default_plugin_2313.h"
#include "custom_lobby_2313.h"
#include "server_store_2313.h"
#include "runtime_dispatch_2313.h"
#include "photon_trace_2313.h"
#include "sio_codec_trace_2313.h"
#include "startup_trace_2313.h"
#include "startup_clock_2313.h"
#include "admin_resync_2313.h"
#include "client_diagnostics_2313.h"
#include "team_strike_bots_2313.h"
#include "escort_bots_2313.h"
#include "bot_aim_2313.h"
#include "ui_progress_trace_2313.h"
#include "version_2313.h"
#include "ws_redirect_2313.h"

namespace {

void (*g_payload_ready_callback)() = nullptr;

constexpr const char* kIl2Cpp = "libil2cpp.so";
constexpr int kWaitSteps = 6000;
constexpr useconds_t kWaitStepUs = 10 * 1000;
constexpr int kStableChecks = 25;
constexpr useconds_t kSettleUs = 100 * 1000;

// ============================================================================
// FEATURE CONFIGURATION
//
// Gameplay/account/configuration behavior belongs to OPEN-PIXEL-GUN-3D-BACKEND.
// Functional hooks below only provide exact-build compatibility or redirect a
// stock transport. Diagnostic hooks are compile-time optional and their result
// never contributes to functional readiness.
// ============================================================================
namespace feature_config {

constexpr bool diagnostics = OPG3D_DIAGNOSTICS != 0;

namespace startup {
constexpr bool signature_compatibility = true;
constexpr bool obb_provisioner = true;

// The broad AuthScene sweep overlaps targeted auth hooks in ShadowHook UNIQUE
// mode. Keep it as an explicit deep-analysis profile; normal diagnostics use
// the narrower probes below, which can run together.
constexpr bool auth_scene_sweep = false;
constexpr bool targeted_auth_traces = diagnostics && !auth_scene_sweep;

constexpr bool version_traces = targeted_auth_traces;
constexpr bool startup_trace = diagnostics;
constexpr bool ui_progress_trace = diagnostics;
constexpr bool activity_indicator_trace = diagnostics;
constexpr bool gate_key_trace = diagnostics;
constexpr bool progress_flag_trace = targeted_auth_traces;
constexpr bool progress_sync_balance_trace = diagnostics;
constexpr bool auth_state_watch = targeted_auth_traces;
constexpr bool anomaly_chain_trace = diagnostics;
constexpr bool config_ingest_trace = diagnostics;
constexpr bool socket_dispatch_trace = diagnostics;
constexpr bool slot_config_trace = diagnostics;
constexpr bool config_store_trace = diagnostics;
constexpr bool auth_scene_trace = targeted_auth_traces;
constexpr bool auth_state_trace = targeted_auth_traces;
constexpr bool info_window_trace = diagnostics;
constexpr bool await_trace = diagnostics;
constexpr bool coroutine_name_trace = diagnostics;
constexpr bool progress_apply_trace = diagnostics;
constexpr bool progress_gate_trace = diagnostics;
constexpr bool quiet_auth_trace = diagnostics;
constexpr bool game_mode_events_trace = diagnostics;
} // namespace startup

namespace network {
constexpr bool backend_client = true;
constexpr bool backend_redirect = true;
constexpr bool photon_online = true;
constexpr bool photon_default_plugin = true;
constexpr bool websocket_redirect = true;
constexpr bool photon_trace = diagnostics;
constexpr bool sio_codec_trace = diagnostics;
} // namespace network

} // namespace feature_config

template <typename Installer>
bool install_component(const char* category, const char* name, bool enabled,
                       Installer installer) {
    if (!enabled) {
        LOGI("config: [%s] %s disabled", category, name);
        return true;
    }
    const bool installed = installer();
    LOGI("config: [%s] %s %s", category, name,
         installed ? "enabled" : "failed");
    return installed;
}

size_t assembly_count(void* domain) {
    if (domain == nullptr || il2cpp::domain_get_assemblies == nullptr) return 0u;
    size_t count = 0u;
    il2cpp::domain_get_assemblies(domain, &count);
    if (count > 8192u) return 0u;
    return count;
}

// Written by the bootstrap worker, published by the dispatcher release-store,
// and read only by its once-consumed Unity-thread job.
uintptr_t g_runtime_base = 0;
bool g_signature_compat = false;

void install_runtime_components(uintptr_t base, bool signature_compat) {
    (void)install_component("diagnostics", "client errors", feature_config::network::backend_redirect,
        []() { return client_diagnostics_2313::install(); });
    (void)install_component(
        "startup", "Switcher loading clock",
        true,
        []() { return startup_clock_2313::install(); });

    (void)install_component(
        "progress", "targeted admin resynchronization",
        true,
        []() { return admin_resync_2313::install(); });

    (void)install_component(
        "gameplay", "Team Strike staggered bots",
        true,
        []() { return team_strike_bots_2313::install(); });
    (void)install_component("gameplay", "Escort objective bots", true,
        []() { return escort_bots_2313::install(); });
    (void)install_component("gameplay", "Bounded bot combat tracking", true,
        []() { return bot_aim_2313::install(); });

    (void)install_component(
        "diagnostics", "lifecycle trace",
        feature_config::startup::version_traces,
        []() { return version_2313::install_runtime_hooks(); });
    (void)install_component(
        "diagnostics", "startup coroutine trace",
        feature_config::startup::startup_trace,
        []() { return startup_trace_2313::install_hooks(); });
    (void)install_component(
        "diagnostics", "UI progress trace",
        feature_config::startup::ui_progress_trace,
        []() { return ui_progress_trace_2313::install_hooks(); });
    (void)install_component(
        "diagnostics", "activity indicator trace",
        feature_config::startup::activity_indicator_trace,
        []() { return activity_indicator_trace_2313::install_hooks(); });
    (void)install_component(
        "diagnostics", "gate-key trace",
        feature_config::startup::gate_key_trace,
        []() { return gate_key_trace_2313::install_hooks(); });
    (void)install_component(
        "diagnostics", "progress-status trace",
        feature_config::startup::progress_flag_trace,
        []() { return progress_flag_trace_2313::install_hooks(); });
    (void)install_component(
        "diagnostics", "progress-sync balance trace",
        feature_config::startup::progress_sync_balance_trace,
        []() { return progress_sync_balance_trace_2313::install_hooks(); });
    (void)install_component(
        "diagnostics", "auth state watch",
        feature_config::startup::auth_state_watch,
        []() { return auth_state_watch_2313::install_hooks(); });
    (void)install_component(
        "diagnostics", "comparison trace",
        feature_config::startup::anomaly_chain_trace,
        []() { return anomaly_trace_2313::install_hooks(); });
    (void)install_component(
        "diagnostics", "config ingestion trace",
        feature_config::startup::config_ingest_trace,
        []() { return config_ingest_trace_2313::install_hooks(); });
    (void)install_component(
        "diagnostics", "managed log trace",
        feature_config::startup::config_ingest_trace,
        []() { return debug_log_trace_2313::install_hooks(); });
    (void)install_component(
        "diagnostics", "config system trace",
        feature_config::startup::config_ingest_trace,
        []() { return config_system_trace_2313::install_hooks(); });
    (void)install_component(
        "diagnostics", "game-mode-events trace",
        feature_config::startup::game_mode_events_trace,
        []() { return game_mode_events_trace_2313::install_hooks(); });
    (void)install_component(
        "diagnostics", "socket dispatch trace",
        feature_config::startup::socket_dispatch_trace,
        []() { return socket_dispatch_trace_2313::install_hooks(); });
    (void)install_component(
        "diagnostics", "slot document parser trace",
        feature_config::startup::slot_config_trace,
        []() { return slot_config_trace_2313::install_hooks(); });
    (void)install_component(
        "diagnostics", "config store trace",
        feature_config::startup::config_store_trace,
        []() { return config_store_trace_2313::install_hooks(); });
    (void)install_component(
        "diagnostics", "auth scene trace",
        feature_config::startup::auth_scene_trace,
        []() { return auth_scene_trace_2313::install_hooks(); });
    (void)install_component(
        "diagnostics", "auth state trace",
        feature_config::startup::auth_state_trace,
        []() { return auth_state_trace_2313::install_hooks(); });
    (void)install_component(
        "diagnostics", "AuthScene deep sweep",
        feature_config::startup::auth_scene_sweep,
        []() { return auth_scene_sweep_2313::install_hooks(); });
    (void)install_component(
        "diagnostics", "info-window trace",
        feature_config::startup::info_window_trace,
        []() { return info_window_trace_2313::install_hooks(); });
    (void)install_component(
        "diagnostics", "coroutine wait trace",
        feature_config::startup::await_trace,
        []() { return await_trace_2313::install_hooks(); });
    (void)install_component(
        "diagnostics", "coroutine-name trace",
        feature_config::startup::coroutine_name_trace,
        []() { return coroutine_name_trace_2313::install_hooks(); });
    (void)install_component(
        "diagnostics", "binded-account gate trace",
        feature_config::startup::progress_gate_trace,
        []() { return progress_gate_trace_2313::install_hooks(); });
    (void)install_component(
        "diagnostics", "progress applier trace",
        feature_config::startup::progress_apply_trace,
        []() { return progress_apply_trace_2313::install_hooks(); });
    (void)install_component(
        "diagnostics", "quiet authorization trace",
        feature_config::startup::quiet_auth_trace,
        []() { return quiet_auth_trace_2313::install_hooks(); });

    (void)install_component(
        "diagnostics", "post-match results trace",
        OPG3D_POST_MATCH_TRACE != 0,
        [base]() { return post_match_trace_2313::install_hooks(base); });

    const bool backend_redirect = install_component(
        "network", "backend redirect", feature_config::network::backend_redirect,
        []() { return backend_redirect_2313::install_hooks(); });
    (void)install_component(
        "network", "Server store catalog", feature_config::network::backend_redirect,
        []() { return server_store_2313::install_hooks(); });
    const bool photon_online = install_component(
        "network", "Photon online", feature_config::network::photon_online,
        []() { return photon_2313::install_hooks(); });
    const bool default_plugin = install_component(
        "network", "Photon default plugin",
        feature_config::network::photon_default_plugin,
        []() { return photon_default_plugin_2313::install_hooks(); });
    (void)install_component(
        "network", "Custom room lobby", feature_config::network::photon_online,
        []() { return custom_lobby_2313::install_hooks(); });
    (void)install_component(
        "diagnostics", "Photon trace", feature_config::network::photon_trace,
        []() { return photon_trace_2313::install_hooks(); });
    const bool websocket_redirect = install_component(
        "network", "config socket redirect",
        feature_config::network::websocket_redirect,
        []() { return ws_redirect_2313::install_hooks(); });
    (void)install_component(
        "diagnostics", "socket.io codec trace",
        feature_config::network::sio_codec_trace,
        []() { return sio_codec_trace_2313::install_hooks(); });

    if (signature_compat && backend_redirect && photon_online &&
        default_plugin && websocket_redirect) {
        if (g_payload_ready_callback) g_payload_ready_callback();
        LOGI("init: functional 23.1.3 thin client ready; diagnostic hook "
             "availability does not affect this verdict");
    } else {
        LOGE("init: functional 23.1.3 thin client incomplete: signature=%d "
             "backend-redirect=%d photon=%d plugin=%d config-socket=%d",
             signature_compat ? 1 : 0, backend_redirect ? 1 : 0,
             photon_online ? 1 : 0, default_plugin ? 1 : 0,
             websocket_redirect ? 1 : 0);
    }
}

void install_runtime_job() {
    install_runtime_components(g_runtime_base, g_signature_compat);
}

void* init_thread(void*) {
    LOGI("init: libopg3d build %s", OPG3D_BUILD_STAMP);
    LOGI("init: [0/6] 23.1.3 ARM64 thin-client bootstrap started");

    uintptr_t base = 0u;
    bool found = false;
    for (int i = 0; i < kWaitSteps && !found; ++i) {
        found = elfsym::find_library(kIl2Cpp, &base);
        if (!found) usleep(kWaitStepUs);
    }
    if (!found) {
        LOGE("init: %s not found after 60 seconds", kIl2Cpp);
        return nullptr;
    }
    LOGI("init: [1/6] %s found, base=0x%" PRIxPTR, kIl2Cpp, base);

    const bool signature_compat = install_component(
        "startup", "signature compatibility",
        feature_config::startup::signature_compatibility,
        [base]() { return version_2313::install_early_signature_patch(base); });
    if (!signature_compat) {
        LOGE("init: exact 23.1.3 APK re-sign compatibility was not installed; "
             "startup remains fail-closed");
    }

    bool resolved = false;
    for (int i = 0; i < kWaitSteps && !resolved; ++i) {
        resolved = il2cpp::resolve();
        if (!resolved) usleep(kWaitStepUs);
    }
    if (!resolved) {
        LOGE("init: required il2cpp_* exports were not resolved");
        return nullptr;
    }
    LOGI("init: [2/6] IL2CPP API resolved");

    // 23.1.3's il2cpp_domain_get() crashes if used as a pre-init poll. Wait
    // on the exact build's validated root-domain publication slot instead.
    void* domain = il2cpp_runtime_2313::wait_for_domain(
        base, kWaitSteps, kWaitStepUs);
    if (domain == nullptr) {
        LOGE("init: IL2CPP root domain did not become safely available");
        return nullptr;
    }
    LOGI("init: [3/6] domain=%p", domain);

    size_t last = 0u;
    int stable = 0;
    for (int i = 0; i < kWaitSteps && stable < kStableChecks; ++i) {
        const size_t now = assembly_count(domain);
        stable = (now != 0u && now == last) ? stable + 1 : 0;
        last = now;
        usleep(kWaitStepUs);
    }
    if (stable < kStableChecks) {
        LOGE("init: assembly list never settled (last count=%zu)", last);
        return nullptr;
    }
    LOGI("init: [4/6] assembly list settled at %zu assemblies", last);
    usleep(kSettleUs);

    void* attached_thread = il2cpp::thread_attach(domain);
    if (attached_thread == nullptr) {
        LOGE("init: il2cpp_thread_attach failed");
        return nullptr;
    }
    LOGI("init: [5/6] thread attached to runtime");

    void* image = nullptr;
    for (int i = 0; i < kWaitSteps && image == nullptr; ++i) {
        image = il2cpp::find_image("Assembly-CSharp.dll");
        if (image == nullptr) usleep(kWaitStepUs);
    }
    if (image == nullptr) {
        LOGE("init: Assembly-CSharp.dll never appeared; nothing to hook");
        if (il2cpp::thread_detach != nullptr) il2cpp::thread_detach(attached_thread);
        return nullptr;
    }
    LOGI("init: [6/6] Assembly-CSharp.dll ready; scheduling 23.1.3 hooks");

    g_runtime_base = base;
    g_signature_compat = signature_compat;
    (void)runtime_dispatch_2313::schedule(&install_runtime_job);

    if (il2cpp::thread_detach != nullptr) il2cpp::thread_detach(attached_thread);
    LOGI("init: 23.1.3 bootstrap worker finished; runtime readiness reported by Unity job");
    return nullptr;
}

} // namespace

static void on_load() {
    if (feature_config::startup::obb_provisioner) {
        (void)obb_framework_dir::prepare();
        (void)obb_provisioner::provision();
    }
    // Started before the IL2CPP wait, not after it: finding the server and
    // registering the device takes seconds, and doing it in parallel with the
    // runtime coming up means the session and the Photon credential are usually
    // ready before the game's first request.
    if (feature_config::network::backend_client) {
        (void)backend_client_2313::start();
    }
    pthread_t thread;
    if (pthread_create(&thread, nullptr, init_thread, nullptr) == 0) {
        pthread_detach(thread);
    } else {
        LOGE("init: pthread_create failed");
    }
}

extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM*, void*) {
    if (feature_config::startup::obb_provisioner) {
        (void)obb_framework_dir::prepare();
        (void)obb_provisioner::provision();
    }
    return JNI_VERSION_1_6;
}

// Explicit, versioned loader ABI: dlopen itself must not start any game work.
extern "C" JNIEXPORT int opg3d_payload_abi() { return 1; }
extern "C" JNIEXPORT int opg3d_payload_start(const char* endpoint) {
    static bool started = false;
    if (started || endpoint == nullptr || *endpoint == '\0') return 0;
    if (setenv("OPG3D_SERVER", endpoint, 1) != 0) return 0;
    started = true;
    on_load();
    return 1;
}

extern "C" JNIEXPORT void opg3d_payload_set_ready_callback(void (*callback)()) {
    g_payload_ready_callback = callback;
}

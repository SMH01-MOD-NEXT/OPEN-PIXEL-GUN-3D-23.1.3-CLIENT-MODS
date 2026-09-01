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
#include "sandbox_2313.h"  // TEMPORARY run-144 sync fork probe
#include "slot_config_trace_2313.h"
#include "elf_sym.h"
#include "il2cpp.h"
#include "il2cpp_runtime_2313.h"
#include "loading_stall_guard_2313.h"
#include "log.h"
#include "obb_provisioner.h"
#include "photon_2313.h"
#include "photon_default_plugin_2313.h"
#include "photon_trace_2313.h"
#include "sio_codec_trace_2313.h"
#include "startup_guards_2313.h"
#include "startup_trace_2313.h"
#include "ui_progress_trace_2313.h"
#include "version_2313.h"
#include "ws_redirect_2313.h"

namespace {

constexpr const char* kIl2Cpp = "libil2cpp.so";
constexpr int kWaitSteps = 6000;
constexpr useconds_t kWaitStepUs = 10 * 1000;
constexpr int kStableChecks = 25;
constexpr useconds_t kSettleUs = 100 * 1000;

// ============================================================================
// FEATURE CONFIGURATION
// Change only true/false values in this block to isolate a component. Disabled
// components are treated as intentionally skipped, not as initialization
// failures.
//
// This library is no longer a backend. Accounts, sessions, bans, progression,
// economy, catalogs, remote configuration, client updates and Photon
// credentials all belong to the separate server project
// (OPEN-PIXEL-GUN-3D-BACKEND). What stays here is what only native code inside
// this process can do: satisfy the exact 23.1.3 APK re-sign check, provision
// the expansion file, survive the boot sequence, and carry the traffic.
// ============================================================================
namespace feature_config {

// Diagnostic hooks are retained as source-level probes but do not ship in the
// normal thin-client build. Enable only for a uniquely named evidence run.
constexpr bool diagnostics = false;

// Startup prerequisites and diagnostics.
namespace startup {
constexpr bool signature_compatibility = true;
constexpr bool version_traces = diagnostics;
constexpr bool startup_guards = false;
// This installer also owns the exact-build state-35 startup compatibility,
// so it remains functional even when passive diagnostics are disabled.
constexpr bool switcher_trace = false;
// Names the widget that actually renders the frozen 90%. Logging only:
// every static candidate for that number has already been eliminated.
constexpr bool ui_progress_trace = diagnostics;
// Run 89 proved the percentage is a UILabel written by the static
// float setter on ActivityIndicator. This names the call site behind
// every write, so the emitter of the final 0.90 is identified rather
// than inferred. Logging only.
constexpr bool activity_indicator_trace = diagnostics;
// Run 90 proved the boot stops on a missing state transition: the auth
// scene reaches Authorized and its dispatcher gate returns false forever.
// That gate reads one persisted Storager key. This names the key and the
// value behind it, plus every distinct integer the boot persists. Logging
// only.
constexpr bool gate_key_trace = diagnostics;
// Run 91 named the gate key (localMultyKey, stored 0) and then the
// disassembly of its call site ruled the gate out: the branch it guards
// is the local-multiplayer one. The real fork at libil2cpp.so+0x3DC9854
// tests a static Nullable<bool> owned by Progress.<TypeDefIndex 6640>,
// and only when that flag is true does the scene start the coroutine
// that carries the bar past 90%. This watches the flag and everything
// that writes it, including a server event callback. Logging only.
constexpr bool progress_flag_trace = diagnostics;
// Run 156 reached the stock final startup iterator but its AfterProgressSync
// callback did not fire. This passive trace balances every progress-command
// acquire/release and names the operation keeping Login behind the 90% gate.
constexpr bool progress_sync_balance_trace = diagnostics;
// Runs 95-98 disproved that flag as the root cause: forcing it true only
// diverts the boot onto the technical-works path and the bar dies at 80%.
// What is left is the scene's own state variable, which reaches 2
// (Authorized) and never 3 (FullySynchronized). Its writers cannot all be
// found statically -- the generated setter is inlined and the inline stores
// use several metadata slots -- so this watches the variable itself and
// names the caller whenever an assignment passes through managed code.
constexpr bool auth_state_watch = diagnostics;
constexpr bool loading_stall_watchdog = diagnostics;
constexpr bool anomaly_chain_trace = diagnostics;
// Reads every configuration document the game accepts, together with the
// source it came from (Embedded/Server/Fallback) and its ConfigId. The
// boot stops on CommandsWithFullSlots missing the 23.1.3 version key, and
// the document's field names exist nowhere in the metadata literals, so
// this is the only way to learn the shape the backend has to serve.
constexpr bool config_ingest_trace = diagnostics;

// Names every inbound socket event and every handler registration. The
// config parser has no static call site, so the only way it can run is a
// delegate in the socket layer's subscriber registry, and runs 30-33 show
// it never running while the answer arrives in full.
constexpr bool socket_dispatch_trace = diagnostics;

// Makes the game name the managed method that logs the
// ParseFullSlotConfig failure: the native logger prints the RVA of
// its own caller, which tools/resolve_rva.py resolves to a method.
// The fourteen documents BalanceController accepts carry no version
// dictionary, so the slot document has a different reader, and that
// reader must be known before the backend can author a document the
// client will parse.
constexpr bool slot_config_trace = diagnostics;
// Names every ConfigId that moves through the client's binary config
// store, with the size of its document. The subscribe ack was refuted as
// the delivery channel for the full-slot document in all three of its
// possible spellings, so the store is where the question moves next.
constexpr bool config_store_trace = diagnostics;
constexpr bool auth_scene_trace = diagnostics;
constexpr bool auth_state_trace = diagnostics;
constexpr bool auth_scene_sweep = diagnostics;
constexpr bool info_window_trace = diagnostics;
constexpr bool await_trace = diagnostics;
// Runs 76-81 showed both Switcher iterators reach their end and the
// main-menu scene load is requested, so the boot sequence is not what
// stalls. One await never completes: AuthSceneController's iterator waits
// on a bare Coroutine at call#3 and no call#4 is ever logged. This names
// the IEnumerator behind every started coroutine so that handle can be
// identified.
constexpr bool coroutine_name_trace = diagnostics;
constexpr bool progress_apply_trace = diagnostics;
constexpr bool progress_gate_trace = diagnostics;
constexpr bool quiet_auth_trace = diagnostics;
constexpr bool command_type_trace = diagnostics;
// TEMPORARY run-143 analysis sandbox. All older force constants stay false;
// only the single caller at 0x3DC7EF8 is allowed to take the state-9 branch
// once so the stock client can expose get_progress. The real fix is backend.
constexpr bool sandbox = diagnostics;
constexpr bool obb_provisioner = true;
} // namespace startup

// Traffic. The self-hosted backend answers every retired HTTP service, and
// Photon stays the battle transport with its credential served by that backend
// instead of being compiled in.
namespace network {
constexpr bool backend_client = true;
constexpr bool backend_redirect = true;
constexpr bool photon_online = true;
constexpr bool photon_default_plugin = true;
constexpr bool photon_trace = diagnostics;
// Points the game's socket.io config channel at the self-hosted backend. That
// channel runs on BestHTTP, so the UnityWebRequest/WWW redirect never sees it,
// and the live config the boot waits for arrives nowhere else. Photon's own
// WebSocket address is reported but never rewritten.
constexpr bool websocket_redirect = true;
// Observes the auth namespace payload codec: the open event name going in
// and the sealed byte[] attachments coming out. Logging only, no packet is
// altered. This is the only route left to the key: the metadata literal
// table holds no standalone one.
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
    LOGI("init: [6/6] Assembly-CSharp.dll ready; installing 23.1.3 hooks");

    const bool version_traces = install_component(
        "startup", "version traces", feature_config::startup::version_traces,
        []() { return version_2313::install_runtime_hooks(); });
    const bool startup_guards = install_component(
        "startup", "startup guards", feature_config::startup::startup_guards,
        []() { return startup_guards_2313::install_hooks(); });
    const bool switcher_trace = install_component(
        "startup", "switcher trace", feature_config::startup::switcher_trace,
        []() { return startup_trace_2313::install_hooks(); });
    const bool ui_progress_trace = install_component(
        "startup", "ui progress trace",
        feature_config::startup::ui_progress_trace,
        []() { return ui_progress_trace_2313::install_hooks(); });
    (void)ui_progress_trace;
    const bool activity_indicator_trace = install_component(
        "startup", "activity indicator trace",
        feature_config::startup::activity_indicator_trace,
        []() { return activity_indicator_trace_2313::install_hooks(); });
    (void)activity_indicator_trace;
    const bool gate_key_trace = install_component(
        "startup", "gate key trace", feature_config::startup::gate_key_trace,
        []() { return gate_key_trace_2313::install_hooks(); });
    (void)gate_key_trace;
    const bool progress_flag_trace = install_component(
        "startup", "progress flag trace",
        feature_config::startup::progress_flag_trace,
        []() { return progress_flag_trace_2313::install_hooks(); });
    (void)progress_flag_trace;
    (void)install_component(
        "startup", "progress sync balance trace",
        feature_config::startup::progress_sync_balance_trace,
        []() { return progress_sync_balance_trace_2313::install_hooks(); });
    const bool auth_state_watch = install_component(
        "startup", "auth scene state watch",
        feature_config::startup::auth_state_watch,
        []() { return auth_state_watch_2313::install_hooks(); });
    (void)auth_state_watch;
    const bool stall_watchdog = install_component(
        "startup", "loading stall watchdog",
        feature_config::startup::loading_stall_watchdog,
        []() { return loading_stall_guard_2313::start_watchdog(); });
    const bool anomaly_trace = install_component(
        "startup", "anomaly comparison trace",
        feature_config::startup::anomaly_chain_trace,
        []() { return anomaly_trace_2313::install_hooks(); });
    // Diagnostic only, so its result is deliberately not folded into the
    // completeness check below: a refused trace must not report the thin
    // client as broken when every functional component is in place.
    (void)install_component(
        "startup", "config ingestion trace",
        feature_config::startup::config_ingest_trace,
        []() { return config_ingest_trace_2313::install_hooks(); });
    // Shares the ingestion trace flag because it answers the same question
    // from the other side: which managed component logs the config wait.
    (void)install_component(
        "startup", "managed log caller trace",
        feature_config::startup::config_ingest_trace,
        []() { return debug_log_trace_2313::install_hooks(); });
    // Same flag once more, one layer deeper. Run 29 resolved the managed caller
    // of the config wait line to an auth-scene iterator that polls one byte on
    // the config system singleton, and the only code that writes that byte is
    // the config system's answer parser -- whose third parameter is a
    // List<byte[]>. This trace reads that shape out of the live process.
    (void)install_component(
        "startup", "config system trace",
        feature_config::startup::config_ingest_trace,
        []() { return config_system_trace_2313::install_hooks(); });
    (void)install_component(
        "startup", "game mode events state trace",
        feature_config::startup::sandbox,
        []() { return game_mode_events_trace_2313::install_hooks(); });

    // TEMPORARY, diagnostic only, so its result is deliberately not
    // folded into the completeness check below.
    (void)install_component(
        "startup", "analysis sandbox", feature_config::startup::sandbox,
        []() { return sandbox_2313::install_hooks(); });
    // Reads the other half of the same question: not whether the config
    // system asks, but whether anything is listening when the answer is
    // delivered to it.
    (void)install_component(
        "startup", "socket dispatch trace",
        feature_config::startup::socket_dispatch_trace,
        []() { return socket_dispatch_trace_2313::install_hooks(); });
        (void)install_component("startup", "slot document reader trace",
                                feature_config::startup::slot_config_trace,
                                []() {
                                    return slot_config_trace_2313::install_hooks();
                                });
    (void)install_component(
        "startup", "config store trace",
        feature_config::startup::config_store_trace,
        []() { return config_store_trace_2313::install_hooks(); });
    (void)install_component(
        "startup", "auth scene trace",
        feature_config::startup::auth_scene_trace,
        []() { return auth_scene_trace_2313::install_hooks(); });
    (void)install_component(
        "startup", "auth state trace",
        feature_config::startup::auth_state_trace,
        []() { return auth_state_trace_2313::install_hooks(); });
    (void)install_component(
        "startup", "auth scene sweep",
        feature_config::startup::auth_scene_sweep,
        []() { return auth_scene_sweep_2313::install_hooks(); });
    (void)install_component(
        "startup", "info window trace",
        feature_config::startup::info_window_trace,
        []() { return info_window_trace_2313::install_hooks(); });
    (void)install_component(
        "startup", "coroutine wait trace",
        feature_config::startup::await_trace,
        []() { return await_trace_2313::install_hooks(); });
    (void)install_component(
        "startup", "coroutine name trace",
        feature_config::startup::coroutine_name_trace,
        []() { return coroutine_name_trace_2313::install_hooks(); });
    (void)install_component(
        "startup", "binded account gate trace",
        feature_config::startup::progress_gate_trace,
        []() { return progress_gate_trace_2313::install_hooks(); });
    // Run 49 left one question open: the applier throws either because the
    // consumer's cast produced a null or because the document it did receive
    // is not the shape it expects. This reports which of the two happened and
    // names the three keys the consumer reads, without changing any value.
    (void)install_component(
        "startup", "boot progress applier trace",
        feature_config::startup::progress_apply_trace,
        []() { return progress_apply_trace_2313::install_hooks(); });
    (void)install_component(
        "startup", "quiet authorization trace",
        feature_config::startup::quiet_auth_trace,
        []() { return quiet_auth_trace_2313::install_hooks(); });

    const bool backend_redirect = install_component(
        "network", "backend redirect", feature_config::network::backend_redirect,
        []() { return backend_redirect_2313::install_hooks(); });
    const bool photon_online = install_component(
        "network", "Photon online", feature_config::network::photon_online,
        []() { return photon_2313::install_hooks(); });
    const bool default_plugin = install_component(
        "network", "Photon default plugin",
        feature_config::network::photon_default_plugin,
        []() { return photon_default_plugin_2313::install_hooks(); });
    const bool photon_trace = install_component(
        "network", "Photon trace", feature_config::network::photon_trace,
        []() { return photon_trace_2313::install_hooks(); });
    const bool websocket_redirect = install_component(
        "network", "config socket redirect",
        feature_config::network::websocket_redirect,
        []() { return ws_redirect_2313::install_hooks(); });
    const bool sio_codec_trace = install_component(
        "network", "socket.io codec trace",
        feature_config::network::sio_codec_trace,
        []() { return sio_codec_trace_2313::install_hooks(); });

    if (signature_compat && version_traces && startup_guards &&
        switcher_trace && stall_watchdog && anomaly_trace && backend_redirect &&
        photon_online && default_plugin && photon_trace &&
        websocket_redirect && sio_codec_trace) {
        LOGI("init: all enabled 23.1.3 components installed; "
             "disabled components were intentionally skipped");
    } else {
        LOGE("init: 23.1.3 thin client incomplete: signature=%d traces=%d "
             "startup-guards=%d switcher-trace=%d stall-watchdog=%d "
             "anomaly-trace=%d photon=%d plugin=%d photon-trace=%d "
             "config-socket-redirect=%d sio-codec-trace=%d",
             signature_compat ? 1 : 0, version_traces ? 1 : 0,
             startup_guards ? 1 : 0, switcher_trace ? 1 : 0,
             stall_watchdog ? 1 : 0, anomaly_trace ? 1 : 0,
             photon_online ? 1 : 0, default_plugin ? 1 : 0,
             photon_trace ? 1 : 0, websocket_redirect ? 1 : 0,
             sio_codec_trace ? 1 : 0);
    }

    if (il2cpp::thread_detach != nullptr) il2cpp::thread_detach(attached_thread);
    LOGI("init: 23.1.3 bootstrap initialization finished cleanly");
    return nullptr;
}

} // namespace

__attribute__((constructor)) static void on_load() {
    if (feature_config::startup::obb_provisioner) {
        obb_provisioner::provision();
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
        obb_provisioner::provision();
    }
    return JNI_VERSION_1_6;
}

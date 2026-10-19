# Targeted admin resynchronization — 23.1.3

## Purpose and scope

An already-open lobby reconnects its socket without rerunning the stock progress initialization coroutine. The backend therefore cannot refresh all local progress merely by closing that socket.

`admin_resync_2313.h` recognizes the backend's authenticated `progress_status` marker `opg3d_admin_resync: "<player>:<revision>"` and schedules the stock AuthorizationScene through the Unity main-thread update callback. It does not parse or replace gameplay balances, acknowledge progress, clear preferences, erase cache or kill the Android process. The existing startup-clock guard remains unchanged.

## Stock contracts and safeguards

- Hooks use metadata for MainMenuController.OnEnable, HTTPUpdateDelegator.Update and the measured progress-status reply callback; originals remain called.
- The reply dictionary is read through its actual inflated ContainsKey/get_Item metadata. Only a real System.String marker is accepted.
- Player identifiers and positive uint64 revisions are bounded and validated. Cold boot ignores the marker because authorization is already running.
- Replay protection is per account, allowing another account to have a lower revision. Storage is bounded to 1024 accounts without replay-protection eviction.
- Scene loading occurs only on the Unity thread, not in the transport callback.
- Installed BuildSettings identifies scene19 as Assets/Scenes/AuthorizationScene.unity; its level19 contains AuthSceneController.
- The overloaded LoadSceneAsync(string, LoadSceneMode) method is checked against the measured 23.1.3 metadata pointer (base + 0x47A85D0). A mismatch fails closed; this is not a generic cross-version hook.

The companion backend persists a player revision, fences stale uploads, reconnects only that authenticated account and clears pending delivery only after stock full download and Login completion. Polling-only live transport refresh was not verified. Full server-side pricing/reward validation remains outside this compatibility repair.

## Verification — 2026-10-19

Final online revision26 was edited with one live session at 18:28:08 local time. Stock sync_full downloaded 87 sections at 18:28:21 and Login acknowledged at 18:28:23. The Android PID stayed unchanged. No manual restart or data/cache wipe was needed. Offline delivery was separately verified on the next login. Later purchases and chest XP changes persisted without replaying admin counters.

Production build: `./gradlew check :opg3d:assembleRelease -POPG3D_DIAGNOSTICS=false --no-daemon`.
Host tests: admin resync, startup clock, post-match observer and URL diagnostics compiled with warnings as errors and passed; four Python tool tests passed.

Native SHA-256: `1badf6acd0a65c2c5e722b7dec2828e1012e584cab9cb906ba069b8292849d02`.
Backend SHA-256: `172246245bfe311f9a297f483dafd6097bf8cf630279c29a8b594f751a1d71b3`.

No original data dumps are required by this bridge. Private device backups and logs are intentionally not committed. Do not overwrite later user purchases with a diagnostic backup. This change does not claim to fix the separate intermittent 21% loading case.

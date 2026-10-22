# Realme startup recovery — 23.1.3 ARM64

## Observed failure

On the connected Realme RMX3760 (Android 15), repeated launches terminated in
UnityMain with SIGSEGV, PC=0. The APK native libraries and global metadata were
byte-identical to those installed on Nothing A059P, which completed startup.
The older signed diagnostic payload also reproduced the crash (one launch
completed, the next crashed); this is not established as a custom-room regression.

The exact first failing indirect call is in the shared
`Rilisoft.Singleton<T>.get_Instance` body: RVA `0x4A02240`, LR `0x4A02244`.
A temporary pass-through observer identified the closed type as
`Rilisoft.WebSocket.WebSocketManager`. Its RGCTX contains a valid inflated
`UnityEngine.Object.FindObjectOfType<WebSocketManager>` MethodInfo, but both
native entry pointers are null. The same path on Nothing had non-null entries.
Other singleton instantiations on Realme also had valid entries.

An experimental bridge for that one missing lookup only moved the crash to a
second missing generic entry in
`Dictionary<object,object>.System.Collections.IDictionary.set_Item`
(RVA `0x3B6160C`, LR `0x3B61610`). It is therefore not a valid general fix and
was removed. No MethodInfo rewriting, fake object, swallowed signal or skipped
stock game operation remains in the release.

## Change

Previously the native bootstrap worker resolved and installed game hooks while
Unity was performing startup/class/generic initialization. It now only performs
the existing native signature compatibility, export/domain/assembly readiness
checks and an engine-level `UnitySynchronizationContext.Exec` hook. The game
hook installation is queued once and executed on that Unity callback before its
original queue processing. Server discovery remains parallel native work.

The callback is atomically published and consumed before running, preventing
reentrant or repeated installation. Failed scheduling leaves game hooks unarmed
and does not report functional readiness. Every stock Exec call is forwarded
with unchanged arguments. The payload-ready callback is still gated by the
required functional components, and now runs after Unity-thread installation.

This removes concurrent bootstrap game-metadata work and also defers it to a
known engine callback. The captures support a startup initialization-order /
concurrency issue; they do not identify the precise internal IL2CPP cache or
lock defect. There is no model-name blacklist, global sleep, allocator policy,
heap-tagging opt-out, account reset, API-level downgrade or graphics fallback.

## Verification

- Host test: null/repeated/failed scheduling; no work on the scheduling worker;
  execution on the consuming thread; reentrancy; stock arguments/forwarding;
  exactly-once consumption under 16 competing callers. Warnings are errors.
- Gradle `check :opg3d:assembleRelease -POPG3D_DIAGNOSTICS=false` passed.
- Diagnostic candidate: 3 consecutive Realme launches survived the former
  failure; the last was visually verified in the main menu.
- Ordinary release candidate: signed local payload build 19,
  SHA-256 `53f3d3f063b514ebd30717f5fe3d99fbfb4bd148d9295f570638ba8d3d6ee13a`.
  Five consecutive release launches completed to the main menu, visually
  verified from all five screenshots. Each app process logged exactly one Unity
  setup and functional readiness, with no SIGSEGV/SIGABRT or Java fatal error.
  PID-scoped logs contain no temporary observer/bridge/allocator probes.
- Eight C++ host test binaries and eleven Python tests passed; the dispatcher
  test also passed AddressSanitizer and UndefinedBehaviorSanitizer.
- The user independently confirmed Realme no longer crashed during their checks
  and requested resuming custom-room work. A complete ordinary match regression
  and end-of-match flow have not been verified in this pass.

Only the selected local backend release was changed. The installed APK, signing
identity, game data, account, balances and tutorial flags were not replaced or
reset. Nothing was disconnected by the user during Realme investigation, so
post-change Nothing and two-device custom-room acceptance remain pending.

Private logs/screenshots are excluded from Git. This is bounded device evidence,
not a claim that every crash on every Realme model is fixed.

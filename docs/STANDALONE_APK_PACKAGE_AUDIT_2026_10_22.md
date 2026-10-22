# Standalone APK package migration audit — 2026-10-22

## Scope

Standalone `org.openpixelgun.opg2313`, launcher label `OPG2313`, owned 23.1.3
ARM64 assets and persistent signing identity. Original Java class/JNI names and
backend/Photon protocol identity are intentionally distinct from the Android
application ID; do not replace every occurrence of the old package globally.

## Confirmed blocker and repair

The pre-Photon `GameConnect` call entered but never returned. A temporary scoped
IL2CPP exception observer captured:

```
UnityEngine.AndroidJavaException:
android.content.pm.PackageManager$NameNotFoundException: com.pixel.gun3d
```

`Initializer` at RVA `0x2ED7A64` asks Android PackageManager for the APK signing
certificate using a hardcoded original package. This fails when only the
standalone application is installed. Replace only its package-argument producer
with the real `UnityEngine.Application.identifier` getter at `0x4441E10`.
The existing certificate query, hash calculation and connection logic remain.
No signature is invented; no exception is suppressed; no protocol ID is renamed.
`localMultyKey` was not changed and is not the cause of this exception.

The source-only `tools/standalone_apk.py` validates the original ELF SHA-256,
all replacement instruction spans, and the complete identifier getter before
writing anything. Five instructions at `0x2ED7CB0` / `0x2ED7CDC` preserve the
arguments array in x20 and keep the real managed package string in x21 across
the stock array type check. The pre-existing narrow OBB coroutine patch lets
Unity consume expansion assets merged into the APK. The patch list rejects
unknown input, overlaps, unexpected instructions and repeated application.
The original ELF and metadata are not committed or globally string-rewritten.

## Verification

- 11 Python tests passed, including exact-owned-ELF integration verification:
  only validated native spans change; file size is unchanged; second application
  fails without modifying the already-patched file.
- C++ observer lifecycle/scoped forwarding, startup clock, Team Strike scheduling,
  and loader policy host tests passed with warnings treated as errors.
- Gradle `check :opg3d:assembleRelease` passed for diagnostics and default-off.
- APK rebuild, 16 KiB native alignment and signature v1/v2/v3 verification passed.
- APK installed with `adb install -r --no-streaming`, same persistent signing key.
  No data/cache wipe or account/progress migration was performed.
- Device process 6895: former connection call returned true; game loaded a match;
  screenshot and the user independently confirmed match entry.
- That specific initial match uses stock PUN offline mode. It does NOT establish
  online matchmaking, a second-device session or public-server acceptance.

Installed fixed APK SHA-256: `b6e00c1b53bea73eb2a0e3f17a8d839571e472c11b13a8fdd9c0fc82725535bf`.
Observed signed diagnostic payload build 6 SHA-256:
`27b4bcd8c1b0ad08eee7bbfa95dca6be3576df19ad238aaf2298e6e4abace1d5`.
Default-off native release was built, not installed during the user's match.
Temporary scoped exception probes were removed from source after capture;
the prior uncommitted passive Photon trace remains untouched.

## Additional migration audit

- Manifest application ID and app data directory use `org.openpixelgun.opg2313`.
- Both provider authorities use the standalone ID (AndroidX startup, NativeShare).
- No old application ID remains in decoded manifest/resource XML.
- Launcher Activity retains its real Java class identity; this is intentional.
- NativeShare uses Context.getPackageName for its authority resolution.
- App is not debuggable; backup remains disabled. VersionName 23.1.3,
  versionCode 268411, minSdk22, targetSdk33 are preserved, not invented.
- The only exact legacy Java package string found is unused
  `com.pixel.gun3d.BuildConfig.APPLICATION_ID`. No decoded smali references to
  that field were found. It is not a second demonstrated runtime failure.
- Other native literals serve original protocol/store/service identities.
  Their presence alone is not evidence of a package lookup failure; they are
  not blanket-replaced.
- In captured logs for process 6895 there are no NameNotFoundException,
  ClassNotFoundException, NoSuchMethod/NoSuchFieldError, UnsatisfiedLinkError,
  FileNotFound/DirectoryNotFoundException, DllNotFound/EntryPointNotFoundException,
  FATAL EXCEPTION, SIGSEGV, SIGABRT or OutOfMemoryException after installation.
  This is bounded evidence from one device/session, not a universal guarantee.

## Remaining errors — not hidden by the migration repair

- Tutorial player module: `System.FormatException` while parsing an integer.
- MapPopularityController: `KeyNotFoundException` for missing response key `data`.
  Both were already documented before the package migration in
  `STARTUP_CLOCK_RECOVERY_23_1_3.md` and remain separate contract/data follow-ups.
- ClanCommon is empty; TrophyRoad parsing fails; TopUp, ConfrontationEvent and
  ManualOffers configs are null. These log at initialization. No causal link
  to package renaming has been demonstrated; backend config support needs its
  own verification. Do not make up config payloads or rewards to silence them.
- A duplicate shared hook is reported in the broad diagnostic profile. This is
  an observer overlap, not evidence of a release-profile package crash.

Still unverified: real online matchmaking; end-of-match rewards/Continue for
this standalone install; store/share/push/deep-link paths and other devices/OS.
Optional Google integrations were intentionally disabled by the earlier packaging
work; this audit does not claim those services continue to function.

Private backups and logs are under the local recovery cache's
`apk-standalone/resume-connection` directory, excluded from version control.
No private signing key, session-bearing log, APK, game asset or vendor smali is
included in the source commit. Current gameplay was not interrupted for audit.

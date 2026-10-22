# Native-only launcher and signed frontend updates

## What goes where

- **APK:** `lib/arm64-v8a/libopg3d.so` — permanent launcher, responsive embedded
  UI, discovery, compatibility gate, download/cache/signature verification.
- **Backend only:** `lib/arm64-v8a/libopg3d_client.so` — downloadable gameplay
  compatibility/transport module, including existing Team Strike and pass fixes.
- Never put `*.unstripped.so` or the downloadable module in the APK. Keep symbols
  privately for diagnostics. Both shipping artifacts statically link their own
  dependencies; no additional project Java classes, DEX or UI assets are needed.
  Android's installed system WebView renders the HTML embedded in the launcher.

If the APK already calls `System.loadLibrary("opg3d")` before Unity, replace that
library without changing DEX. For a clean APK without such an entry point, add
`DT_NEEDED libopg3d.so` to **libil2cpp.so** before repacking/signing, e.g.
`patchelf --add-needed libopg3d.so libil2cpp.so`. Merely copying an unreferenced
library into an APK does not execute it. Do not add a dependency from the loader
back to libil2cpp; that would create a cycle.

## Build and trust

`./gradlew check :opg3d:assembleRelease --no-daemon` exports both libraries.
The checked-in `loader/release-public.der` is a **public** RSA3072 SPKI trust pin.
Override it with `-POPG3D_RELEASE_PUBLIC_KEY_FILE=/absolute/path/public.der` when
building an APK for another publisher. Never embed a private signing key.
Missing public keys fail the build; invalid signatures fail closed at runtime.
Use the existing `ORG_GRADLE_PROJECT_OPG3D_APPID_KEY` environment variable when
building a functioning payload against the backend's sealed Photon envelopes.

The signing private key remains outside both repositories. Losing it prevents
publishing updates for existing APKs; changing the trust root requires rebuilding
and redistributing the permanent launcher. Back it up securely. Do not publish
that key, `.env`, state files or runtime logs.

## Startup and selection

1. The native launcher gates `il2cpp_init` before game initialization.
2. The UI is created on Android's **main Looper**, using the registered
   `MessageQueue.nativePollOnce` JNI boundary, found by validated registration
   name/signature rather than a device-specific code offset. Worker-owned
   WebViews are forbidden: Android screenshot/scroll-capture callbacks use the
   main thread and will otherwise crash the process.
3. Remembered endpoints are checked every launch; LAN discovery uses the existing
   UDP47318 protocol. Valid LAN replies are verified over HTTP, deduplicated and
   marked `Local`. Saved unavailable endpoints remain in the list.
4. Manual DNS/IPv4 host + port (1–65535) can be checked and remembered. Last
   successful Play selection is preferred on the next launch.
5. Protocol1, game23.1.3, ARM64, maintenance and release availability gate Play.
   Game version and frontend release/build are separate concepts.
6. On Play, fetch and verify the signed manifest, reject incompatible/downgrade
   metadata, then reuse an intact matching cache or download the named artifact.
7. Verify size, SHA256, AArch64 ELF and explicit payload ABI before starting it.
   Only then release the game gate. Temporary dlopen bridges are removed first:
   leaving them installed changes Android vendor linker namespaces and can break
   GPU driver loading. No loaded library is overwritten or hot-swapped.

The server picker is responsive CSS with safe-area/fullscreen support and a
mobile single-column layout. Names are inserted as text, never HTML. UI commands
use a random per-process192-bit path on a loopback-only HTTP listener, strict
same-origin CSP, bounded requests and no JavaScript-to-Java bridge. No remote HTML
or scripts are accepted. Android system WebView must be available.

## Cache and failure behavior

The private cache is under `files/opg3d/loader/`, never external shared storage.
Candidate files are written via a temporary file, fsynced and renamed, then made
read-only before dlopen. Partial downloads/hash failures do not replace the last
active cache. Files are content-addressed; the APK-resident loader never changes.

The payload explicitly reports functional hook initialization. Only then are its
active/highest build markers committed and its pending-start marker removed.
Two generations are retained. An interrupted start before that confirmation can
fall back to a separately signature-verified known-good compatible cache on the
next Play; without one, a new server release is required. This is not a guarantee
of rollback for every later gameplay/GPU crash after the confirmation boundary.
A server offering an older confirmed build is rejected rather than silently
rolling back code. Updates take effect on the next process launch, not in-game.

Transport remains the existing backend HTTP protocol (DNS/IPv4 + port). Code
signatures protect update authenticity even on LAN/HTTP; they do **not** encrypt
account traffic. HTTPS-only reverse proxies and IPv6 endpoints are not currently
supported by the legacy gameplay transport. Do not describe this as TLS.

## Publishing

See backend `docs/NATIVE_FRONTEND_RELEASES.md`. Publication is offline signed and
atomic, independent of restarting the backend. Countries are explicit ISO3166
server configuration; `ZZ` means unspecified, not an inferred location. A LAN
server always displays `Local` instead of a country.

## Verification

`c++ -std=c++17 -Wall -Wextra -Werror tools/test_loader_policy.cpp -o /tmp/loader-policy && /tmp/loader-policy`
checks endpoints, header/path injection, hashes, size and rollback policy.
Gradle check/build verifies both native targets without adding DEX. Backend tests
cover signed/tampered/wrong-key/incompatible releases and immutable delivery.
Physical device checks must additionally cover screenshot capture, background/
resume, keyboard/manual selection, first download, changed release and cached
launch all the way to the game lobby. Only tested device/OS combinations should
be reported as verified; runtime platform hooks are not a universal Android claim.

# Pixel Gun 3D 23.1.3 thin client

This standalone project contains the native frontend for the frozen 23.1.3 Android client. It does not contain a backend. Accounts, sessions, bans, progression, economy, remote configuration, telemetry, and authoritative gameplay behavior live in `OPEN-PIXEL-GUN-3D-BACKEND`.

## Components that remain in-process

| Component | Responsibility |
| --- | --- |
| `version_2313.h` | Applies exact-build signature compatibility so a re-signed APK can start. |
| `obb_framework_dir.h`, `obb_provisioner.h` | Prepare the framework-visible expansion directory and provision the OBB before Unity opens it. |
| `backend_client_2313.h` | Selects the backend, registers the device, establishes a session, and opens the sealed PUN credential. |
| `backend_redirect_2313.h` | Rewrites retired HTTP hosts to the selected backend while preserving the original host in the path. |
| `ws_redirect_2313.h` | Redirects the legacy Socket.IO configuration transport. |
| `photon_2313.h`, `photon_default_plugin_2313.h` | Inject the backend-provided PUN credential and route the exact client build correctly. |
| `*_trace_2313.h` and other trace modules | Optional passive diagnostics that observe managed call frames without changing stock results. |

The normal profile compiles with `OPG3D_DIAGNOSTICS=0`. Functional readiness depends only on compatibility and transport components, never on a diagnostic hook.

## 1. Build the library

```bash
cd "$HOME/Studio canaryProjects/OLD-PIXEL-GUN-3D-2313-FRONTEND"
./gradlew :opg3d:assembleRelease --no-daemon --stacktrace
```

A credential-free build verifies compilation. For an operational build, pass the same envelope key used by the backend without writing it into the repository:

```bash
export ORG_GRADLE_PROJECT_OPG3D_APPID_KEY='<shared key>'
./gradlew :opg3d:assembleRelease --no-daemon --stacktrace
unset ORG_GRADLE_PROJECT_OPG3D_APPID_KEY
```

Optional properties:

| Property | Meaning |
| --- | --- |
| `-POPG3D_DEFAULT_SERVER=host[:port]` | Fallback after manual configuration, remembered selection, and LAN discovery all fail. |
| `-POPG3D_DIAGNOSTICS=true` | Enables observational diagnostics for a specifically named evidence build. |
| `-PCMAKE_VERSION=<version>` | Uses an installed CMake version instead of the verified default, 4.1.2. |

Every assemble task exports:

```text
lib/arm64-v8a/libopg3d.so             stripped; put this in the APK
lib/arm64-v8a/libopg3d.unstripped.so  symbols; retain for crash analysis
```

To refresh only the exported files after a completed release build:

```bash
./gradlew :opg3d:exportReleaseNativeLib
```

## 2. Put the library in the APK

The library uses an ELF constructor and `JNI_OnLoad`; no additional managed bridge is required once the APK loads `libopg3d.so`.

```bash
APK=pg3d-23.1.3.apk
rm -rf work
unzip -q "$APK" -d work
cp lib/arm64-v8a/libopg3d.so work/lib/arm64-v8a/libopg3d.so
(cd work && zip -q -r ../repacked.apk . -x '*.DS_Store')
zipalign -p -f 4 repacked.apk aligned.apk
apksigner sign --ks your.keystore --out signed.apk aligned.apk
adb install -r signed.apk
```

The loader must reference `libopg3d.so`, for example through an existing `DT_NEEDED` entry or a managed `System.loadLibrary("opg3d")` call. Re-signing is expected; `version_2313.h` supplies the compatibility patch for this exact build.

## 3. Select a backend

The client resolves a server in this order:

1. `OPG3D_SERVER` in the process environment for scripted runs;
2. `/sdcard/Android/data/com.pixel.gun3d/files/opg3d-server.txt`, containing `host` or `host:port`;
3. the last successful selection in the app's private files directory;
4. a LAN broadcast using `OPG3D-DISCOVER-V1` on UDP port 47318;
5. the optional `OPG3D_DEFAULT_SERVER` build property.

For a remote test server:

```bash
adb shell 'echo 203.0.113.10:8080 > /sdcard/Android/data/com.pixel.gun3d/files/opg3d-server.txt'
```

## 4. Read the client log

```bash
adb logcat -c
adb logcat -s OPG3D:V
```

A healthy startup identifies the v2 ARM64 thin-client build, selects a backend, completes `/api/v1/ping` and device registration, opens the PUN envelope, and reports that the functional thin client is ready.

Common failures:

| Log text | Meaning |
| --- | --- |
| `no backend found` | Every selection method failed. Configure the server file or fallback. |
| `did not answer /api/v1/ping` | The address is wrong, the backend is unavailable, or the device cannot reach it. |
| `no backend is selected yet` | Startup raced the handshake. Repetition indicates server selection failed. |
| `AppID envelope failed authentication` | Frontend and backend envelope keys differ. |
| `the server refused this device: banned` | The backend applied a configured ban. |
| `status=404` with `unhandled_route` | The backend still needs an implementation for that legacy route. |

Use a diagnostics-enabled build only to answer a concrete wire-contract question. The diagnostic profile emits bounded previews and must call stock methods exactly once with unmodified arguments and results.

## 5. Symbolize a native failure

Keep the matching unstripped library and run the repository's symbolization tools against the captured log. Do not commit private game dumps, metadata, APK contents, device logs, build outputs, or credentials.

# 23.1.3 thin client — build it, put it in the APK, read the log

This branch (`23.1.3-thin-client`) no longer contains a backend. Accounts,
sessions, bans, remote config, progression, telemetry, client updates and the
Photon credential all live in the separate server project
(`OPEN-PIXEL-GUN-3D-BACKEND`, GPLv3). What is left inside `libopg3d.so` is what
only native code inside the game process can do:

| Component | Why it cannot move to the server |
| --- | --- |
| `version_2313.h` signature patch | The 23.1.3 APK refuses to run re-signed; the check is in-process. |
| `obb_provisioner.h` | The expansion file has to be on disk before Unity opens it. |
| `startup_guards_2313.h`, `loading_stall_guard_2313.h` | Boot watchdogs for the dead-service era; they come off one at a time against a real server log. |
| `backend_client_2313.h` | Finds the server, registers the device, opens the sealed Photon AppID. |
| `backend_redirect_2313.h` | Rewrites the game's HTTP URLs at the chosen server. |
| `photon_2313.h`, `photon_default_plugin_2313.h` | PUN route and credential injection. |
| traces (`*_trace_2313.h`), `anomaly_trace_2313.h` | Diagnostics that need managed call frames. |

## 1. Build the library

`OPG3D_APPID_KEY` must be byte-for-byte the server's `OPG3D_APPID_KEY`. It only
unseals Photon credentials — it grants nothing else — so if it leaks you rotate
that one key on the server and rebuild.

```bash
cd "$HOME/Studio canaryProjects/OLD-PIXEL-GUN-3D-ONLINE-PROJECT"
./gradlew :opg3d:assembleRelease \
  -POPG3D_APPID_KEY="$(grep '^OPG3D_APPID_KEY=' \
     "$HOME/Studio canaryProjects/OPEN-PIXEL-GUN-3D-BACKEND/.env" | cut -d= -f2)"
```

Optional properties:

| Property | Meaning |
| --- | --- |
| `-POPG3D_DEFAULT_SERVER=host[:port]` | Fallback server for builds handed to players who cannot use LAN discovery. Empty by default. |
| `-PCMAKE_VERSION=<version>` | Overrides the default CMake 4.1.2 on a machine that has another one. |
| `-PPHOTON_APP_ID=<id>` | Bench-only fallback AppID. Leave unset for real builds: the point of the port is that the credential is not in the APK. |

Every build ends by dropping the library in the project root, in the same layout
the APK uses, so nothing has to be dug out of `build/intermediates`:

```
lib/arm64-v8a/libopg3d.so             stripped   - this is what goes in the APK
lib/arm64-v8a/libopg3d.unstripped.so  symbols    - keep for tools/symbolize_log.py
```

The copy is done by `export<Variant>NativeLib`, wired to `assemble<Variant>`, so
`assembleRelease` and `assembleDebug` each refresh their own build. The `lib/`
directory is gitignored. To refresh it without a full assemble:

```bash
./gradlew :opg3d:exportReleaseNativeLib
```

## 2. Put it in the APK

The library needs no managed glue: it has an ELF constructor plus `JNI_OnLoad`,
so it starts working the moment the loader maps it. Two ways to get it mapped,
both entirely on your side of the fence:

```bash
APK=pg3d-23.1.3.apk
unzip -o "$APK" -d work >/dev/null
cp lib/arm64-v8a/libopg3d.so work/lib/arm64-v8a/libopg3d.so

# a) if your loader already pulls it in (patched libmain.so DT_NEEDED, or a
#    smali System.loadLibrary("opg3d") call), just replace the file:
(cd work && zip -q -r ../repacked.apk . -x '*.DS_Store')

# b) fresh install into an untouched APK: add the DT_NEEDED to libmain.so once,
#    then step (a) is all you ever repeat.

zipalign -p -f 4 repacked.apk aligned.apk
apksigner sign --ks your.keystore --out signed.apk aligned.apk
adb install -r signed.apk
```

Re-signing is exactly what `version_2313::install_early_signature_patch` exists
for, so a self-signed build is expected and supported.

Don't forget the expansion file: `obb_provisioner` extracts it from the APK if
it is bundled, otherwise put it where the game expects it before first launch.

## 3. Point the client at a server

The client picks a server in this order, and remembers whatever worked:

1. `OPG3D_SERVER` in the process environment (for scripted runs);
2. the user-writable file — no root needed, any file manager will do:
   `/sdcard/Android/data/com.pixel.gun3d/files/opg3d-server.txt`, one line,
   `host` or `host:port` (`#` comments allowed);
3. the server remembered from the previous successful launch
   (`<app files>/opg3d/server.txt`);
4. UDP discovery on the local network — probe `OPG3D-DISCOVER-V1` broadcast to
   port 47318, which the backend answers with its name and HTTP port;
5. `OPG3D_DEFAULT_SERVER` from the build.

So for a LAN test you normally do nothing: start the server, start the game.
For a remote server, write one line into the file in step 2:

```bash
adb shell 'echo 203.0.113.10:8080 > \
  /sdcard/Android/data/com.pixel.gun3d/files/opg3d-server.txt'
```

The on-screen picker (LAN scan plus manual entry) is the next piece of work and
will write this same file, so nothing here changes when it lands.

## 4. Watch the log

```bash
adb logcat -c
adb logcat -s OPG3D:V
```

A healthy start looks like this — every line is one of ours, in order:

```
init: libopg3d build 23.1.3 ARM64 thin client v1 ... built Aug 27 2026 18:57:02
23.1.3-client: package=com.pixel.gun3d state_dir=/data/user/0/.../files/opg3d
23.1.3-client: backend selected via lan discovery -> 192.168.1.50:8080
23.1.3-client: req#1 -> GET (anon) http://192.168.1.50:8080/api/v1/ping ...
23.1.3-client: req#1 <- status=200 ms=3 bytes=84 body={"maintenance":false,...}
23.1.3-client: signed in as player_id=662808792 nick=<unset> session=9db17b...e052
23.1.3-client: opened the pun AppID envelope (36 chars, alg=AES-256-GCM)
init: [6/6] Assembly-CSharp.dll ready; installing 23.1.3 hooks
23.1.3-redirect: armed - 5/5 UnityWebRequest and 3/3 WWW entry point(s) ...
23.1.3-redirect: UnityWebRequest.Get (#1)
  game asked for: https://server-v2.pixelgun3dserver.com/auth_v2/?id_player=...
  sent instead to: http://192.168.1.50:8080/@server-v2.pixelgun3dserver.com/...
23.1.3-photon: mapped Switcher AppID selector -> the credential served by ...
```

What the noisy lines mean:

| Line | Meaning |
| --- | --- |
| `no backend found` | Steps 1-5 all came up empty. Write the file from section 3. |
| `did not answer /api/v1/ping` | Wrong address, server not running, or the phone is on another network. The request is logged in full above it. |
| `left untouched (#N)` | A URL that matched no retired host. Expected for CDN and store traffic. |
| `no backend is selected yet` | The game raced the handshake. Harmless once, repeated means section 3. |
| `the ... AppID envelope failed authentication` | Build key ≠ server `OPG3D_APPID_KEY`. |
| `the server refused this device: banned` | The ban you added with `/api/v1/admin/bans` works. |
| `req#N <- status=404 ... unhandled_route` | The server has no handler yet. It logs these as `http.unhandled`; that list is the to-do list for phase 2. |

Both sides log the same exchange, so a failure is always readable from two
angles: the client prints what it sent and expected, the server prints what it
received and answered.

## 5. Symbolizing a crash

Unchanged from the rest of the project: keep the unstripped `libopg3d.so` and
run `tools/symbolize_log.py` over the captured logcat. Native failures are also
forwarded to the server's `/api/v1/problem`, so a report survives a log that has
already rolled over.

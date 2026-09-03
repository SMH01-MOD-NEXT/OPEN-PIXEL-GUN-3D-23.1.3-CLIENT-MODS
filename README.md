# OLD-PIXEL-GUN-3D-2313-FRONTEND

Standalone native frontend for the frozen **Pixel Gun 3D 23.1.3** Android client (`arm64-v8a`). This repository contains the thin compatibility and transport layer only. Accounts, sessions, bans, progression, economy, remote configuration, telemetry, and other authoritative behavior belong to the separate GPLv3 server project `OPEN-PIXEL-GUN-3D-BACKEND`.

The project is an independent compatibility effort for a discontinued client. It is not affiliated with Cubic Games or Photon.

## Frontend responsibilities

`libopg3d.so` runs inside the game process and provides only behavior that cannot live on the backend:

- compatibility for the exact 23.1.3 APK, including re-signing and expansion-file handling;
- backend discovery, device registration, session establishment, and authenticated retrieval of the PUN credential;
- redirection of retired HTTP and Socket.IO endpoints to the selected backend;
- PUN route and credential injection for the legacy client;
- compile-time optional, observational diagnostics for reverse-engineering backend wire contracts.

Normal builds leave diagnostics disabled. Diagnostic hooks must not alter functional readiness or become an in-process backend.

## Requirements

- Android SDK with API 36;
- Android NDK `29.0.14206865`;
- CMake `4.1.2`;
- a JDK supported by Android Gradle Plugin 9.3.2;
- the included Gradle 9.7.1 wrapper.

Set `sdk.dir` in the ignored `local.properties` file before building.

## Build

A release build without embedded credentials is sufficient for compile verification:

```bash
./gradlew :opg3d:assembleRelease --no-daemon --stacktrace
```

For a client that can open the backend's sealed PUN envelope, provide the same key configured on `OPEN-PIXEL-GUN-3D-BACKEND` without committing it:

```bash
ORG_GRADLE_PROJECT_OPG3D_APPID_KEY='<shared key>' \
  ./gradlew :opg3d:assembleRelease --no-daemon --stacktrace
```

Optional Gradle properties:

| Property | Purpose | Default |
| --- | --- | --- |
| `OPG3D_DEFAULT_SERVER=host[:port]` | Fallback backend after manual configuration, remembered selection, and LAN discovery fail | empty |
| `OPG3D_DIAGNOSTICS=true` | Compile passive diagnostic hooks into the active profile | `false` |
| `CMAKE_VERSION=<installed version>` | Override the verified CMake version | `4.1.2` |

Every assemble task exports both native artifacts to the APK-style layout:

```text
lib/arm64-v8a/libopg3d.so
lib/arm64-v8a/libopg3d.unstripped.so
```

The `lib/` directory is generated and intentionally ignored.

## Checks

```bash
./gradlew check :opg3d:assembleRelease --no-daemon --stacktrace
```

CI also verifies that native hook frames do not reference LLVM's generic C++ exception personality.

## Backend boundary

The frontend never stores authoritative gameplay state and does not answer retired backend routes in-process. It selects a server, forwards the legacy client's traffic, and supplies exact-build compatibility. New endpoint behavior belongs in `OPEN-PIXEL-GUN-3D-BACKEND`.

See [`docs/PORT_23_1_3_THIN_CLIENT.md`](docs/PORT_23_1_3_THIN_CLIENT.md) for build, deployment, server selection, and log details.

## License

Project code is licensed under GPLv3; see [LICENSE](LICENSE). ShadowHook is MIT-licensed; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

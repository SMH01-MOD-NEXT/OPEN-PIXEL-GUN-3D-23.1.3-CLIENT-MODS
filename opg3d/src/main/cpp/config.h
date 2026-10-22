#pragma once

// Shared secret with OPEN-PIXEL-GUN-3D-BACKEND. It authenticates the sealed
// PUN AppID envelope; gameplay/account data is never compiled into this client.
#ifndef OPG3D_APPID_KEY
#define OPG3D_APPID_KEY ""
#endif

// Optional "host" or "host:port" fallback after manual config, remembered
// server and LAN discovery have all failed.
#ifndef OPG3D_DEFAULT_SERVER
#define OPG3D_DEFAULT_SERVER ""
#endif

// Diagnostics are observational hooks and are disabled in normal builds.
// Gradle: -POPG3D_DIAGNOSTICS=true.
#ifndef OPG3D_DIAGNOSTICS
#define OPG3D_DIAGNOSTICS 0
#endif

#ifndef OPG3D_BUILD_TAG
#define OPG3D_BUILD_TAG "23.1.3 ARM64 downloadable client v1 (native loader ABI1, Team Strike bots)"
#endif

#define OPG3D_BUILD_STAMP OPG3D_BUILD_TAG " built " __DATE__ " " __TIME__

// Focused end-of-match profile, independent of noisy startup diagnostics.
#ifndef OPG3D_POST_MATCH_TRACE
#define OPG3D_POST_MATCH_TRACE 0
#endif

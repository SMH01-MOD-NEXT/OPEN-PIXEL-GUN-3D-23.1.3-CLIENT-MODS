#pragma once

// PHOTON_APP_ID is only a fallback for bench builds. In a normal build the
// AppID is never compiled in: the backend serves it sealed per player and
// backend_client_2313 opens it at runtime.
#ifndef PHOTON_APP_ID
#define PHOTON_APP_ID ""
#endif

#ifndef PHOTON_SERVER_ADDRESS
#define PHOTON_SERVER_ADDRESS ""
#endif

#ifndef PHOTON_SERVER_PORT
#define PHOTON_SERVER_PORT 5055
#endif

// Shared secret with the backend's OPG3D_APPID_KEY. It only unseals Photon
// credentials; it grants no access to accounts, bans or player data, so a
// leaked build key costs one server-side key rotation and nothing else.
#ifndef OPG3D_APPID_KEY
#define OPG3D_APPID_KEY ""
#endif

// Optional "host" or "host:port" burned into the build, used only when the
// player has not chosen a server and LAN discovery found none. Public builds
// normally leave this empty and rely on discovery or the manual file.
#ifndef OPG3D_DEFAULT_SERVER
#define OPG3D_DEFAULT_SERVER ""
#endif

#ifndef OPG3D_BUILD_TAG
#define OPG3D_BUILD_TAG "23.1.3 ARM64 thin client v1 (self-hosted backend: LAN discovery + manual server file, device registration, sealed Photon AppID, update feed, full traffic trace) + signature compatibility + obb provisioner + boot diagnostics"
#endif

#define OPG3D_BUILD_STAMP OPG3D_BUILD_TAG " built " __DATE__ " " __TIME__

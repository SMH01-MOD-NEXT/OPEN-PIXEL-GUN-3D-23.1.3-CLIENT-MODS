#pragma once
// Host-only no-op Android logging shim for native observer regression tests.
#define ANDROID_LOG_INFO 4
#define ANDROID_LOG_WARN 5
#define ANDROID_LOG_ERROR 6
inline int __android_log_write(int, const char*, const char*) { return 0; }

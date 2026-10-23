# AWS signed payload refresh after launcher localization

The device retained a pending-start marker for build 33, equal to its active
SHA-256. The loader correctly refused another attempt with those identical
bytes. No markers or game data were deleted and no security checks were disabled.

Recompiled the accepted gameplay implementation with a fresh compile-time build
stamp, without gameplay source changes. Published signed build **35**, version
`23.1.3-launcher-refresh`, SHA-256
`3b0f333738aad22e45766e2ae1176fc6930fa7f9b12f0310a1d952a3ffdd0bd9`.
The resident APK loader is unchanged.

- Gradle check/release build, launcher localization tests and backend release tests passed.
- Verified signing-key pin, signature, size and public downloaded artifact hash.
- Atomically activated on EC2; backend/gateway PIDs unchanged; public ping healthy.
- Backup: `/var/backups/opg3d/aws-release35-20261023-083204`.
- Previous artifact retained; signed monotonic rollback build 36 prepared, not published.
- Nothing downloaded and confirmed build 35: highest=35, active=new SHA and pending
  marker removed by the native ready callback. User independently confirmed launch.

The old pending marker alone does not establish the cause of its interrupted
startup; no claim of a new crash fix is made by this release.

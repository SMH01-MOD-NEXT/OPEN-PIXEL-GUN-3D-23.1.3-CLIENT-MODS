# Post-match results investigation — 23.1.3

Status: diagnosis in progress; no gameplay fix claimed.

## Current evidence, not historical assumptions

The production recovery build reaches gameplay but leaves a lone victory/defeat
caption after a completed match. Existing post-match modules were removed in
`dd8d503`. Restoring their auto-Continue, coroutine restart or broad visibility
watchdog without checking the present runtime would mask the actual failure.

The first current capture used the focused passive profile, SHA-256
`68165845acd0e1d291772dcdb82b4fcd0d872a325450126665e67c46c9ac4b3a`.
On the rooted device, process 25221:

- Results wrapper B receives a real payload: exp 30, coins 8, showAward true.
- At 14:06:12 device time, the results iterator enters state 1 waiting for
  the tables animation flag. It is then stepped normally **74 times**.
- Both controller host activeSelf and activeInHierarchy remain true, as does
  Behaviour.enabled. The prior diagnosis of an inactive coroutine host does
  **not** explain this capture.
- OnTablesShow fires at 14:06:15 and clears the flag, followed by
  OnMatchEndAnimationDone and OnTablesShown.
- On the next attempted resume, iterator state becomes -1. There is no
  normal return/finish log from the MoveNext hook. Independent heartbeats
  continue for almost two minutes with the host active and the flag clear.
- ranksInterface, endInterfacePanel and the Continue button remain inactive.
  The visible finishedInterface panel has alpha 1; some child widgets are
  transparent, but this alone does not justify altering their animation state.

The important distinction is a non-returning iterator resume, consistent with
managed exception unwinding, rather than an iterator abandoned at a yield. Its
exact exception and failing method still require the next capture. The initial
trace did not log inside the runtime exception-raising path; Unity's ordinary
log stream contained no corresponding match-end exception.

Historical `finishedInterface=false` observations are unreliable: that field is
**GameObject**, not bool. The current trace reads its actual reference and tests
activity via Unity getters. State -1 also does not imply completion unless the
MoveNext call returned false; generated iterators write -1 on entry to a state.

## Focused profile

Build with the existing backend AppID envelope key provided through the normal
private build environment:

```sh
./gradlew check :opg3d:assembleRelease \
  -POPG3D_DIAGNOSTICS=false -POPG3D_POST_MATCH_TRACE=true --no-daemon
```

The profile defaults off and is independent of the startup diagnostic suite.
22 animation/driver hooks and two Update heartbeat hooks are checked at startup.
The observer records yield type, iterator steps, table wait flag, host ancestry,
root activity, and active NGUI component alpha/enabled counts. UI scans are
bounded to 512 components per root. The diagnostic iterator GC reference is
released on normal completion, replacement, destroyed controller or 120-second
expiry. It never schedules a coroutine or changes a field.

The second profile additionally validates the exact-build BL chain from exported
`il2cpp_raise_exception` through Exception::Raise to its returning stack capture
helper. After stock capture, scoped result/reward exceptions are formatted using
IL2CPP's own captured managed stack, without native unwinding or catch/suppress.
The original capture always runs; the original raising path then continues.
No exception observer is installed if its layout validation fails.

The second installed SHA-256 is
`e4aac4f02286fcf4de35ce77ed1962c9e235ae94822fa90ee482629e06c8f222`.
This is diagnostics, not a fix.

## Validation and next step

- Gradle check + assembleRelease pass for both focused and default-off profiles,
  with no native warnings. Default-off output was built but not installed; the
  device remains on the second focused profile.
- Four existing Python symbolizer tests and the URL-redaction C++ test pass.
- Host C++ observer tests cover optional-API failure, bounded GC ownership,
  replacement, normal completion, expiry, original Update delegation, scoped
  exception capture forwarding and scope cleanup.
- Device: 22/22 driver hooks and both heartbeat hooks installed successfully;
  a real completed match produced the evidence above.
- Data and preferences preserved; backend and economy behavior unchanged.

Complete another match with the second profile and leave the result screen
visible for 15 seconds. Use `23.1.3-post-match-exception` lines to identify the
real failing call. Do not revive an iterator that has unwound through an
exception, grant local rewards, or auto-press Continue.

Continuous private capture is kept under `~/.cache/opg3d-recovery/`; raw logs and
screenshots are not committed. Installed-library rollback is available in
`/data/local/tmp/opg3d-before-post-match-trace.so` (known-good startup release).

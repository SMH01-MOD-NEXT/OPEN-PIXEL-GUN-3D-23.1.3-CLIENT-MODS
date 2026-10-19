# 23.1.3 stopped loading clock recovery

## Confirmed failure (2026-10-19)

The rooted test device stayed in `Switcher.InitializeSwitcher` iterator state 23
at 25% for over six minutes. `Switcher`'s static Stopwatch had elapsed=0,
started=0 and is_running=false; the configured minimum delay was three seconds.
AuthScene was still Initial, before profile synchronization, so there were no
missing progress response packets that could unblock this particular wait.

Stock disassembly explains the loop:

- `AppsMenu` at 0x436FA58 caches `PlayerPrefs.HasKey("MainMenuControllerOnceEnabled")`.
- Its setup path at 0x436FF08 branches around `Stopwatch.Start` (0x436FF70)
  when that cached flag is true.
- Switcher's state 23 resumes at 0x217C438 and waits at 0x217D640:
  `ElapsedMilliseconds / 1000 < minSec`. It cannot complete with a stopped clock.
- Clearing preferences would change the branch, but risks losing account state
  and is not an acceptable fix.

## Narrow compatibility change

The `Switcher.InitializeSwitcher` factory wrapper first calls the stock factory
exactly once. For a non-null iterator, it obtains the named static Stopwatch
through IL2CPP metadata and calls stock `Start` only if `get_IsRunning` is false.
It never resets the clock, lowers the minimum delay, advances iterator/auth
states, alters profile data, removes the persistent flag, or synthesizes progress.
Metadata and method lookup occur during hook installation, but the static field
is read only on the caller's thread after the original factory returns.
Missing metadata leaves stock behavior unchanged with a diagnostic error.
The functional hook is independent of optional verbose diagnostics.

## Verification

- Host C++ regression checks cover stopped/running/null clocks, missing method
  functions, preserved elapsed time, exact original arguments and return identity,
  one stock call per wrapper invocation, failed factories and missing field API.
- Gradle check and both diagnostic and production ARM64 release builds passed.
  Four existing Python tests and the existing post-match observer host test passed.
- First corrected device boot passed state 23, completed state 45, entered AuthScene,
  downloaded the pending profile revision and reached the menu.
- Three further force-stop/cold starts reached the menu with level 65, 499 XP and
  unchanged coins/gems. Revision 20 received the raw echo and durable acknowledgement.
- All 87 server progress sections remained present. Relative to the pre-test
  snapshot, only existing fields in sections 14 and 21 changed during normal startup;
  all core slots and client-state sections remained intact.
- No application data/cache was cleared; local settings and full server profiles
  were privately backed up. No veteran chest was opened or reward granted.

This confirms the reproduced 25% startup defect, not every possible future
progress-loss cause. The user's reported 21% case needs fresh stage evidence if it
reappears. Unrelated tutorial-format and map-popularity response exceptions are
not hidden by this fix and remain separate follow-ups.

Production artifact SHA-256: `f9e95d4a86d1305bcb6982d3e23bb0c34d24d7315bd9eec97b73ddbd98e64e56`.
The backend was restarted with its existing binary to exercise reconnect; the
menu and saved profile remained intact. Backend reconstruction/relogin changes
were not folded into this frontend commit.

Two additional cold starts using the production library (diagnostics off) also
reached the menu with level 65 and 499 XP. The stopped clock was resumed on both
starts. All 87 sections and original server counters remained present. A final
comparison additionally found one item entry (`343009`) added to previously empty
section 37; no entries were removed there. The client coin display later differed
from the server counter, so complete gameplay-economy reconciliation is not
claimed by this startup-only fix. Chest reward issuance remains untested.

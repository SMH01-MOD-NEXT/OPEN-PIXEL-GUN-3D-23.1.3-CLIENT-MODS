# Stock bot combat tracking — exact 23.1.3

## Scope

Improve existing AIBotController-controlled bots across game modes; Team Strike (16) and Escort (17) receive stronger tuning. Does not add bots to other modes. Only the actual owning PhotonView's living player avatar is eligible, with a stock detector target. Test behaviors, player aim, remote bots and idle navigation are untouched. The user independently confirmed two-player Escort and host transfer before this work; this is user-reported verification, not an instrumented two-device aim test.

## Verified code paths

- AIBotController.LateUpdate `2144D84` validates the stock target and calls combat aim-point `214541C`; no replacement of target selection or visibility checks.
- `214541C` caches its output at B0 using scalar AC (field 七丌丙丁丝丝与与万); equal distance/radius inputs can reuse an old world-space aim point. Periodic invalidation of this scalar forces the original method to recompute its own target-relative point and existing weapon prediction. This addresses stale tracking without selecting targets or writing transforms.
- Combat rotation uses stock `Quaternion.Slerp` with deltaTime times getter `2143A64` (万丞丞下东丌下丞七). This hook returns an exponential-response equivalent rate, with a bounded per-frame fraction, rather than directly snapping rotation.
- Random aim magnitude helper `2146880` (丝丏丟丘与丐丌且与) caches its base at A8. Scale its returned value within the combat call only, not its cache or global difficulty configuration. No weapon spread/damage/rate-of-fire edits.
- Exact method addresses and named metadata must agree before any hook installation. Partial installation keeps tuning inactive and delegates to stock.

## Initial tuning

| Parameter | Other modes | Team Strike / Escort |
| --- | --- | --- |
| Forced aim-point refresh | 100ms | 65ms |
| Stock tracking-rate multiplier | 1.8 | 2.8 |
| Effective response-rate range | 6–9/s | 9–12/s |
| Stock random-error multiplier | 0.45 | 0.25 |

Positive stock error stays positive (floor0.002, but never larger than the original error). A zero/invalid stock error is preserved, not silently invented. The stock weapon still has its own spread, projectile travel and collision. Refresh is a maximum cache age while the combat getter runs, not a promise that every bot shoots at this frequency.

The rotation fraction is `min(1-exp(-rate*dt),0.35)`. At ordinary30/60/120fps its convergence is equivalent for these presets; a stalled frame cannot make the new tracking contribution snap directly to its target. Stock target acquisition/reaction, line of sight, firing eligibility, ammunition, reload and target loss remain authoritative. No aim through walls, head selection, perfect projectile interception or enforced hit percentage. “Rare misses” is a gameplay tuning goal, not an asserted measured hit rate.

Fixed64-slot refresh cache uses integer identity keys only, no allocations/GC roots; immediate refresh on a different target, cleanup on OnDestroy and stale-slot reuse. Changes are isolated to a scoped stock LateUpdate call, including stock passthrough for unrelated callers.

## Verification

Initial candidate33: `23.1.3-bounded-bot-aim`, SHA256 `170c1211bbb5107c7f02fb641caa3a2bbe68c81e0d96859499a0b134d4c20664`.

- Ten native test executables passed, including aim tuning, fps response, finite/stalled inputs, positive residual error, refresh timing/capacity/target replacement, owned/alive/target guards, stock passthrough and destroy cleanup.
- Aim and Escort tests under ASan/UBSan passed;11 Python regressions passed.
- Diagnostics-off Gradle check/assembleRelease passed in8s; diff whitespace check passed. Aim test added to CI.
- Nothing PID17023 loaded candidate33 and the runtime installation reported enabled. Online Escort room entered20:21:14; device combat observation is in progress.
- Local signed publication only. AWS remains custom/startup build20. Do not infer verified aim balance or hit rate from successful compilation/tests.

## First live combat observation — candidate33

Online Escort reached5/5 rosters and active Defend at20:22:49. Nothing’s live combat getter logged mode17, stock tracking coefficient6.000, tuned10.876 at dt0.0167 (Slerp fraction0.181), confirming that actual combat, not only idle navigation, uses the tuning. Forced point refreshes were observed throughout combat (9860 total by20:25:37). The scoreboard20:24 showed bot kills on both sides (e.g.7/2/1 on one side,2/2/1 on the other); DEV had0kills. No hit-rate percentage is inferred from this uncontrolled match.

The match was deliberately left through the stock Quit menu for additional-mode regression, not completed or disconnected. Online Team Strike entered20:27:19, real capacity6; its combat observation follows.

## Additional live mode checks and handoff

Candidate33 entered Team Strike with3/3 participants (five bots plus human). After the stock round transition, active combat20:30:01 logged mode16, interval65ms, stock6.000→tuned10.876 at60fps, fraction0.181. No conclusion about a measured Team Strike hit rate is claimed.

Ordinary Team Fight combat20:31:10 logged mode2, interval100ms, stock6.000→tuned8.358 at60fps, fraction0.139: the softer preset is demonstrably selected outside modes16/17. The same process17023 remained alive across Escort→Team Strike→Team Fight, with no captured Fatal signal/FATAL EXCEPTION. Mode switches used the stock Quit menu.

Local build33 is ready for subjective moving-target playtesting. The automated checks validate safety, scope and bounded tuning; uncontrolled bot kills and logs do not prove a particular hit percentage or difficulty balance. No APK reinstall, account reset, weapon damage/spread change, AWS deployment or network timeout adjustment. Earlier accepted Escort implementation saved separately in commit a358600; unrelated Photon trace and analys2332 work preserved.

## Accepted AWS publication

The user accepted the local gameplay and explicitly authorized AWS publication. On2026-10-23, official Sweden endpoint13.51.187.37:8080 atomically switched from build20 to the same tested signed build33, SHA256 `170c1211bbb5107c7f02fb641caa3a2bbe68c81e0d96859499a0b134d4c20664`, size461680bytes. Public manifest RSA signature and downloaded artifact size/hash verified. Ping and loader info returned200; public admin paths remained404. Backend/gateway stayed active with unchanged PIDs22117/21993; no backend executable, configuration or player-data replacement.

Previous build20 artifact retained; pre-publication manifest/key and a signed monotonic rollback34 manifest stored in root-only AWS backup `aws-release33-20261023-080204`. Rollback was not activated. Existing APKs receive the native payload on the next full launcher start; already-running games do not hot-swap code. No new APK installation is required for these gameplay changes.

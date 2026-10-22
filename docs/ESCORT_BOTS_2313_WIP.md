# Escort bot integration — work in progress

Exact game 23.1.3 only. UI Escort is Siege=17; ClanSiege=18 is not changed.

## Stock paths and restrictions

- Do not change GameConnect RVA 14D804C: this predicate also selects offline matchmaking.
- Share existing PlayerBotsManager hooks with Team Strike, with an exact-mode adapter.
- Siege roster rebuild 5088A3C filters human NST readiness (0x74). Supplement its lists with genuine replicated bot NSTs from GameController.onlineMode's list, preserving team and uniqueness. Do not fabricate Photon actors or write human-ready flags.
- Stock requires five participants per side before its normal initial countdown. Filling one bot per ten seconds takes about 95 seconds for a lone human.
- PlayerBotInstance avatar gate 2396D48 delegates to the mode controller, which excludes Escort. Extend only joined, enabled public Escort's active stages 4 and 6. Ownership, death state and respawn remain in stock caller.
- **AOT ABI caution:** callers 2395990/23959E4 omit the unused instance argument to 2396D48. x0 can be 1. Never dereference it; preserve the original context-free semantics. Candidate 25 exposed this; candidate 26 fixes it and the host test covers a poisoned pseudo-instance.
- Objective-priority locomotion replaces only Walking/Searching/Skirmish movement ticks (214B3DC, 1F4B854, 1F4BB18), plus Walking waypoint selection. Outside the exact ram detectBound, both roles move toward the real ram during active stages 4/6 while stock combat runs. Inside that volume, all three movement states and the waypoint chooser pass through to stock scanning, roaming and combat; do not pin bots stationary there. Enemy detection/state transitions, AIBotController.Update firing and LateUpdate enemy aim remain untouched. **214541C is an aim point, not locomotion: never replace it with the ram.**
- Steering is restricted to the actual owning PhotonView and living Player_move_c. Pathfinding is bounded to once per 500ms per AI, in a fixed 32-slot cache cleared for new rooms/scenes. New behaviors get a logical waypoint immediately without waiting for their next expensive path refresh. Stock complete-path/off-mesh-link-guarded helper 1F4D348 selects a reachable inset point inside the real detectBound, rather than forcing the potentially colliding centre. No teleport or fabricated proximity.
- Skirmish's cached pre-dodge acceleration (0x4C) is reused for objective locomotion. Stock Init/Exit and weapon-dependent movement speed remain authoritative. Non-Escort, disabled/private rooms, remote-owned bots, dead avatars and inactive rounds retain stock behavior.
- Bot avatars have real Player_move_c instances via PlayerBotController.player → PlayerBotEntity's field at 0x60. Use these in TramController's stock Add/Remove helpers with exact local detectBound containment. Never cast BotController to Player_move_c.
- Stock TramController retains progression, synchronization, gates, attack team, round timer, side changes and results. Reverse logic in the inspected stock implementation is a no-attacker timeout, not an explicit two-defender condition. No rule change is claimed here.

## Device evidence

Local signed candidate 26 SHA256 `3eb59798e81da82219970a97c722098425814a8cce797691fef8f602c0b3be0c`.
Nothing: verified payload; online master manager; nine staggered bots; 5/5 stock rosters; initial countdown and status 4. Real bot avatars visible. With human standing at defense spawn, ram progressed 0 → 22 → 96 → 122 → 150 metres. Stock side change reached stage 5 at 18:32:32 and stage 6 at 18:32:37; second-round ram reached 119 metres. Photon reconnects occurred at 18:34:43 and 18:34:49, and the user was removed from the match. The process remained alive; no candidate-26 fatal signal was observed. Android marked Wi-Fi net100 EXITING just before this, but the current release has passive Photon status tracing disabled, so causation is not established. A local trace-only candidate is being prepared; do not treat this as a completed match test.

Expanded host Escort tests cover boundaries/nonfinite positions, scheduling/stages, capacity, private/offline isolation, partial-install passthrough, shared-manager dispatch, unique managed-list insertion and context-free AOT spawn gate. All nine native regression binaries, Escort ASan/UBSan and 11 Python tests passed after the spawn-gate fix. Runtime disconnect diagnosis remains open.

## Pending before completion/release

- Both rounds and stock loss/post-match UI verified on candidate32; draw and gate-destruction outcomes remain separate test cases.
- Earlier candidate26 disconnect cause remains undiagnosed; successful repeats are not proof of a network fix.
- Realme+Nothing shared room, replicated objectives, human replacement and master transfer.
- In-game ordinary-mode/Team Strike/custom regressions.
- Final build/tests, isolated commits. Do not publish the unverified candidate to AWS.

AWS remains verified custom/startup build 20. No Escort changes committed yet.

## Disconnect follow-up

Local candidate27 (1fd33dafcf3ff0a346aae686f7aeca7ec6ac5546effdb369c2f6ca78a8ecd1b7) enables the existing passive Photon trace without changing gameplay or connection decisions. Nothing joined 18:43:17, nine bots filled 5/5, Defend at 18:44:53, side change 18:46:35, Attack at 18:46:40. Both rounds completed and the stock result UI was visible by 18:51:04, with Quit/Again and scores 131 vs149. No Photon timeout/server rejection, unexpected Disconnect call or process fatal was captured during this complete repeat. Result screenshot: private cache escort/trace27-result.png.

This does not prove the earlier disconnect was fixed. Candidate26 had Android Wi-Fi EXITING 18:34:35, datastall=1 18:34:44 and EXITING cleared 18:35:17; Photon cause tracing was off, so causation is unknown. Do not alter timeout/reconnect behavior or suppress disconnect callbacks without evidence.

Preparing local candidate28 with the same passive status/operation/disconnect diagnostics but without hot-path preconnect predicate logging. Temporary diagnostic build source overrides are restored byte-for-byte by the private build script; preserve the pre-existing photon_trace_2313.h changes. AWS remains build20. Two-peer/master-migration and other-mode device regressions remain unverified.

## Defender objective-priority follow-up

Candidate29 SHA256 `46cd76bbb7047ea3abc7c3f59aaa90b3265ed596f7dd8276ecdb8d1974a25f2c` on Nothing: joined online, nine bots, 5/5. Defend began 19:21:47; first-round distance was 47m (not the previous near-gate 150m). Stock side change 19:23:50 → Attack 19:23:55. A bot on the new defending side immediately followed a complete ram path: local horizontal displacement approximately130m at19:23:56, then97m at19:24:01,55m at19:24:06,15m at19:24:12 while in Skirmish. This directly confirms early objective approach rather than waiting at the defense spawn. Complete-match/continued combat observation still in progress at this entry.

Candidate30 SHA256 `890276be0d788b7133c1666384b988953c2149e6cb2876efc564e6c02b0c3727` is locally signed/published for the next launch, not loaded by the running29 process. It adds immediate waypoint refresh on behavior changes and bounded per-team proximity diagnostics. Both release builds passed; nine native regressions, Escort ASan/UBSan and 11 Python tests passed for29. Retest current30 refinements before acceptance. AWS unchanged, no Escort commit.

Candidate29 rejection: both rounds reached the stock result without a process fatal, but all bot kill counts were zero. Early approach alone is insufficient: permanently suppressing stock movement/search/combat ticks inside the ram volume prevents useful combat around cover. Do not accept29 or its unrun30 refinement as the final gameplay repair.

Candidate31 (`5bdee31275339e9092d09dba24bc136dc93785b597d0040bb11ac6f1f097bc93`) restores stock Searching and Skirmish ticks whenever the bot is inside the exact ram proximity bounds. Outside the volume, ram approach has priority; Walking never resumes distant random roaming. This preserves stock near-objective enemy search cancellation, target approach, strafing, dodge/jump selection and combat. It also includes per-team real-avatar proximity telemetry and immediate logical target refresh on state changes. Host tests now cover the near-objective combat passthrough, too. Native test and ASan/UBSan passed; release build31 passed. Device online match began19:30:38; combat effectiveness pending observation. Local-only, AWS unchanged.

## Current candidate32 — resumed validation

Local candidate32 SHA256 `d8b39c92e79456fad7dc7e35876884d11750836b9f81b2c2c5af65ecf06be59c` extends near-objective stock passthrough to **Walking too**, not just Search/Skirmish. This is essential: idle bots need their stock roaming/scanning to acquire targets around cover. Outside real detectBound all three states return to owner-only ram approach. Actual bounds in this test: centre(0,2.3,0), extents(5,2.8,6). No objective/score/round-time override.

MCP connection interrupted initial32 collection; do not infer initial match result from the later menu. On resume5456 remained alive. Repeated online match began19:45:25, Defend at19:47:00. At+10s the four bot defenders averaged70.2m from the ram; at+20s they averaged7.6m, with two already in exact proximity. Continued proximity and repeated returns after respawns observed. At19:50:38 the actual List of players showed autonomous combat on both sides (human DEV0kills): defender bot kills8,6,2,1; attacker bot kills8,4,3,3,2. This fixes the zero-combat regression of29/31; screenshot private cache escort/goal32-scoreboard.png. Ram140m at that scoreboard observation; stock result/side-switch still being recorded at this entry.

All nine native regression executables, 11 Python tests, expanded Escort isolation/near-combat cases under ASan/UBSan passed for current32 source. Default diagnostics-off Gradle check/assembleRelease passed (7s), as did local trace-enabled32. Test added to existing native CI workflow. Temporary main/photon diagnostic overrides match their backups byte-for-byte; pre-existing trace changes preserved. No Escort commit or AWS publication. Two-device/master-migration and ordinary-mode in-game regressions remain open.

## Candidate32 — completed repeat on Nothing

The recorded repeat reached stock side-change stage5 at19:51:03.340 and Attack6 at19:51:08.456. By19:54:19 the stock final result displayed **69 vs139**, “Enemy team has pushed the ram further”, and Quit/Again. This final result, not the rounded in-round140m observation, is the accepted score. Second-round result kills were team1:5,2,0,0 plus idle humanDEV0; team2:13,3,1,1,0. First-round kills were separately confirmed above. Thus autonomous objective approach and combat work in both roles, and the stock two-stage match ends without a forced score/result.

Process5456 stayed alive; the PID-specific repeat capture contains no Fatal signal or FATAL EXCEPTION. No reconnect/removal was observed in this repeat; the older disconnect cause is still not established. Evidence: private cache escort/goal32-current.png and escort/nothing32-repeat.log. No result buttons were pressed after completion.

This completes the single-device defender-priority validation, not the remaining two-peer/master-transfer, ordinary-mode device regressions or all outcome variants. No AWS update and no Escort commit performed.

## User acceptance — two devices and host transfer

After candidate32 single-device validation, the user confirmed that two-player Escort and host transfer work normally. This closes those user acceptance checks, but is not an agent-instrumented migration trace. Further bot aim tuning is a separate change documented in BOT_AIM_2313.md.

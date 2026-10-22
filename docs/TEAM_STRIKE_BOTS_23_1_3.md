# Online Team Strike bots — exact 23.1.3 ARM64 client

## Scope

Team Strike is `Rilisoft.GameMode.TeamDuel = 16`, not normal Team Fight (2).
`team_strike_bots_2313.h` extends only this mode. Escort and all other modes
keep their original behavior. Backend catalogs, balances, rewards and player
statistics remain server-owned. Native AI is simulated by the **Photon room
master**, not by the HTTP backend process.

An already joined online room receives one bot at a time, first after ten
seconds and then no faster than every ten seconds. Room capacity is read from
Photon (observed: six players, three per team), bounded by the native four-seat
TeamDuel UI. Real Photon players, connecting human slots and existing/pending
BotInstances are included. Native code assigns teams and stock backend-provided
names/equipment; no synthetic Photon user accounts or fabricated human results.

## Online invariant — do not remove

`GameConnect.丛丟世世世业丛一丛` at `0x14D804C` is NOT merely a spawn
permission. Overriding it for mode 16 also selects stock **offline matchmaking**.
The first device candidate demonstrated this and was rejected.

The accepted bridge never hooks this predicate and never toggles offline mode.
It requires both the actual Photon `offlineMode` field to be false and
`ClientState.Joined = 9`. The Joined state alone is insufficient: stock Photon
also reports Joined for its synthetic offline room. Disconnected, joining,
lobby, offline, non-master and invalid/final round states cannot schedule bots.

After a genuine online join, TeamDuel.Update adds the original
PlayerBotsManager component to its scene GameObject only if absent. Unity Object
truth semantics, not a raw managed pointer, detect destroyed managers left in
the static singleton by the native OnDestroy. TeamDuel.Awake resets the per-match
allocation latch and cadence, including transitions reusing a Photon room address. A failed
component allocation is attempted once per room, not every frame. All managed
calls and fields are resolved through IL2CPP metadata. The precise method RVAs
validate the 23.1.3 build before installing any hook; a partial install remains
inactive and delegates to native behavior.

## Native round integration

- The stock properties builder (`0x239E1DC`) and Photon BotInstance spawn
  (`0x239E53C`) are reused; the old burst-fill route is suppressed in this mode.
- TeamDuel already builds its roster from NetworkStartTables, including bots.
- Stock BotInstance initialization (`0x2396DA4`) explicitly skips immediate avatar
  creation for mode 16. An owned, registered bot with no avatar stays marked dead until the
  original TeamDuel round reset clears death flags and spawns its avatar.
  The manager checks the actual avatar, since NST registration may precede bot
  initialization. Existing avatars and remotely owned NSTs are never marked dead
  by this guard. This prevents an invisible unspawned bot holding a round open.
- No bot is inserted during the NewRound spawn/countdown transaction. A bot
  registered during play enters combat on the next native round reset. Ordinary
  round death/respawn and elimination code is not replaced.
- Excess bot seats after human joins are retired in Start/FinishedRound only,
  not in a live round. This deliberately avoids changing a live round's outcome.
- Room/authority loss resets the ten-second cadence. There is no catch-up burst
  after a pause or master change. Pending NST initialization blocks another spawn.

## Verification

Host `tools/test_team_strike_bots.cpp`: ten-second interval, no bursts, pending
rosters, room/team capacity, human occupancy, safe removal windows, new rooms,
master loss/acquisition, invalid/final states and offline-vs-online Joined guard.
Regression coverage checks destroyed-but-non-null Unity managers and three scene
initializations with the same room address, without a process restart.
Other existing startup/admin-resync/post-match/URL tests remain applicable.

Device verification uses the rooted connected ARM64 device with its existing
account data (no data clearing). Online-room evidence includes native
`offlineMode=false` + Joined guard and a real established UDP game-server socket
(port 5056), while the backend WebSocket remains connected. The online candidate
registered five bots at ten-second intervals and reached the native round
countdown at 3v3 and completed a live round followed by the next countdown and
playing state. Full evidence is recorded with the deployment.

The final release (`cb6baff07a3fd7e3cb02261c6905945add67c4b2a8e5be21aeedf691d2a8f6bc`)
was deployed without clearing data. Re-entry verification in one unchanged process
(PID 12097) observed the first manager at 10:28:29, new scene managers at
10:31:26 and 10:31:40, then fresh bot spawns at 10:31:51, 10:32:01 and 10:32:11.
The new scene reached five bots again at 10:32:31. An established online UDP
socket to port 5056 remained present. This verifies scene re-entry without
restarting the application. The exact post-results "next match without lobby"
UI route was not separately isolated in this run; its shared scene-reset
lifecycle is covered, but no separate UI result is claimed.

Not tested with a second physical client: real-human replacement and network
master migration. Policy behavior is host-tested, but two-client integration is
not claimed. Refill after master migration requires the new master to run this
updated client bridge. Existing vanilla network replication is not rewritten.

## Build / recovery

Run host tests and `./gradlew check :opg3d:assembleRelease --no-daemon` with the
existing private `OPG3D_APPID_KEY` environment; do not commit the key. Preserve
`-fno-exceptions -funwind-tables` for managed exception compatibility.

Back up the installed library, force-stop before replacement, restore executable
permissions/SELinux context, then start the existing app. Never clear preferences.
Restore the backed-up library to roll back; the backend needs no migration.

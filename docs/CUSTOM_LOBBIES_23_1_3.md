# Custom training rooms — 23.1.3 ARM64

## Cause and compatibility change

Stock 23.1.3 creates `TypedLobby("PixelGun3D", SqlLobby)` for custom browsing
and room creation, but PrivateGamesPanel consumes `PhotonNetwork.GetRoomList()`.
SQL lobbies do not push the ordinary game-list cache consumed by that panel.
The room properties are already listed correctly; inventing a backend room list
or removing client filters does not fix the mismatch.

`custom_lobby_2313.h` routes only stock custom browsing and custom creation to
`TypedLobby("OPG2313Customs", Default)`. The create path is recognized by the
existing nonempty string C1 property (password or stock passwordless sentinel).
The normal SQL lobby, unrelated named lobbies/types, room properties, visibility,
capacity, game/protocol partition, map filters and stock password UI are retained.

Targets are resolved through exact 23.1.3 metadata:

- GameConnect `丏丅丐上万业丆丅丛`/1: private connection entry.
- GameConnect `丘三丒丙丟丈下丁丛`/10: room options/create helper.
- TypedLobby `丝丑丆丒丌丅丆业丕` constructor/2.

A thread-local nested scope confines the constructor rewrite to those calls.
All three hooks must succeed before enabling routing; partial installations
remain pass-through. Discovery and creation use the same lobby name and type.
The Unity-thread bootstrap fix is a separate prerequisite commit.

This is a frontend compatibility repair; no new backend handler, account change,
or Photon plugin is required. Existing clients without this module still use
stock SQL custom lobbies and will not discover rooms in the new Default lobby.
They must restart on a server serving the updated signed module. No hot swap is
attempted inside a running game.

## Password boundary

The stock client compares the entered password to C1 before joining. This check
is preserved, not bypassed. Stock room passwords are not server-enforced access
control against a modified Photon client: C1 is a lobby property. Authoritative
password protection would require a separate server/plugin design. This change
does not claim to introduce stronger security than the original client.

## Verification and release

- Host test: disabled routing, normal/empty/non-string properties, passwordless
  sentinel and password, unchanged create arguments, nested/thread-local scopes,
  unrelated lobby names/types and null names. Strict compiler warnings passed.
- Gradle check and default-off diagnostic release build passed.
- Both connected devices loaded signed local build 20, installed the custom hooks
  from the Unity callback and stayed running after the startup repair.
- The user independently confirmed public custom rooms worked, then tested the
  password/private flow without agent device interaction and confirmed it worked.
  Wrong-password rejection and leave/rejoin were not separately instrumented by
  the agent; ordinary-match/end-of-match regression remains a separate follow-up.
- Tested payload: build 20, `23.1.3-custom-lobbies-unity-bootstrap`, SHA-256
  `33fd039a895631f92fcc297a9d165d6affa331673a4a2b3758198989dce6a256`,
  436472 bytes, ARM64 payload `libopg3d_client.so`, NOT the APK loader library.

User authorized publishing the tested module to the official AWS EC2 endpoint.
Only the signed native artifact/public key/active manifest are needed; preserve
server data, configuration, backend executable and running services. Retain old
artifacts and verify the public manifest, signature, binary hash and health.
Do not deploy signing private keys. A rollback for clients which accepted build
20 must publish the previous artifact under a higher signed build number, not
simply restore the old build-3 manifest.

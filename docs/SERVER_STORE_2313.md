# Self-hosted store catalog — 23.1.3

## Cause and scope

Stock MainStore uses coinsShop availability to gate both its Google Play message and catalog loading/rendering. Removing only the message leaves the catalog manager model unset: its loading coroutine waits for Google Billing readiness.

`server_store_2313.h` scopes the availability override to the catalog coroutine MoveNext and MainStoreView initialization, redraw and section selection. Each scope restores thread-local state before returning/yielding. Original coroutine return values and other readiness conditions remain unchanged. Hooks resolve exact 23.1.3 metadata, including the nested iterator; activation requires every hook and original pointer.

No Google identity, login token, billing inventory, receipt or payment-completion callback is fabricated. Calls outside these catalog scopes keep stock behavior. The existing backend PixelPass virtual-currency exchange catalog supplies the offers and validates settlement; this does not enable real-money Google Play purchases.

## Verification

- All 11 native test executables passed, including scope isolation, original result preservation, disabled behavior, nested scopes and thread/exception cleanup.
- Launcher RU/EN checks and 11 Python tests passed.
- Gradle check/release build passed; backend frontendrelease tests passed.
- Nothing A059P loaded all five server exchange cards with images and prices.
- User confirmed that goods can be purchased.
- A navigation tap during final checking accidentally selected the first virtual offer (100 pass tickets); disclosed immediately. User accepted the operation, so no account balances were restored.

## AWS release

Active signed gameplay build: **45**, version `23.1.3-server-store`.

SHA-256: `dbb48bbd9e81622ac1e42cfdcad39ed78efde9a5886ed640ba1f0d2c41645d28`.

EC2 backup: `/var/backups/opg3d/aws-release45-20261023-090418`.

Manifest signature, artifact size/hash and public health verified. Backend/gateway were not restarted. Monotonic rollback46 is prepared but not published. Device active hash matches build45. Resident loader is unchanged; existing compatible APKs fetch the module without reinstalling.

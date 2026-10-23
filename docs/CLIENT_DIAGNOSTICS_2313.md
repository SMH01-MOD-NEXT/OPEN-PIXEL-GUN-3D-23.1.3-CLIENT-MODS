# Client diagnostics (23.1.3 only)

`client_diagnostics_2313.h` hooks UnityEngine.Application.CallLogCallback and
SetLogCallbackDefined. The original callback always runs. Only Unity Error,
Assert and Exception types enter a bounded queue. No network request runs on
the Unity thread. The background worker sends sanitized reports to the selected
backend, using a registration-issued diagnostics-only credential and exact
endpoint/player guards. Registration responses and diagnostic bodies/credentials
are redacted from native transport logs.

The server signs a player-bound 24-hour diagnostics credential. Unlike gameplay
sessions it survives the stock client's bootstrap identity reconciliation.
It grants no gameplay or admin access. Restart a continuously running client
after 24 hours to renew it. Older servers without scoped credentials fall back
to regular session authentication; servers without the endpoint return 404.

Queue: 32 events in memory, duplicate message+stack suppression for 30 seconds,
5-second delivery interval, at most three attempts, 60-second pause on 429.
Only reports matching the original server/account are sent. Startup reports
without a player bind to the first authenticated account on the same endpoint.
Queue loss at process exit is intentional; no unbounded local log is uploaded.

`client_diagnostics_sink.h` decouples progress reload hooks from the sender.
`resync_started`, `resync_failed`, `resync_lobby_ready` annotate the real reload
lifecycle. Returning to the lobby does not assert the revision was applied;
only the backend's real progress acknowledgment records completion.

Privacy filtering removes credential lines and redacts URLs, email, paths and
long opaque IDs. It is defense in depth, not perfect PII detection. Never add
raw profile, purchase receipt, session or device identifier to diagnostic text.
Existing backend transport is HTTP; TLS is a separate deployment requirement.

Test with `tools/test_client_diagnostics.cpp` and `tools/test_admin_resync.cpp`.
The CI workflow runs the native diagnostic test. The production release is built
with OPG3D_DIAGNOSTICS=false; this bounded error pipeline remains enabled with
the backend feature. SIGSEGV/SIGABRT, ANRs, process death and pre-module startup
failures are NOT captured. Server documentation defines journal retention and
RU/EN admin views.

## Release verification

AWS serves signed module build 49 (`23.1.3-client-diagnostics-v2`), SHA-256
`4e9acbc2be3dceba7d7db9017b5cdae20340865c1f30ba2631ca66592d2015f0`.
Production APK and module builds pass. Existing loader downloads the signed
module on the next launch; installing another APK is not needed for that path.
Nothing disconnected from ADB before the final device delivery retest; pending
check is a 202 POST response and the matching event in the backend journal.

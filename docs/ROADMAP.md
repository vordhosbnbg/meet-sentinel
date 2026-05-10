# Meet Sentinel Roadmap

Last updated: 2026-05-10

## Update Protocol

This document is the project planning and memory tool. Keep it current whenever implementation, architecture, dependency, packaging, or reliability work changes project reality.

Rules for every update:
- Update `Last updated` using ISO date format `YYYY-MM-DD`.
- Preserve the roadmap entry format below.
- Change status only when the repository state supports it.
- Record concrete evidence for completed work: file paths, test names, build commands, or docs.
- Do not mark an item `done` if acceptance criteria are not met.
- Keep future work concise. Add detail when it changes implementation choices or reduces ambiguity for future agents.
- Add important architecture choices to the Decision Log.
- Add unresolved technical/product questions to Open Questions instead of hiding assumptions in milestone text.

Roadmap statuses:
- `planned`: not started beyond notes or placeholders.
- `in_progress`: implementation or design work exists, but acceptance criteria are not complete.
- `blocked`: cannot move forward without a named decision, dependency, or external action.
- `done`: acceptance criteria are met and evidence is recorded.
- `deferred`: intentionally out of current scope.

Roadmap entry format:

```text
### R-000 Short Title

Status: planned | in_progress | blocked | done | deferred
Updated: YYYY-MM-DD
Owner: project

Goal:
- One or two bullets describing the outcome.

Acceptance Criteria:
- Concrete, verifiable completion criteria.

Evidence:
- Files, tests, commands, docs, or decisions proving current status.

Notes:
- Optional implementation constraints, dependencies, or follow-up detail.
```

## Current Snapshot

The project is a local Windows-first tray app for reliable Google Calendar meeting reminders. Development happens on Linux, but the release direction is a statically linked Windows app with vendored dependencies.

Current technical direction:
- C++20.
- CMake + Ninja.
- Qt 6 Widgets for the current tray UI and popup UI only.
- Treat Qt as a replaceable UI toolkit; on Windows, a later UI layer may replace Qt with UWP or another native UI.
- Vendored Qt source under `third_party/qt`, built from a pinned release tag, for current UI development.
- Start with Qt `qtbase` and `qtwayland`: Core, Gui, Widgets, XCB, and Wayland. Do not use Qt Network for HTTP.
- Use vendored `libcurl` for Google OAuth and Calendar REST HTTP/TLS.
- Core reminder logic stays Qt-free and Google-free.
- Google/OAuth, event normalization, persistence, HTTP, and orchestration stay Qt-free.
- Native OS notifications are stretch behavior only; the custom popup is primary.

## Active Roadmap

### R-001 Repository Foundation

Status: done
Updated: 2026-05-09
Owner: project

Goal:
- Establish the initial repository structure, build system, and project instructions.
- Keep the Qt-free core buildable before vendored Qt is available.

Acceptance Criteria:
- `AGENTS.md` captures project goals, architecture rules, stack choices, and dependency policy.
- Root CMake project builds the core library and core tests without Qt.
- Source directories exist for core, app, Google adapters, persistence, UI, tests, docs, and third-party dependencies.
- Vendored dependency policy is documented.

Evidence:
- `AGENTS.md`
- `CMakeLists.txt`
- `src/core/reminder_engine.h`
- `src/core/reminder_engine.cpp`
- `tests/core/reminder_engine_test.cpp`
- `third_party/README.md`
- `docs/BUILDING.md`
- Verified on 2026-05-09: `cmake -S . -B build/dev`, `cmake --build build/dev`, `ctest --test-dir build/dev --output-on-failure`

Notes:
- Qt app target is intentionally skipped until a vendored Qt install prefix exists.
- The initial core reminder engine is a scaffold, not the full MVP decision model.

### R-002 Vendored Static Qt Toolchain

Status: done
Updated: 2026-05-10
Owner: project

Goal:
- Vendor Qt source and define repeatable static Qt builds for Linux development and Windows release.

Acceptance Criteria:
- `third_party/qt` is added as a pinned git submodule.
- Qt submodules are initialized with the minimal required module subset: `qtbase,qtwayland`.
- Static Qt build commands are validated and documented for Linux development with XCB and Wayland QPA backends.
- Windows static build commands are documented for MSVC release builds.
- The app target configures against `MEET_SENTINEL_QT_PREFIX`.

Evidence:
- `docs/BUILDING.md`
- `third_party/README.md`
- `CMakePresets.json`
- `scripts/bootstrap_qt_submodule.sh`
- `scripts/build_qt_linux_static.sh`
- `scripts/build_qt_windows_static.ps1`
- `.gitmodules`
- `third_party/qt` pinned to Qt `v6.11.0` with `qtbase` initialized.
- Verified on 2026-05-09: `QT_BUILD_PARALLELISM=4 scripts/build_qt_linux_static.sh`
- Verified on 2026-05-09: `cmake --preset app-vendored-qt --fresh`, `cmake --build --preset app-vendored-qt`, `ctest --preset app-vendored-qt`
- Verified on 2026-05-09: `cmake --preset core-dev`, `cmake --build --preset core-dev`, `ctest --preset core-dev`
- Updated on 2026-05-10: Linux Qt scripts now initialize `qtbase,qtwayland` and configure `-qpa "xcb;wayland"` with XCB as the default QPA backend.
- Verified on 2026-05-10: `cmake --preset app-vendored-qt`, `cmake --build --preset app-vendored-qt`, `ctest --preset app-vendored-qt`

Notes:
- Qt pin selected: `v6.11.0`.
- Build outputs and install prefixes stay ignored.
- Qt is UI-only. This milestone predates the decision to use `libcurl` for HTTP/TLS, and future Qt build scope should remove unnecessary Network usage.
- Linux development builds still rely on host desktop integration libraries such as XCB, Wayland, EGL/OpenGL, xkbcommon, and Freetype unless those are vendored later.
- The Wayland-enabled static Qt build is verified for the current Linux development toolchain.

### R-003 Core Domain Model And Reminder Decisions

Status: done
Updated: 2026-05-10
Owner: project

Goal:
- Implement deterministic reminder decisions for normalized meeting instances.

Acceptance Criteria:
- Core domain types represent normalized meeting instances, reminder kinds, snooze/dismiss/fired state, and decision reasons.
- Reminder decisions support T-10 and T+0 alerts.
- Decisions are explainable through structured reason data suitable for logs.
- Duplicate reminders are prevented using persisted fired-state inputs.
- Core tests cover edge cases around UTC time, restart behavior, cancelled events, stale cache use, snooze, dismiss, and late polling.

Evidence:
- `src/core/reminder_engine.h`
- `src/core/reminder_engine.cpp`
- `tests/core/reminder_engine_test.cpp`
- Verified on 2026-05-10: `cmake --build --preset core-dev`, `ctest --preset core-dev`
- Verified on 2026-05-10: `cmake --build --preset app-vendored-qt`, `ctest --preset app-vendored-qt`

Notes:
- Core must not depend on Qt or Google API types.
- Use `std::chrono` internally.
- Persistence is still planned under R-004; R-003 defines the state and key types that persistence should serialize.

### R-004 Persistence And Logs

Status: done
Updated: 2026-05-10
Owner: project

Goal:
- Persist settings, fired reminders, cached events, credentials, and logs locally.

Acceptance Criteria:
- JSON settings schema exists.
- Fired reminder state persists across restarts.
- Recent normalized event cache persists and can be used during temporary network failures.
- File logging records polling, normalization, reminder decisions, stale state, auth state, and errors.
- Credential storage abstraction exists, with secure storage preferred and explicit dev fallback behavior.

Evidence:
- `third_party/nlohmann_json` pinned to `v3.12.0`
- `src/persistence/json_persistence.h`
- `src/persistence/json_persistence.cpp`
- `src/persistence/file_logger.h`
- `src/persistence/file_logger.cpp`
- `src/persistence/credential_store.h`
- `src/persistence/credential_store.cpp`
- `tests/persistence/persistence_test.cpp`
- Verified on 2026-05-10: `cmake --preset core-dev`, `cmake --build --preset core-dev`, `ctest --preset core-dev --output-on-failure`
- Verified on 2026-05-10: `cmake --preset app-vendored-qt`, `cmake --build --preset app-vendored-qt`, `ctest --preset app-vendored-qt --output-on-failure`

Notes:
- Uses vendored `nlohmann_json` for persistence schemas.
- `PlaintextCredentialStore` is an explicit development fallback only. A secure Windows credential adapter remains a future implementation choice.

### R-005 Google OAuth And Calendar Polling

Status: in_progress
Updated: 2026-05-10
Owner: project

Goal:
- Authenticate with Google and poll the primary calendar reliably.
- Keep Google/OAuth and HTTP code independent from Qt so the UI toolkit can be replaced later.

Acceptance Criteria:
- A Qt-free `HttpClient` abstraction exists for OAuth and Calendar API code.
- A production HTTP provider uses vendored `libcurl`.
- The Windows `libcurl` direction prefers Schannel so OpenSSL DLLs are not required for release builds.
- OAuth Authorization Code + PKCE flow works for a native app.
- Primary calendar events are fetched through Google Calendar REST.
- Polling runs immediately on startup.
- Polling repeats on a configured interval.
- Polling runs immediately after system resume.
- Temporary network failure keeps recent cached events available.
- Auth errors and fetch errors are represented in app state and logs.

Evidence:
- `third_party/curl` pinned to `curl-8_20_0`
- `src/google/http_client.h`
- `src/google/http_client.cpp`
- `src/google/curl_http_client.h`
- `src/google/curl_http_client.cpp`
- `src/google/pkce.h`
- `src/google/pkce.cpp`
- `src/google/oauth_client.h`
- `src/google/oauth_client.cpp`
- `src/google/calendar_client.h`
- `src/google/calendar_client.cpp`
- `tests/google/google_client_test.cpp`
- Verified on 2026-05-10: `cmake --preset core-dev`, `cmake --build --preset core-dev`, `ctest --preset core-dev --output-on-failure`
- Verified on 2026-05-10: `cmake --preset app-vendored-qt`, `cmake --build --preset app-vendored-qt`, `ctest --preset app-vendored-qt --output-on-failure`

Notes:
- Do not use Qt Network or Qt NetworkAuth in this milestone.
- Keep `libcurl` types behind the HTTP provider boundary; Google/OAuth code should depend on a small project-owned interface.
- OAuth loopback callback handling should also remain Qt-free.
- Implemented so far: HTTP abstraction, `libcurl` provider, PKCE challenge generation, OAuth authorization/token request client, and raw Calendar events fetch client.
- Remaining before `done`: loopback callback server, app/user credential wiring, poll scheduling, immediate startup/resume polling, failure-to-cache behavior in the integrated app state, and live/manual OAuth validation with a Google native-app client.

### R-006 Event Normalization

Status: planned
Updated: 2026-05-09
Owner: project

Goal:
- Convert Google Calendar event data into stable, UTC normalized meeting instances.

Acceptance Criteria:
- Single and recurring event instances are represented with stable instance IDs.
- Start/end times are normalized to UTC.
- Cancelled events are handled.
- Join URLs are extracted for Google Meet and common conference/link fields.
- All-day/non-meeting events are filtered or classified according to explicit rules.
- Normalization tests use representative Google Calendar payload fixtures.

Evidence:
- Planned only.

Notes:
- Normalized data is the only input to reminder decisions.

### R-007 Tray State And Popup Alerts

Status: planned
Updated: 2026-05-10
Owner: project

Goal:
- Provide the visible tray state and primary popup alert UI.
- Keep the UI layer narrow enough that the current Qt Widgets implementation can later be replaced on Windows.

Acceptance Criteria:
- Tray icon shows normal, stale, auth-needed, and error states.
- Popup alert appears for T-10 and T+0 reminders.
- Popup includes Join, Snooze, and Dismiss actions.
- Join opens the meeting URL.
- Snooze and Dismiss update decision inputs so reminders do not repeat incorrectly.
- UI does not own polling, Google fetch, normalization, persistence, or core decision logic.

Evidence:
- Planned only.

Notes:
- Native OS notifications remain secondary/stretch.
- Qt Widgets is the current UI implementation only; do not move HTTP, polling, persistence, or reminder logic into Qt UI classes.

### R-008 Reliability Pass

Status: planned
Updated: 2026-05-09
Owner: project

Goal:
- Verify the app handles restart, resume, stale cache, auth failure, and network failure paths.

Acceptance Criteria:
- Restart does not duplicate already fired reminders.
- Resume triggers an immediate poll.
- Network failure shows stale/error state and keeps using recent cached events.
- Auth failure shows visible auth state.
- Logs explain each reminder emitted or skipped.
- Manual test checklist exists for Windows tray behavior.

Evidence:
- Planned only.

Notes:
- This milestone should not close until the integrated app behavior is tested, not only core logic.

### R-009 Windows Packaging

Status: planned
Updated: 2026-05-10
Owner: project

Goal:
- Produce a Windows release artifact with minimal runtime dependencies.

Acceptance Criteria:
- Release build uses the current vendored static Qt UI layer, or a documented replacement UI layer if the Windows UI has moved to UWP by then.
- MSVC runtime strategy is documented and validated.
- Required Qt static plugins are explicitly controlled.
- Vendored `libcurl` is built with a TLS/backend strategy that minimizes shipped DLLs, preferably Schannel on Windows.
- Release artifact launches on a clean Windows environment matching the supported baseline.
- Packaging notes list any unavoidable DLL/system dependencies.

Evidence:
- Planned only.

Notes:
- Do not let packaging requirements pull the app toward a backend service.

### R-010 Native OS Notifications

Status: deferred
Updated: 2026-05-09
Owner: project

Goal:
- Add native OS notifications as a secondary alert channel.

Acceptance Criteria:
- Native notifications can be enabled without replacing the custom popup.
- Missed or blocked native notifications do not prevent popup alerts.
- Notification behavior is optional and documented as stretch functionality.

Evidence:
- Deferred by MVP scope.

Notes:
- This is not required for MVP.

## Decision Log

### D-001 Qt Boundary

Date: 2026-05-09

Use Qt for the current UI, tray, popup, and platform-facing desktop UI behavior. Keep core reminder logic and domain data Qt-free.

Updated 2026-05-10: D-007 narrows this boundary further. Qt should not be used for HTTP, Google/OAuth, persistence, normalization, or non-UI orchestration.

### D-002 Vendored Dependencies

Date: 2026-05-09

All third-party source dependencies should live as pinned git submodules under `third_party/`. Qt is included in this rule and should be vendored under `third_party/qt`.

### D-003 Static Windows Release Direction

Date: 2026-05-09

Prefer static Qt and static MSVC runtime for Windows release builds to minimize shipped DLLs. Accept unavoidable Windows system dependencies.

### D-004 Avoid Qt Network Authorization For Now

Date: 2026-05-09

Avoid `Qt NetworkAuth` because Qt is now UI-only in this project. Implement OAuth Authorization Code + PKCE directly on top of the project HTTP abstraction.

### D-005 Reminder Evaluation Trace

Date: 2026-05-10

Core reminder evaluation returns both emitted reminder decisions and skip/emit traces with stable reason codes. UI should consume only decisions; logging should consume traces so every reminder outcome is explainable without moving decision logic into UI or persistence.

### D-006 JSON Persistence Library

Date: 2026-05-10

Use vendored `nlohmann_json` pinned to `v3.12.0` for local JSON persistence. Keep schemas based on standard-library and core domain types, not Qt JSON types.

### D-007 Qt UI-Only Boundary

Date: 2026-05-10

Qt is the current UI toolkit only. New non-UI code must not depend on Qt, including Google/OAuth, HTTP, event normalization, persistence, reminder decisions, polling orchestration, and logging. Keep UI contracts narrow enough that the Windows UI can later be replaced by UWP or another native UI layer.

### D-008 HTTP Client Library

Date: 2026-05-10

Use a project-owned Qt-free `HttpClient` abstraction for Google OAuth and Calendar REST. Implement the production provider with vendored `libcurl`; prefer Schannel for Windows release builds to avoid shipping OpenSSL DLLs. Keep `libcurl` C API types behind the provider boundary.

Updated 2026-05-10: `third_party/curl` is pinned to `curl-8_20_0`. Linux development builds currently use host OpenSSL; Windows release builds should use Schannel.

## Open Questions

- Which test framework should be vendored once plain `assert` tests are no longer sufficient?
- What secure credential storage approach best fits static Windows packaging?
- What Windows baseline should release artifacts support?
- Should the OAuth loopback callback server use raw sockets, a tiny vendored HTTP server, or a minimal `libcurl`-adjacent helper?
- How much polling orchestration belongs in a UI-independent app service versus current Qt app bootstrap glue?
- What is the expected timing and scope for replacing Qt Widgets with UWP on Windows?

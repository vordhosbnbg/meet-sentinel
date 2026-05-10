# Project Direction

This is a small personal Windows tray app for reliable meeting reminders from Google Calendar.

Primary goal: avoid missed meetings.
Secondary goal: stay simple and maintainable.

Do not build a SaaS backend.
Do not use Google Calendar push webhooks.
Do not rely on native OS notifications as the primary alerting mechanism.
The primary alert UI is an app-controlled popup window.

## Architecture Rules

Separate:
- Google Calendar fetching
- event normalization
- reminder decision logic
- UI/tray rendering
- persistence

Reminder logic must be testable without Qt and without Google APIs.

Use polling. Every reminder decision should be explainable in logs.

## Technology Stack

- Use C++20.
- Use CMake and Ninja as the primary build system.
- Use Qt 6 Widgets only for the current tray UI, app-controlled popup UI, and platform-facing desktop UI behavior.
- Treat Qt as a replaceable UI toolkit. On Windows, a future UI may replace Qt with UWP or another native UI layer.
- Keep Qt out of core, Google/OAuth, HTTP, event normalization, reminder decision logic, and persistence.
- Prefer the C++ standard library for core data structures, time math, filesystem paths, and pure application logic.
- Use small, targeted third-party libraries where the standard library is missing important pieces.
- Use vendored `nlohmann_json` for JSON persistence.
- Use vendored `libcurl` for HTTP/TLS in Google OAuth and Calendar adapters. Qt Network must not be used for HTTP.
- Native OS notifications are a stretch goal and secondary alert channel only. The app-controlled popup remains the primary alert UI.

## Vendored Dependencies

All third-party source dependencies must be pinned git submodules under `third_party/`.

Qt is vendored under `third_party/qt` and built from source. Build artifacts, install prefixes, caches, and generated dependency files must not be committed.

Default Qt scope:
- Start with the Qt `qtbase` module only.
- Required Qt components for the current app are expected to come from `qtbase`: Core, Gui, and Widgets.
- Do not add Qt Network for Google/OAuth or Calendar API access.
- Do not add QML/Quick, WebEngine, Multimedia, SQL, or other Qt modules unless a concrete feature requires them.
- Avoid GPL-only Qt modules unless the project explicitly accepts the license impact.

Release direction:
- Prefer static Qt builds.
- Prefer static MSVC runtime for Windows release builds.
- Minimize shipped DLLs, while accepting unavoidable Windows system DLLs and platform runtime realities.
- Prefer a `libcurl` Windows build using Schannel to avoid shipping OpenSSL DLLs.

## Important Reliability Requirements

- Poll immediately on startup.
- Poll immediately after system resume.
- Persist fired reminder state.
- Do not duplicate reminders after restart.
- Continue using recent cached events during temporary network failure.
- Show visible stale/auth/error state in tray.
- Normalize internally to UTC.
- In core logic, use `std::chrono` time points rather than Qt date/time types.
- At UI boundaries, convert any UI toolkit date/time types to UTC before passing data inward.

## MVP Scope

Implement:
- Google OAuth
- primary calendar polling
- normalized meeting instances
- tray icon state
- custom popup at T-10 and T+0
- Join / Snooze / Dismiss
- JSON settings
- file logs

Do not implement:
- multi-account support
- calendar writes
- cloud backend
- webhook push notifications
- complex settings UI
- AI/ML filtering

## Repository Layout

- `src/core/`: Qt-free domain types and reminder decision logic.
- `src/app/`: application bootstrap and dependency wiring.
- `src/google/`: Google OAuth and Calendar API adapters.
- `src/persistence/`: settings, fired reminder state, cached event state, credential storage adapters.
- `src/ui/`: Qt tray, popup, and platform UI code.
- `tests/core/`: fast tests for Qt-free logic.
- `third_party/`: pinned third-party git submodules only.
- `docs/`: project notes that are useful for future agents and humans.

## Design Rules

- Keep reminder decisions deterministic: inputs are normalized meeting instances, fired reminder state, current UTC time, and settings.
- Keep Google Calendar fetching, normalization, reminder decisions, UI presentation, and persistence independently testable.
- Do not put `QString`, `QDateTime`, `QJsonObject`, `QNetworkReply`, or other Qt types in `src/core/`.
- Do not put Qt types in `src/google/`, `src/persistence/`, or future non-UI orchestration code.
- Do not perform Google API calls from UI classes.
- Do not perform reminder decision logic in UI classes.
- Do not treat native OS notifications as sufficient for MVP reliability.
- Every emitted reminder should have a stable fired-state key based on calendar/event instance identity and reminder kind.
- Every skipped or emitted reminder decision should be loggable with a clear reason.

## Build Rules

- The core library and core tests should configure and build before vendored Qt is available.
- The Qt app target should build only when a vendored Qt install prefix is supplied, unless `MEET_SENTINEL_ALLOW_SYSTEM_QT=ON` is intentionally set for local experimentation.
- Prefer out-of-source builds under `build/`.
- Do not commit generated build directories or vendored dependency build/install outputs.

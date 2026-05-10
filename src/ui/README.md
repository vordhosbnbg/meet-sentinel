# UI

This directory is for Qt tray state, popup windows, and future secondary native notification integration.

The app-controlled popup is the primary alert path. Native OS notifications are secondary/stretch behavior.

Qt is the current UI toolkit only. UI code may depend on Qt, but core, Google/OAuth, HTTP, normalization, and persistence code must not.

Windows direction:

- Keep UI boundaries narrow enough that Qt Widgets can later be replaced by UWP or another native Windows UI layer.
- Do not let Qt own Google API calls, reminder decisions, or persisted state semantics.

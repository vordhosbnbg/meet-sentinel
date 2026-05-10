# Google Adapter

This directory is for Google OAuth and Calendar REST adapters.

Keep Google API and HTTP/TLS details out of `src/core/`. Convert fetched data into normalized core meeting instances before reminder decisions run.

HTTP direction:

- Define a small Qt-free `HttpClient` abstraction for Google/OAuth code.
- Implement the production HTTP provider with vendored `libcurl`.
- Do not use Qt Network or Qt NetworkAuth in this layer.
- Keep `libcurl` C API types behind the adapter boundary.

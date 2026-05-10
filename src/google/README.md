# Google Adapter

This directory is for Google OAuth and Calendar REST adapters.

Keep Google API and HTTP/TLS details out of `src/core/`. Convert fetched data into normalized core meeting instances before reminder decisions run.

HTTP direction:

- Define a small Qt-free `HttpClient` abstraction for Google/OAuth code.
- Implement the production HTTP provider with vendored `libcurl`.
- Do not use Qt Network or Qt NetworkAuth in this layer.
- Keep `libcurl` C API types behind the adapter boundary.

Current components:

- `http_client.*`: Qt-free HTTP request/response interface.
- `curl_http_client.*`: production `libcurl` provider.
- `pkce.*`: Authorization Code + PKCE helper functions.
- `oauth_client.*`: authorization URL, token exchange, and refresh request client.
- `loopback_authorization_server.*`: native-app loopback callback receiver.
- `calendar_client.*`: raw Google Calendar events fetch client.

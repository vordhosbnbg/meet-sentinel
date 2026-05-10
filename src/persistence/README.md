# Persistence

This directory is for JSON settings, cached events, fired reminder state, credentials, and file logging adapters.

Persisted schemas should use standard-library-friendly domain data and a vendored JSON library. Keep Qt JSON types at the edge if they are used for adapter glue.

Current components:

- `json_persistence.*`: JSON settings, fired/dismissed/snoozed reminder state, and normalized event cache round-trips.
- `file_logger.*`: JSON-lines file logging for polling, normalization, reminder decisions, stale state, auth state, and errors.
- `credential_store.*`: credential storage abstraction plus an explicit plaintext development fallback.

Credential direction:

- Prefer a secure platform-backed store for production Windows builds.
- Use `PlaintextCredentialStore` only as a development fallback or until a secure adapter is selected.

---
name: nerack-deploy
description: Deploying a Nerack app to production, either as a single binary copied to a Linux host (a VPS or bare server, with systemd and a reverse proxy) or as a Docker or Podman image. Use when shipping, hosting, upgrading, or configuring a Nerack app in production.
---

# Nerack: deploying

There are two ways to ship an app. Use the one the user names; if they have not said, ask which they want.

- **Binary**: copy `output/app.<arch>` to any Linux host of that architecture and run it, for example under systemd on a VPS. Read [references/binary.md](references/binary.md).
- **Image**: `nerack docker` builds a Docker image (Podman works through its `docker` command) to run with Compose, push to a registry, or deploy to a container platform. Read [references/docker.md](references/docker.md).

## What both share

- Build first. `nerack build` writes `output/app.<arch>` for each architecture in the project's `.nerack/arch` (`x86_64` unless changed): one static Linux binary with every module and asset embedded. For arm64 hosts (AWS Graviton and the like) list `aarch64` there, or pass `--arch arm64` for one build; any machine builds either. It fails on any compiler warning, so fix warnings in dev mode before deploying.
- The binary is the whole app. It brings its own libc, needs no system libraries, and never reads `.env`. Everything it needs at run time comes from environment variables, flags, or secret files (below).
- Nerack does not terminate TLS. Put a reverse proxy or load balancer (Caddy, Nginx, a cloud load balancer) in front for HTTPS, and set `TRUSTED_PROXIES` to its address so `server:client_ip` is the real client's.
- Run one process per host: it owns its SQLite databases. `WORKER_COUNT` and `HTTP_REACTOR_COUNT` (below) set how much of the machine it uses.
- Data: SQLite databases are files under `DATA_DIR`, Nerack's own included (`auth_db`, `cookie_auth_db`, `rns_auth_db`, `pubsub_db`, `task_db`), owned by that one process. Back up that folder. To share data across processes or hosts, use a networked engine such as Postgres, added as a vendor module, with its connection read from the environment (`postgres("todos_db", "{{env:database_url}}", ...)`), and point overridden bundled databases at it the same way.
- Migrations and seeds run at startup, each once, so starting a new build migrates its databases. They only move forward: rolling back to an older binary does not undo them.
- Undelivered events and unfinished tasks are durable, so they resume after a restart.
- On SIGTERM, Nerack closes open SSE streams at once (clients reconnect), gives other requests `SHUTDOWN_TIMEOUT` seconds, and lets running task steps checkpoint. Keep the orchestrator's stop grace period above it (`stop_grace_period` in compose, `terminationGracePeriodSeconds` in Kubernetes).

## Configuration

Every setting is an environment variable, and every non-secret one also a flag (lowercase, `_` as `-`: `PORT` → `--port`); a flag wins. Every variable, Nerack's or the app's own, is in context as `env:<lowercased name>`. Each file in `/run/secrets/` becomes the variable named after it (`/run/secrets/cookie_secret` is `COOKIE_SECRET`); a name set both ways fails startup. Secret values never appear in logs.

| Variable | Default | Purpose |
|---|---|---|
| `PORT` | `3000` | HTTP listening port. |
| `COOKIE_SECRET` | none | Signs every cookie. Required in production by an app that uses `http.h`; generate one with `openssl rand -hex 32` and keep it stable, or every session ends. |
| `DATA_DIR` | working directory | Base for relative SQLite paths, Nerack's own databases included. |
| `HTTP_REACTOR_COUNT` | `1` | HTTP reactor threads; one serves many thousands of connections, so raise it only for heavy HTTP traffic. |
| `WORKER_COUNT` | half the CPU cores, at least 2 | Threads for `run_worker()`, database calls, and other blocking work. |
| `SHUTDOWN_TIMEOUT` | `10` | Seconds a stopping process gives in-flight requests to finish. |
| `TRUSTED_PROXIES` | none | Addresses and CIDR ranges whose `X-Forwarded-For` is trusted. |
| `SSL_CERT_FILE` | system store | CA certificates for `https://` calls from `http_fetch()`; without it the host's store, else a bundle built into the binary (for `FROM scratch`). |
| `MAX_UPLOAD_BYTES` | 10 MB | Largest accepted upload. |
| `SESSION_DAYS`, `SESSION_MAX_DAYS` | 30, 90 | Session lifetime without use, and however active. |
| `OTEL_EXPORTER_OTLP_ENDPOINT`, other `OTEL_*` | `http://localhost:4318` | Where OpenTelemetry spans, logs, and errors go; `OTEL_SERVICE_NAME` defaults to the project folder's name. With no collector there, the app logs that and runs on. |

Add the app's own variables (database URLs, API keys) the same way, and read them as `{{env:name}}`.

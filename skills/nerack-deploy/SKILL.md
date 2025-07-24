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
- The binary is the whole app. It brings its own libc, needs no system libraries, and never reads `.env`. Everything it needs at run time comes from environment variables, flags, secret files, or `SECRET__` variables (below).
- Nerack does not terminate TLS. Put a reverse proxy or load balancer (Caddy, Nginx, a cloud load balancer) in front for HTTPS, and set `TRUSTED_PROXIES` to its address so `server:client_ip` is the real client's.
- Run one process per host: it owns its SQLite databases. `WORKER_COUNT` and `HTTP_REACTOR_COUNT` (below) set how much of the machine it uses.
- Data: SQLite databases are files in `data/` under the working directory (a relative path such as `file:todo.db` is `data/todo.db`), Nerack's own included (`auth_db`, `cookie_auth_db`, `rns_auth_db`, `pubsub_db`, `task_db`), owned by that one process. Back up that folder. To share data across processes or hosts, use a networked engine such as Postgres, added as a vendor module, with its connection read from a secret (`postgres("todos_db", "{{secret:database_url}}", ...)`), and point overridden bundled databases at it the same way.
- Scaling past one machine is by placement, never by splitting modules into services: run the same binary on several machines (bigger ones for the heavy part, such as an `images` module) and have the load balancer send each group its routes. Data every machine reads, users and sessions included (`auth_db`, `cookie_auth_db`), goes on a shared networked engine; `task_db` and `pubsub_db` can stay on SQLite, so each machine runs the work of the requests it received, but cron then fires on every machine, so move `task_db` to the shared engine to run each schedule once. Per-tenant SQLite works only when the load balancer keeps each tenant on one machine.
- Migrations and seeds run when each database first opens (on its first query, never all at boot), each once, so a new build migrates its databases as it starts using them. They only move forward: rolling back to an older binary does not undo them.
- Undelivered events and unfinished tasks are durable, so they resume after a restart.
- On SIGTERM, Nerack closes open SSE streams at once (clients reconnect), gives other requests `SHUTDOWN_TIMEOUT` seconds, and lets running task steps checkpoint. Keep the orchestrator's stop grace period above it (`stop_grace_period` in compose, `terminationGracePeriodSeconds` in Kubernetes).

## Configuration

Every setting is an environment variable, and every non-secret one also a flag (lowercase, `_` as `-`: `PORT` → `--port`); a flag wins. Every variable but secrets, Nerack's or the app's own, is in context as `env:<lowercased name>`, holding the value in effect. Secrets are the `secret:` scope, never `env:`: each file in `/run/secrets/` (`/run/secrets/cookie_secret` is `secret:cookie_secret`) and each `SECRET__<NAME>` variable (`SECRET__API_KEY` is `secret:api_key`); the variable wins over the file, both over dev's `.env` (and `nerack test`'s `.env.test`), and startup logs which source won, never the value. Templates read a secret only with `{{secret:name}}`. Secret values never appear in logs, telemetry, or checkpoints.

| Variable | Default | Purpose |
|---|---|---|
| `PORT` | `3000` (`3001` under `nerack test`) | HTTP listening port. |
| `SECRET__COOKIE_SECRET` (or `/run/secrets/cookie_secret`) | none | The secret `cookie_secret`; signs every cookie. Required in production by an app that uses `http.h`; generate one with `openssl rand -hex 32` and keep it stable, or every session ends. |
| `HTTP_REACTOR_COUNT` | `1` | HTTP reactor threads; one serves many thousands of connections, so raise it only for heavy HTTP traffic. |
| `WORKER_COUNT` | half the CPU cores, at least 2 | Threads for `run_worker()`, database calls, and other blocking work. |
| `SHUTDOWN_TIMEOUT` | `10` | Seconds a stopping process gives in-flight requests to finish. |
| `TRUSTED_PROXIES` | none | Addresses and CIDR ranges whose `X-Forwarded-For` is trusted. |
| `SSL_CERT_FILE` | system store | CA certificates for `https://` calls from `http_fetch()`; without it the host's store, else a bundle built into the binary (for `FROM scratch`). |
| `MAX_UPLOAD_BYTES` | 10 MB | Largest accepted upload. |
| `SESSION_DAYS`, `SESSION_MAX_DAYS` | 30, 90 | Session lifetime without use, and however active. |
| `OTEL_EXPORTER_OTLP_ENDPOINT`, other `OTEL_*` | `http://localhost:4318` | Where OpenTelemetry spans, logs, and errors go; `OTEL_SERVICE_NAME` defaults to the project folder's name. With no collector there, the app logs that and runs on. |

Add the app's own settings the same way, read as `{{env:name}}`, and its secrets (database URLs with passwords, API keys) as secret files or `SECRET__` variables, read as `{{secret:name}}`. Every `env:` and `secret:` name the app reads must be set, or the binary refuses to start, naming each missing one.

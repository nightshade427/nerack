# Deploying an image

`nerack docker` runs `nerack build`, then builds one image per architecture in `.nerack/arch`, `<project folder>:latest-<arch>` (for example `myapp:latest-x86_64`), with Docker (or Podman through its `docker` command, from the `podman-docker` package); `--arch arm64` (or `x86_64,arm64`) for one run. Each image is for one platform and lands in the local image store, ready to run with `docker compose up`, tag and push to a registry (`docker tag myapp:latest-x86_64 registry.example.com/myapp:1.4`), or deploy as a stack. Any machine builds either architecture's image, with nothing emulated.

## The project's files

It builds from three files at the project root. When one is missing, the run writes a starter and says so; from then on the file is the project's to change and commit, and `nerack docker` never rewrites it.

- `Dockerfile`: an Alpine image holding the app, with CA certificates and time zones, `/data` as its volume (`DATA_DIR=/data`), port 3000, and a non-root user, built once per architecture with `ARCH` set to it. Commands (`apk add`) run in its first stage, `FROM --platform=$BUILDPLATFORM`, on the building machine's own platform; the second stage only copies files in (`COPY --from=files ...`, and `output/app.${ARCH}` as `/app`), so nothing runs emulated. Keep it that way when changing it: add system packages to the first stage and copy what they install. The app brings its own libc, so any Linux base works.
- `.dockerignore`: lets only the `output/app.*` binaries and `output/THIRD_PARTY_LICENSES` into the build, so `.build/`, `.git/`, the databases, and `.env` never reach the image. Add a line for each file the `Dockerfile` copies in.
- `compose.yml`: the app's service, its `data` volume on `/data`, an optional `.env` for settings, and `cookie_secret` as a secret read from `COOKIE_SECRET` wherever Compose runs.

## Running it

```bash
nerack docker
COOKIE_SECRET=$(openssl rand -hex 32) docker compose up
```
On a server, the platform's secret store sets `COOKIE_SECRET`, and keeps it stable across deploys.

## Secrets and settings

Secrets reach the app as files in `/run/secrets/`, which Docker, Swarm, Compose, and Kubernetes mount: `/run/secrets/cookie_secret` becomes `COOKIE_SECRET`. Add one per secret in `compose.yml` (a `secrets:` entry and the service's `secrets:` list). Non-secret settings go in `environment:` or the `.env` file Compose reads; the app itself never reads `.env`.

## Growing the stack

Change `compose.yml` as the app grows:
- A Postgres service, with the app's connection URL as a secret (`database_url`, read as `{{env:database_url}}`), and `depends_on` so the app starts after it.
- A reverse proxy service (Caddy, Traefik) for HTTPS, with the app's port no longer published, and `TRUSTED_PROXIES` set to the proxy.
- Drop the `data` volume once nothing uses SQLite.

## Upgrading

Run `nerack docker` again, then `docker compose up -d` (or push a new tag and roll the platform's deployment). New migrations run when the container starts; durable events and tasks resume. Keep the previous tag to roll back the code; a rollback does not undo migrations.

## Podman

`nerack docker` runs `docker`, so with Podman install `podman-docker`, which provides that command; run the result with `podman compose up` or `podman run`. The starter `Dockerfile` names its base image in full (`docker.io/library/alpine:3.20`), so Podman needs no short-name setup; keep names full when changing it.

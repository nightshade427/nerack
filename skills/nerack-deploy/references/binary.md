# Deploying the binary

`output/app.x86_64` runs on any Linux x86_64 host (a VPS, a bare server, a VM), and `output/app.aarch64` on any arm64 one, with nothing installed: no Docker, no libraries, no runtime. Copy the one for the host's architecture (`uname -m` on the host prints it).

## First deploy

1. Build in the project root: `nerack build`.
2. Copy the binary to the host: `scp output/app.x86_64 deploy@host:/srv/myapp/app`.
3. Create the data folder in the app's working directory, owned by the user the app runs as: `/srv/myapp/data` (the app keeps its databases in `data/` under its working directory).
4. Put the settings in a root-readable file such as `/etc/myapp/env` (mode `600`), one `KEY=value` per line:
   ```
   SECRET__COOKIE_SECRET=<output of openssl rand -hex 32>
   PORT=3000
   TRUSTED_PROXIES=127.0.0.1
   SECRET__DATABASE_URL=postgres://myapp:...@db.internal:5432/myapp
   ```
   Only what the app uses: `SECRET__DATABASE_URL` stands for the app's own secrets, read as `{{secret:database_url}}`; plain settings are read as `{{env:name}}`.
5. Run it as a service (below), and put a reverse proxy in front for HTTPS.

## systemd

`/etc/systemd/system/myapp.service`:
```ini
[Unit]
Description=myapp
After=network-online.target
Wants=network-online.target

[Service]
User=myapp
Group=myapp
EnvironmentFile=/etc/myapp/env
WorkingDirectory=/srv/myapp
ExecStart=/srv/myapp/app
Restart=on-failure
NoNewPrivileges=true
ProtectSystem=strict
ReadWritePaths=/srv/myapp/data
PrivateTmp=true

[Install]
WantedBy=multi-user.target
```
Then `systemctl daemon-reload && systemctl enable --now myapp`. Its output goes to the journal (`journalctl -u myapp -f`); spans, logs, and errors go to the OpenTelemetry collector set by `OTEL_EXPORTER_OTLP_ENDPOINT`.

## Reverse proxy

Caddy obtains certificates by itself; `/etc/caddy/Caddyfile`:
```
myapp.example.com {
	reverse_proxy 127.0.0.1:3000
}
```
Keep the app's port closed to the outside (bind the proxy to it locally, or firewall it), and set `TRUSTED_PROXIES` to the proxy's address. Server-sent events (`.sse` resources) are long-lived requests: with Nginx, turn off `proxy_buffering` for them and raise `proxy_read_timeout`.

## A networked database

SQLite needs nothing: its files live in `data/` under the working directory (`WorkingDirectory=/srv/myapp` below, so `/srv/myapp/data`). For Postgres (or another networked engine), the app includes that engine's vendor module and reads its connection from a secret (`postgres("todos_db", "{{secret:database_url}}", {"create_todos_table"})`); the host only needs to reach the database server. Bundled modules' databases (`auth_db` and the others) move the same way, from an override such as `overrides/auth/module.c`.

## Upgrading

1. `nerack build`, then copy the new binary beside the old one and swap it in with a rename (`mv app.new app`), so a running process never sees a half-written file.
2. `systemctl restart myapp`. New migrations run at startup; durable events and tasks resume.
3. Keep the previous binary to roll back the code. A rollback does not undo migrations, so keep schema changes compatible with the previous build, or restore the data from backup.

## Backups

Back up `/srv/myapp/data` (or use the networked database's own backups). Copy SQLite files with the app stopped, or with `sqlite3 <file> ".backup <copy>"` while it runs.

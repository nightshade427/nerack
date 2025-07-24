---
name: nerack-cli
description: Running the nerack command line. Dev mode and its console, nerack info for the app's resources, events, routines, and databases, nerack build, setup, pin, versions, lsp, debug, and clean. Use when running, checking, inspecting, testing, or building a Nerack app, or setting a project up.
---

# Nerack: the command line

Run every command in the project root, the folder holding `app.c`. `nerack help` lists them.

| Command | What it does |
|---|---|
| `nerack` | Dev mode: compiles the app into `.build/`, serves it, and reloads on every save. Sets the project up first when it needs it. |
| `nerack info` | Lists what the app declares: resources, events, cron schedules, routines, databases, and overrides, each with its module. |
| `nerack build` | The production binary for each architecture in `.nerack/arch`, `output/app.<arch>`, plus a package for each exported module. `--arch arm64` (or `x86_64,arm64`) for one run. Fails on any compiler warning. |
| `nerack docker` | `nerack build`, then a Docker image per architecture, `<project folder>:latest-<arch>` (see the `nerack-deploy` skill). |
| `nerack setup` | Sets the project up on this machine. |
| `nerack pin` | Pins the project to the running version of Nerack. |
| `nerack versions` | Lists the installed versions. |
| `nerack remove <version>...` | Removes installed versions and their caches. |
| `nerack prune` | Removes every cache not in use. |
| `nerack clean` | Removes `.build/` and `output/`. |
| `nerack lsp` | A language server (clangd) for the project, on stdin and stdout. |
| `nerack debug` | Dev mode under the debugger (LLDB). |
| `nerack --version` | Prints the version, and the project's pin when it differs. |

## Dev mode

- `nerack` runs until stopped, so start it in the background and read its output. It listens on port 3000; change it with `--port` or `PORT`. It reads the project's `.env` (`KEY=value` lines) for local settings and secrets; a variable already set wins.
- Every start compiles the app fresh. Saving a file recompiles what changed and hot-reloads it; an asset change reloads only that asset.
- The console shows requests, errors, and the compiler's diagnostics. After each compile or reload it prints one status line: a file that failed to compile (the previous version keeps serving), the number of warnings, or the module reloaded. Read it after every change.
- Warnings do not stop dev mode, but `nerack build` fails on them, so fix every warning before moving on.
- Every name the app references (templates, databases, routines, resources, events) is checked when it loads. An unknown name stops that module from loading, with a message naming it.

## Checking what the app declares

`nerack info` compiles the app, checks every name, prints what it declares, and exits. It serves nothing, opens no database, and runs no migration. After adding or changing resources, events, schedules, routines, databases, or overrides, run it to confirm the app declares what you meant, and that no URL, name, or event is missing or duplicated.

- `nerack info --json` prints one object with a key per section (`resources`, `events`, `cron`, `routines`, `databases`, `overrides`), each an array, empty when the section is. Prefer it to reading the tables.
- `nerack info resources cron` prints only the named sections.
- A broken name fails it with the same message dev mode prints.

## Building

`nerack build` writes `output/app.<arch>` for each architecture the project's `.nerack/arch` lists (one per line, `x86_64` or `aarch64`; pin writes `x86_64`): one static Linux binary with every module and asset embedded, needing nothing else on the host. Either machine builds either architecture; `--arch` (comma-separated, `amd64` and `arm64` accepted) overrides the file for one run, such as a CI job per architecture. It is strict: any warning fails it, naming the file. A module folder holding `export.h` is also exported, for both architectures whatever the app builds for: `output/<name>.h`, `LICENSE.<name>`, and `<name>.<arch>.so` and `.a`, for another app's `vendor/`. Everything shipped lands in `output/`; everything intermediate in `.build/`.

## Installing

`nerack` is one Linux binary, x86_64 or aarch64 (`uname -m`), put on `PATH`; it needs nothing else. On a Mac (Apple silicon) it runs in an arm64 Linux machine (OrbStack: `orb create ubuntu nerack`, or Lima: `limactl start --name=nerack`), and on Windows in WSL2 (`wsl --install`). There, keep projects in the Linux home folder, never a folder shared from the host (`/mnt/c`, a macOS share): the file watcher misses edits made through it, and compiling is slow. The host's browser reaches `localhost:3000`; editors run inside Linux through their remote modes (VS Code WSL or Remote-SSH, JetBrains Gateway) so `nerack lsp` does too. `nerack docker` needs Docker: OrbStack's, Docker Desktop with WSL integration, or Docker inside the Linux machine.

## Setup and versions

- `.nerack/version` pins the project to one version of Nerack; commit it. In a project pinned to another version, `nerack` runs that version in its place when it is installed (every version copies itself to `~/.local/share/nerack/versions/` on its first run), and otherwise `nerack` and `nerack build` stop and name the one to get; `nerack versions` lists the installed ones. `nerack pin` moves the project to the running version, rewriting `.nerackignore`'s Nerack block too; commit the change.
- `nerack` sets the project up the first time it runs on a machine; `nerack setup` does it by hand. Setup writes only ignored files: `.nerack/local/` (this machine's setup) and Nerack's skills in `.claude/skills/` and `.agents/skills/`. In a project with no `.nerack/version` it pins first. Pin writes the committed files: a starter `app.c` when there is none, `.nerack/version`, `.nerack/arch` when there is none (the project's choice from then on; pin never rewrites it), `.nerackignore`'s block between `# >>> nerack` and `# <<< nerack` (what the scan skips by default; add the project's own lines below it, `!README.md` to bring one back), and `.gitignore` entries. A cloned, pinned project's committed files are never touched.
- Setup and `nerack pin` replace the `nerack` and `nerack-*` skill folders with the running version's. Never edit them; write a project's own skills in folders of other names beside them.

## Files never to edit

`.build/` (dev and production builds), `output/` (what `nerack build` ships), `.nerack/local/` (this machine's setup), and the `nerack*` folders in `.claude/skills/` and `.agents/skills/`. `nerack clean` removes `.build/` and `output/` safely at any time.

## Testing the app

There is no test runner: test over the app's protocols. Start it against a throwaway data folder, so every run begins with fresh databases (migrations and seeds applied), on a port of its own, then drive it with an HTTP tool (Hurl, curl) or a browser tool (Playwright):
```bash
DATA_DIR=$(mktemp -d) PORT=3001 nerack &
hurl --test tests/*.hurl
```
POST, PUT, PATCH, and DELETE from a tool that is not a browser need an `Origin` header matching the app (`Origin: http://localhost:3001`), or the CSRF check rejects them. Events and tasks run after the request, so poll for their effects rather than checking right after the response.

## Editor and debugger

- `nerack lsp` serves clangd for the project. Dev mode also writes `.build/dev/compile_commands.json`, and a `.clangd` (when there is none) pointing an editor's own clangd at it.
- `nerack debug` runs dev mode under LLDB, pipeline-aware: halt on a step, step through a pipeline, and inspect the full context, nested tables and records included.

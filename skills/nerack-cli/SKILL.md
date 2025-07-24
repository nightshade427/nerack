---
name: nerack-cli
description: Running the nerack command line. Dev mode and its console, nerack test with scenarios (mocks, seeds) and coverage, nerack info for the app's resources, events, cron schedules, tasks, routines, and databases, nerack build, setup, pin, versions, lsp, debug, and clean. Use when running, checking, inspecting, testing, or building a Nerack app, or setting a project up.
---

# Nerack: the command line

Run every command in the project root, the folder holding `app.c`. `nerack help` lists them.

| Command | What it does |
|---|---|
| `nerack` | Dev mode: compiles the app into `.build/`, serves it, and reloads on every save. Sets the project up first when it needs it. Takes only setting flags (`--port 4000`). |
| `nerack test [scenario]` | Dev mode for tests: fails on any compiler warning, opens fresh copies of the databases every run, reads `.env.test` over `.env`, and with a scenario puts its files (mocks, seeds, any asset) in place of the app's. `nerack test --report` checks the runs' coverage (see Testing the app). |
| `nerack info` | Lists what the app declares: resources, events with the keys they give, cron schedules, tasks, routines, databases, overrides, and replaced steps (steps that never run because their key is already set), each with its module. |
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

- `nerack` runs until stopped, so start it in the background and read its output. It listens on port 3000; change it with `--port` or `PORT`. It reads the project's `.env` (`KEY=value` lines) for local settings and secrets; a variable already set wins. SQLite databases live in `data/` at the root (gitignored); delete a file there to start that database over.
- Every start compiles the app fresh. Saving a file recompiles what changed and hot-reloads it; an asset change reloads only that asset.
- The console shows requests, errors, and the compiler's diagnostics. After each compile or reload it prints one status line: a file that failed to compile (the previous version keeps serving), the number of warnings, or the module reloaded. Read it after every change.
- Warnings do not stop dev mode, but `nerack build` fails on them, so fix every warning before moving on. A step whose key an earlier writer always sets never runs and is a warning too (`⚠ get_weather (module weather) never runs: weather_data is set by weather_data.json`); after a request, the console lists a step that had not run yet and was skipped because a conditional writer set its key first, beside unresolved names.
- Every name the app references (templates, databases, routines, tasks, resources, events) and every context key a template reads is checked when it loads. An unknown name stops that module from loading, with a message naming it. Names only the running app can check (inside sections over `http_fetch()`/`json()`/`run()` tables, after a `run()`, in `format()`) are checked as they render: after a request, the console lists the names it rendered with no value anywhere yet (`⚠ GET /profile: 1 unresolved name`), with where each is read. An `env:` or `secret:` name the app reads but nothing sets stops the start.

## Checking what the app declares

`nerack info` compiles the app, checks every name, prints what it declares, and exits. It serves nothing, opens no database, and runs no migration. After adding or changing resources, events, schedules, tasks, routines, databases, or overrides, run it to confirm the app declares what you meant, and that no URL, name, or event is missing or duplicated.

- `nerack info --json` prints one object with a key per section (`resources`, `events`, `cron`, `tasks`, `routines`, `databases`, `overrides`), each an array, empty when the section is. Prefer it to reading the tables.
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

There is no test runner: test over the app's protocols with an HTTP tool (Hurl, curl) or a browser tool (Playwright). Each test starts its own `nerack test`, on port 3001 (beside dev mode on 3000; set `PORT` or `--port` only when 3001 does not suit, such as parallel test servers): dev mode compiled as strictly as `nerack build` (a warning fails the start), where every database opened is a fresh copy beside the real one (`<name>_test_<random>`: migrations and seeds applied, dropped on stop; the dev databases are never touched), and settings come from `.env.test` over `.env` (commit `.env.test`: stand-in secrets for mocked steps, a test `PORT` if `.env` sets one):
```bash
nerack test & pid=$!
hurl --test tests/todos.hurl
kill $pid; wait $pid
```
POST, PUT, PATCH, and DELETE from a tool that is not a browser need an `Origin` header matching the app (`Origin: http://localhost:3001`), or the CSRF check rejects them. Events and tasks run after the request, so poll for their effects rather than checking right after the response.

**Scenarios.** `nerack test <scenario>` gathers every folder named `<scenario>` in a `tests/` folder: the root's `tests/` (the app module's part), each `modules/<name>/tests/` (that module's part), each `overrides/<name>/tests/`. A name no folder has fails. `tests/` is the one subfolder a module may have; it is never built, scanned with the module's assets, or shipped. A scenario folder's files of asset types are assets that replace the module's own of the same name and type for that run; anything else (hurl files, specs, notes) is the project's own:
- `<context_key>.json`, a mock: a `.json` asset, so the key is set before the module's first step, and every step of the module that sets it is skipped (`.map` and its code do not run). An array of objects is a table (numbers and booleans become text, `null` an empty value), a JSON string a string. A mock of a key a `sql_query()` sets must have the query's columns. Unmocked steps run for real against the test's databases.
- `<seed_key>.sql`, replacing that seed: seeds still apply in order with this file in its place, so `.seeds = {"admin_seed", "todos_seed"}` plus a scenario `todos_seed.sql` applies the real `admin_seed`, then the scenario's. An empty file leaves the seed out. `todos_seed.postgres.sql` replaces only the Postgres variant.
- any other asset (a template, query, partial) replaces the module's of that name.

Replaced steps are still fully checked (the keys they read, their assets, their `env:`/`secret:` names). A run fails, naming the file, on a scenario file that replaces nothing (no such asset in its module; for a `.json`, no step setting the key and no template reading it), a mock whose columns differ from its query's, a step that had not run yet and was skipped because a key was already set by something other than the scenario, or a conditional writer (`.if_context_key`/`.unless_context_key`) colliding with another at run time. Replacements are recorded in the run file, never printed. To write mocks, read the query's columns (its SQL and the migrations) or the API's response shape, and write the table the step would have produced.

**Coverage.** Names only the running app can check (inside sections over `http_fetch()`/`json()`/`run()` tables, SQL with sections, `.map`, after `run()`) are proven by rendering them with a value. Each run writes what it saw to `.build/test/` on stop; after the suite, `nerack test --report` merges every run of the same build, lists each name no run resolved ("rendered, never resolved" or "never rendered"; a step mocked in every run is no gap), exits non-zero if any, and deletes the records:
```bash
for scenario in search_todos empty_list; do
  nerack test $scenario & pid=$!
  hurl --test modules/*/tests/$scenario/*.hurl
  kill $pid; wait $pid
done
nerack test --report
```

## Editor and debugger

- `nerack lsp` serves clangd for the project. Dev mode also writes `.build/dev/compile_commands.json`, and a `.clangd` (when there is none) pointing an editor's own clangd at it.
- `nerack debug` runs dev mode under LLDB, pipeline-aware: halt on a step, step through a pipeline, and inspect the full context, nested tables and records included.

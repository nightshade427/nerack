![Nerack C](./images/nerack_scene.png)

## Why Nerack

Nerack is a declarative, protocol-agnostic framework for network applications in C, built around self-contained systems. You write steps, and it runs as fast, non-blocking C with no async code or locks to manage. It ships as a single binary with everything bundled.

* **Self-contained systems:** each module owns everything for its slice of the app: databases, schemas, resources, templates, routines, tasks, and events. Modules communicate only through events and links to each other's pages, and the compiler enforces the boundaries.
* **Developer experience:** low boilerplate, no manual memory or concurrency management, hot reloading, and mistakes caught before the app starts.
* **Durable tasks and events:** tasks resume after a crash, and events are never lost.
* **Performance:** C speed with no async code to write: steps read like plain code but never block.
* **Observability:** every step emits OpenTelemetry spans, logs, and errors, with no instrumentation code.
* **Introspection:** see the whole app at a glance, and exactly what you changed in bundled modules.
* **Any protocol:** HTTP, RNS, NomadNet, Gemini, Finger, SMTP, and more as modules, several in one app. A pipeline can cross protocols: for example a message arriving over RNS can be pushed in real time over SSE to HTTP clients.
* **Realtime hypermedia:** HTML or Micron over the wire with SSE, Datastar, and htmx, even on protocols like RNS that lack them natively.
* **Any database:** SQLite, Postgres, MySQL, Redis, DuckDB, and more as modules, several in one app, with multi-tenancy built in. A pipeline can mix databases and engines: a shared Postgres database for authentication alongside a per-user SQLite database.
* **Authentication:** cookie sessions over HTTP and identity-based sign-in over RNS are bundled and share the same users; WebAuthn, OTP, SSO, or any other scheme can be added as a module.
* **Bundled modules:** HTTP, RNS, SQLite; HTML, Markdown, and Micron templates; Datastar, htmx, Tailwind, DaisyUI; session auth for HTTP and RNS; pub/sub; background and cron tasks.
* **Develop anywhere:** Linux, Mac, or Windows, on Intel/AMD or Arm, and deploy to x86 or Arm Linux servers such as AWS Graviton.

---

## Table of Contents
* [Quick Start](#quick-start)
* [Philosophy](#philosophy)
* [Tooling](#tooling)
  * [Supported Machines](#supported-machines)
  * [Setup](#setup)
  * [Development Environment](#development-environment)
  * [Introspection](#introspection)
  * [Coding Agents](#coding-agents)
  * [Debugging](#debugging)
  * [Testing](#testing)
  * [Deployment](#deployment)
  * [Observability](#observability)
* [Built With](#built-with)
* [Meet Nerack](#meet-nerack)
* [License](#license)
* **Documentation**
  * [Guide](./GUIDE.md): build a todo app one concept at a time
  * [Reference](./REFERENCE.md): full options for every step, helper, and field

---

## Quick Start

Download the binary, make a folder for the project (the project root), and run `nerack` in it.

```bash
mkdir myapp && cd myapp
nerack
```

The first run sets the project up (see [Setup](#setup)) and writes this starter `app.c`. Every run starts the dev server on port 3000 with file watching, auto compilation, hot code reloading, and HMR; open http://localhost:3000. Edit `app.c` in any editor (see [Development Environment](#development-environment)) and Nerack picks up every save.

```c
#include <nerack.h>
#include <http.h>

module(){
  http("home", "/", .mime = mime_text,
    .get = {
      context("hello", "Hello, world"),
      http_response("hello")
    }
  );
}
```

`http()` declares the `home` resource mapping `/` to a GET pipeline: `context()` renders its template into the context key `hello`, and `http_response()` sends it as `text/plain`, set by `.mime`. `http()` comes from `http.h`; the core itself has no notion of HTTP. See the [Guide](./GUIDE.md) for a step-by-step walkthrough.

---

## Philosophy

An application is a data transformation: input arrives, is transformed, and leaves as output. Nerack arranges the work into pipelines: ordered lists of steps that turn a request into a response.

### Steps

Steps are the primitive. A step is declarative: it describes what should happen, not how. Each step is self-contained: the next step starts only once it resolves. A step guarantees that when it finishes, context holds the values it promised; if it cannot, it raises an error, which an error or repair pipeline handles. Each step is its own OpenTelemetry span and logs itself.

### The DSL is C

Apps are written in a declarative DSL that is C: modules, resources, and pipelines are C code, checked by the C compiler and edited and debugged with C tools.

### Built on Standards

The assets are standard formats: SQL, JSON, Markdown, HTML, CSS, JS, Micron. Business logic is plain C. The tooling is standard too: LLDB for debugging, clangd for editing, OpenTelemetry for observability.

### Everything is a String

The protocols and formats Nerack speaks are text: HTTP, RNS, Micron, HTML, Markdown, JSON, SQL. The pipeline context stores and passes data as strings. Context values are interpolated into SQL, templates, and URLs with `{{context_key}}`.

### CLAD

Four principles:

* **(C)omposable:** small, independent steps chain into feature pipelines.
* **(L)ocality of Behavior:** behavior is apparent from reading the code. SQL, templates, and logic for a feature live together, not spread across model, view, and controller trees.
* **(A)utonomous:** each module owns its schemas, migrations, seeds, resources, UI, and logic, and the compiler enforces the boundaries.
* **(D)omain Based:** each module owns one slice of the app. A `todos` module defines everything related to todos and nothing else.

Inspired by:

* [Data Oriented Design](https://youtu.be/rX0ItVEVjHc)
* [A Philosophy of Software Design](https://youtu.be/bmSAYlu0NcY)
* [CUPID](https://youtu.be/cyZDLjLuQ9g)
* [Self-Contained Systems](https://youtu.be/Jjrencq8sUQ)
* [Locality of Behavior](https://htmx.org/essays/locality-of-behaviour)

---

## Tooling

### Supported Machines

Develop on Linux, a Mac, or Windows, on Intel/AMD or Arm, and deploy to Linux servers of either kind, including Arm servers such as AWS Graviton. On a Mac or Windows, Nerack runs in a lightweight Linux environment (OrbStack or Lima on a Mac, WSL on Windows), so it behaves the same everywhere, and nothing is emulated.

### Setup

`nerack` sets a project up the first time it runs, so a new project and a fresh clone both start with one command:
```bash
git clone https://example.com/myapp.git && cd myapp
nerack
```

A new project is pinned to the version of Nerack that created it. Commit the files it writes, and everyone on the project uses the same version; `nerack pin` moves the project to a newer one.
```bash
nerack pin
```

`nerack help` lists every command.

### Development Environment

Running `nerack` starts dev mode in the current directory, the project root. `app.c`, `modules/`, and assets (SQL, HTML, and so on) live there. On every start Nerack compiles the app fresh, then watches for changes. Requests and errors print to the console. The dev server listens on port 3000.
```bash
nerack
```

Edit `.c` files and assets with any editor; saving a file recompiles what changed and hot-reloads it. Warnings show in the console and on the page without stopping the app, and a file that fails to compile leaves the previous version running. `nerack lsp` runs a language server configured for the project; point your editor's LSP client at it.
```bash
nerack lsp
```

An editor that already runs its own clangd works too, with no setup.

### Introspection

`nerack info` lists everything the app declares, by module. Name sections to see only those (`nerack info resources`), or add `--json` for scripts and coding agents.
```bash
nerack info
```
```
nerack info · todo_app

Resources
+----------+------------+----------+--------------+------------------------+
| PROTOCOL | URL        | NAME     | METHODS      | MODULE                 |
+----------+------------+----------+--------------+------------------------+
| http     | /          | home     | GET          | app                    |
|          | /account   | account  | GET POST     | cookie_auth (bundled)  |
|          | /activity  | activity | GET          | activity               |
|          | /login     | login    | GET POST     | cookie_auth (override) |
|          | /logout    | logout   | POST         | cookie_auth (bundled)  |
|          | /signup    | signup   | GET POST     | cookie_auth (bundled)  |
|          | /todos     | todos    | GET POST     | todos                  |
|          | /todos/:id | todo     | PATCH DELETE | todos                  |
+----------+------------+----------+--------------+------------------------+
| rns      | /          | home     | GET          | app                    |
|          | /login     | login    | GET POST     | rns_auth (bundled)     |
|          | /todos     | todos    | GET POST     | todos                  |
+----------+------------+----------+--------------+------------------------+

Events
+---------------+-----------+----------------+---------------------+
| EVENT         | PUBLISHER | GIVES          | SUBSCRIBERS         |
+---------------+-----------+----------------+---------------------+
| todo_created  | todos     | user_id, title | activity, reminders |
| todo_finished | todos     | user_id, title | activity            |
+---------------+-----------+----------------+---------------------+

Cron
+--------------+--------------------+-----------+----------------+-----------+
| SCHEDULE     | WHEN               | NEXT RUN  | NAME           | MODULE    |
+--------------+--------------------+-----------+----------------+-----------+
| */15 * * * * | every 15 minutes   | in 9m     | send_reminders | reminders |
| 0 8 * * *    | every day at 08:00 | in 6h 12m | daily_digest   | reminders |
+--------------+--------------------+-----------+----------------+-----------+

Tasks
+-----------------+---------------+-------------------+
| MODULE          | TASK          | ACCEPTS           |
+-----------------+---------------+-------------------+
| reminders       | send_reminder | user_id, email    |
+-----------------+---------------+-------------------+
| mailer (vendor) | send_email    | to, subject, body |
+-----------------+---------------+-------------------+

Routines
+-----------+---------------+
| MODULE    | ROUTINE       |
+-----------+---------------+
| reminders | due_reminders |
| todos     | count_todos   |
+-----------+---------------+

Databases
+-----------------------+----------------+----------+----------------------------+
| MODULE                | DATABASE       | ENGINE   | DECLARED IN                |
+-----------------------+----------------+----------+----------------------------+
| activity              | activity_db    | sqlite   | modules/activity/module.c  |
| reminders             | reminders_db   | sqlite   | modules/reminders/module.c |
| todos                 | todos_db       | sqlite   | modules/todos/module.c     |
+-----------------------+----------------+----------+----------------------------+
| auth (override)       | auth_db        | postgres | overrides/auth/module.c    |
| cookie_auth (bundled) | cookie_auth_db | sqlite   | bundled                    |
| pubsub (bundled)      | pubsub_db      | sqlite   | bundled                    |
| rns_auth (bundled)    | rns_auth_db    | sqlite   | bundled                    |
| task (bundled)        | task_db        | sqlite   | bundled                    |
+-----------------------+----------------+----------+----------------------------+

Overrides
+-------------+----------+------------------------+----------+------------------------+
| MODULE      | TYPE     | NAME                   | CHANGE   | FILE                   |
+-------------+----------+------------------------+----------+------------------------+
| auth        | database | auth_db                | replaced | module.c               |
|             | asset    | create_users_table.sql | replaced | create_users_table.sql |
+-------------+----------+------------------------+----------+------------------------+
| cookie_auth | resource | http login             | replaced | module.c               |
|             | asset    | login.html             | replaced | login.html             |
|             | asset    | welcome.html           | added    | module.c               |
+-------------+----------+------------------------+----------+------------------------+

11 resources · 2 events · 2 schedules · 2 tasks · 2 routines · 8 databases · 5 overrides
```

### Coding Agents

Setup installs Nerack's skills for Claude Code and other coding agents: `nerack` for writing apps, `nerack-cli` for running, checking, and building them, and `nerack-deploy` for shipping them as a binary or a Docker image. An agent loads a skill only when the task calls for it, so the reference costs nothing in a session that does not need it, and setup never touches your agent's instructions file.

### Debugging

The interactive debugger is pipeline-aware: halt on a step, step through execution, and inspect the full context, including nested tables and records.
```bash
nerack debug
```

### Testing

Test an app from the outside, over its protocols, with any HTTP or browser testing tool, such as [Hurl](https://hurl.dev) for requests and responses or [Playwright](https://playwright.dev) for real-browser flows. `nerack test` runs the app for tests: as strict as `nerack build`, on fresh copies of the databases every run (migrations and seeds applied, dropped when it stops), with `.env.test` over `.env`, and on port 3001, beside dev mode on 3000. `nerack test <scenario>` also puts the scenario's files in place of the app's: JSON mocks standing in for what steps would put in context, seeds, or any other asset, so a test needs no outside service or hand-made data.

```hurl
# tests/todos.hurl
POST http://localhost:3001/todos
Origin: http://localhost:3001
[FormParams]
title: milk
HTTP 302

GET http://localhost:3001/todos
HTTP 200
[Asserts]
body contains "milk"
```
```bash
nerack test & pid=$!
hurl --test tests/todos.hurl
kill $pid; wait $pid
```

### Deployment

`nerack` always runs in dev mode. For production, `nerack build` compiles the app into `output/app.x86_64`, one fully optimized Linux binary with every module and asset embedded. The production build is strict: a warning that dev mode lets through fails it, naming the file.
```bash
nerack build
```

The binary needs nothing else on the server, not even Docker: copy it there and run it. It is built for x86_64 servers to start with, and can be built for Arm servers such as AWS Graviton too, from any machine.
```bash
./app.x86_64
```

To ship an image instead, `nerack docker` builds the same binary into a Docker image named after the project, ready to run, push to a registry, or deploy. The first run writes a starter `Dockerfile` and `compose.yml` for you to commit and change.
```bash
nerack docker
docker compose up
```

Nerack does not terminate TLS; place it behind a reverse proxy or load balancer (Nginx, Caddy, AWS ALB) for HTTPS. Run one process per host.

To scale, run the same binary on machines of different sizes and send each its share of routes, such as image processing to larger machines, with no module split out into a service of its own.

### Observability

Each pipeline step emits OpenTelemetry spans, logs, and errors over OTLP, with no manual instrumentation. By default they go to a collector on localhost at the standard port, so running SigNoz, Jaeger, or any other OpenTelemetry backend locally is enough; if none is found, Nerack logs that and continues.

---

## Built With

| | |
|---|---|
| [C](https://en.cppreference.com/w/c) | Language standard |
| [musl](https://musl.libc.org/) | C library, linked statically so the app is one binary that runs on any Linux |
| [mimalloc](https://github.com/microsoft/mimalloc) | Memory allocator for the whole process |
| [libuv](https://libuv.org/) | Event loops, async I/O, file watching, shared thread pool |
| [h2o](https://h2o.examp1e.net/) | HTTP server |
| [Mustach](https://gitlab.com/jobol/mustach) | Templates and `{{key}}` interpolation |
| [md4c](https://github.com/mity/md4c) | Markdown rendering |
| [Jansson](https://github.com/akheron/jansson) | JSON parsing and generation |
| [curl](https://curl.se/) | HTTP client for calling APIs |
| [OpenSSL](https://www.openssl.org/) | TLS for HTTPS calls |
| [Argon2](https://github.com/P-H-C/phc-winner-argon2) | Password hashing |
| [SQLite](https://sqlite.org/) | Embedded database |
| [microReticulum](https://github.com/attermann/microReticulum) | Reticulum mesh networking stack |
| [clang](https://clang.llvm.org/) | Compiler, built into `nerack` |
| [lld](https://lld.llvm.org/) | Linker, built into `nerack` |
| [clangd](https://clangd.llvm.org/) | Language server |
| [LLDB](https://lldb.llvm.org/) | Debugger |
| [OpenTelemetry](https://opentelemetry.io/) | Traces, logs, errors |

---

## Meet Nerack

Nerack is also the name of the mascot.

<p>
  <img src="./images/nerack.png" alt="Nerack, a round glowing mascot" width="200">
  <img src="./images/nerack_c.png" alt="Nerack holding a glowing C" width="250">
</p>

---

## License

Nerack is released under the [MIT License](./LICENSE). The Nerack mascot artwork is licensed under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).

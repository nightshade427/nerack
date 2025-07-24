![Nerack C](./images/nerack_scene.png)

## Why Nerack

Nerack is a declarative, protocol-agnostic framework for asynchronous networking applications in C, built around self-contained systems.

* **Self-contained systems:** an app is a set of self-contained modules whose boundaries the compiler enforces.
* **Durable tasks and events:** both are persisted. After a crash, incomplete tasks resume at the step where they stopped and undelivered events replay.
* **Developer experience:** compilation, hot code reloading, and an HMR feedback loop, with no build scripts. Write code and save. The only dependency is Docker.
* **Memory, concurrency, and I/O managed by the framework:** application code does not call malloc/free or manage threads, mutexes, or locks.
* **Observability:** pipeline steps emit OpenTelemetry spans, logs, and errors automatically.
* **Protocols:** HTTP, RNS, LXMF, NomadNet, Gemini, Finger, SMTP, or any other; one app can serve several at once.
* **Databases:** SQLite, Postgres, MySQL, Redis, DuckDB, or any other; multi-tenancy is built in, and one app can use several at once.
* **Authentication:** cookie and session auth are bundled; WebAuthn, OTP, and other schemes are example modules you can add.
* **Bundled modules:** HTML, Markdown, and Micron templates; Datastar, HTMX, Tailwind, DaisyUI; HTTP, RNS, SQLite; pub/sub; background and cron tasks.

---

## Table of Contents
* [Quick Start](#quick-start)
* [Philosophy](#philosophy)
* [Tooling](#tooling)
* [Built With](#built-with)
* [License](#license)
* **Documentation**
  * [Guide](./GUIDE.md): build a todo app one concept at a time
  * [Reference](./REFERENCE.md): full options for every step, helper, and field

---

## Quick Start

Everything runs in Docker.

```bash
mkdir myapp && cd myapp
wget https://docker.nightshadecoder.dev/nerack/compose.yml

# Dev server on :3000, telemetry on :4000
# Includes file watching, auto compilation, hot code reloading, HMR
docker compose up
```

Create `hello_world.c` with the example below. Nerack watches for changes and hot-reloads on save. Use your own editor, or attach to the built-in TUI with `docker compose attach nerack` for an integrated editor, lsp, and console.

```c
#include <nerack.h>
#include <http.h>

module(hello_world){
  context("hello", "Hello, world");
  http("home", "/", .mime = mime_text, .get = {http_response("hello")});
}
```

A module is any `.c` file declaring `module(name){...}`; `hello_world.c` declares the `hello_world` module. `context()` seeds the context key `hello` from a string literal instead of a file; `http()` declares the `home` resource mapping `/` to a GET pipeline that sends that value with `http_response()`, as `text/plain` because of `.mime`. `http()` comes from `http.h`; the core itself has no notion of HTTP. See the [Guide](./GUIDE.md) for a step-by-step walkthrough.

---

## Philosophy

An application is a data transformation: input arrives, is transformed, and leaves as output. Nerack arranges the work into pipelines: ordered lists of steps that turn a request into a response.

The assets are standard formats: SQL, JSON, Markdown, HTML, CSS, JS, Micron. Business logic is plain C. The tooling is standard too: LLDB for debugging, Playwright and Criterion for testing, OpenTelemetry for observability.

### Everything is a String

The web is text: HTTP, RNS, Micron, HTML, Markdown, JSON, SQL. The pipeline context stores and passes data as strings. Strings are interpolated into SQL, templates, and URLs with `{{context_key}}`.

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

### Development Environment
Built-in TUI editor with HMR and LSP support.

### Introspection
`/app_info` is a built-in resource in dev builds. Query it like any other endpoint:
```bash
curl localhost:3000/app_info            # view topology
curl localhost:3000/app_info/resources  # list all resources
curl localhost:3000/app_info/pipelines  # inspect pipelines
curl localhost:3000/app_info/events     # view pub/sub map
curl localhost:3000/app_info/databases  # inspect schemas
```
Production builds omit it; see [Deployment](#deployment).

### Testing
Built-in runners for unit and end-to-end testing; no external framework setup required.
```bash
unit_tests # fast, criterion-based tests
e2e_tests # playwright-powered browser tests
```

### Debugging
The debug commands are pipeline-aware: halt on a step, step through execution, and inspect the full context, including nested tables and records.
```bash
app_debug # interactive debugger in the TUI
```

### Deployment
Nerack deploys as a standard Docker container. It does not terminate TLS; production deployments place Nerack behind a reverse proxy or load balancer (Nginx, Caddy, AWS ALB) to handle HTTPS.
```bash
app_build # outputs a minimal production Docker image
```

`app_build` runs each module's asset scan once (see [Assets](./REFERENCE.md#assets)) and compiles the results into that module's binary. The production image excludes the file watcher and `/app_info`.

### Observability
Each pipeline step emits OpenTelemetry spans. Logs, traces, errors, and auto-profiling are visualized on the telemetry server at port 4000. No manual instrumentation required.

## Built With

| | |
|---|---|
| [C](https://en.cppreference.com/w/c/23) | Language standard |
| [Docker](https://www.docker.com/) | Development environment, production images, stack orchestration |
| [libuv](https://libuv.org/) | Event loops, async I/O, file watching, shared thread pool |
| [libmicrohttpd](https://www.gnu.org/software/libmicrohttpd/) | HTTP server, used by the `http` protocol module |
| [Mustach](https://gitlab.com/jobol/mustach) | Templating and string interpolation, used by `html()`/`markdown()` |
| [Jansson](https://github.com/akheron/jansson) | JSON parsing and generation, used by `json()` and `http_fetch()` |
| [curl](https://curl.se/) | HTTP client, used by `http_fetch()` |
| [Reticulum](https://reticulum.network/) | Mesh networking stack, used by the `rns` protocol module |
| [LXMF](https://github.com/markqvist/LXMF) | Message transfer protocol over Reticulum, used by the `lxmf` protocol module |
| [Fresh](https://getfresh.dev/) | TUI editor |
| [clangd](https://clangd.llvm.org/) | Language server |
| [LLDB](https://lldb.llvm.org/) | Debugger |
| [Criterion](https://github.com/Snaipe/Criterion) | Unit testing |
| [Playwright](https://playwright.dev/) | End-to-end testing |
| [SigNoz](https://signoz.io/) + [OpenTelemetry](https://opentelemetry.io/) | APM, traces, logs, errors, dashboards |

---

## License

Nerack is licensed under the [LGPL](./LICENSE). Your application code can use any license; it is a Nerack plugin.

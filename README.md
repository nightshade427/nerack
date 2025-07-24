![Nerack C](./images/nerack_scene.png)

## Why Nerack

Nerack is a declarative, protocol-agnostic framework for asynchronous network applications in C, built around self-contained systems. It ships as a single Linux binary with everything bundled.

* **Self-contained systems:** each module owns everything for its slice of the app: databases, schemas, resources, templates, routines, and events. Modules communicate only through events and links to each other's pages, and the compiler enforces the boundaries.
* **Developer experience:** low boilerplate, no manual memory or concurrency management, and hot reloading on every save.
* **Durable tasks and events:** both are persisted, so after a crash, incomplete tasks resume at the step where they stopped, and undelivered events are replayed.
* **Performance:** tens of thousands of concurrent connections per core on a reactor architecture, with flat memory per connection and no context switching, so long-lived connections like SSE are cheap.
* **Observability:** every step emits OpenTelemetry spans, logs, and errors, with no instrumentation code.
* **Any protocol:** HTTP, RNS, NomadNet, Gemini, Finger, SMTP, and more as modules, several in one app. A pipeline can cross protocols: a message arriving over RNS can be pushed in real time over SSE to HTTP clients in the same chat room.
* **Realtime hypermedia:** HTML or Micron over the wire with SSE, CQRS, Datastar, and htmx, even on protocols like RNS that lack them natively. JSON works too, so Nerack can also serve as a backend-for-frontend (BFF) for SPAs.
* **Any database:** SQLite, Postgres, MySQL, Redis, DuckDB, and more as modules, several in one app, with multi-tenancy built in. A pipeline can mix databases and engines: a shared Postgres database for authentication alongside a per-user SQLite database.
* **Authentication:** cookie sessions over HTTP and identity-based sign-in over RNS are bundled and share the same users; WebAuthn, OTP, SSO, or any other scheme can be added as a module.
* **Bundled modules:** HTTP, RNS, SQLite; HTML, Markdown, and Micron templates; Datastar, htmx, Tailwind, DaisyUI; pub/sub; background and cron tasks.

---

## Table of Contents
* [Quick Start](#quick-start)
* [Philosophy](#philosophy)
* [Tooling](#tooling)
  * [Development Environment](#development-environment)
  * [Coding Agents](#coding-agents)
  * [Debugging](#debugging)
  * [Testing](#testing)
  * [Deployment](#deployment)
  * [Observability](#observability)
* [Built With](#built-with)
* [License](#license)
* **Documentation**
  * [Guide](./GUIDE.md): build a todo app one concept at a time
  * [Reference](./REFERENCE.md): full options for every step, helper, and field

---

## Quick Start

Download the binary and run `nerack` in an empty directory; that directory is the project root. It starts the dev server on port 3000 with file watching, auto compilation, hot code reloading, and HMR.

```bash
mkdir myapp && cd myapp
nerack
```

Create `app.c` with the example below, in any editor (see [Development Environment](#development-environment)). Nerack picks it up on save.

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

Every app starts with `app.c`, which declares the app module, `app`, with `module(){...}`; more modules and shared code go in folders of their own (see [Project Layout](./REFERENCE.md#project-layout)). `http()` declares the `home` resource mapping `/` to a GET pipeline: `context()` renders its template into the context key `hello`, and `http_response()` sends it as `text/plain`, set by `.mime`. `http()` comes from `http.h`; the core itself has no notion of HTTP. See the [Guide](./GUIDE.md) for a step-by-step walkthrough.

---

## Philosophy

An application is a data transformation: input arrives, is transformed, and leaves as output. Nerack arranges the work into pipelines: ordered lists of steps that turn a request into a response.

### Steps

Steps are the primitive. A step is declarative: it describes what should happen, not how. Each step is self-contained: it may run synchronously or asynchronously, but the pipeline treats it as a single synchronization point, and the next step starts only once it resolves. A step guarantees that when it finishes, context holds the values it promised; if it cannot, it raises an error, which an error or repair pipeline handles. Each step is its own OpenTelemetry span and logs itself.

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

### Development Environment

Running `nerack` starts dev mode in the current directory, the project root. `app.c`, module folders, and assets (SQL, HTML, and so on) live there, laid out as shown in [Project Layout](./REFERENCE.md#project-layout). On every start Nerack compiles the app fresh into `.build/`, a hidden folder for everything it builds, then watches for changes. Requests and errors print to the console. The dev server listens on port 3000; change it with `--port` or `PORT` (see [Configuration](./REFERENCE.md#configuration) for every setting and its default).
```bash
nerack
```

Edit `.c` files and assets with any editor; saving a file recompiles what changed and hot-reloads it. Dev mode lets warnings through, so half-finished code still runs, but makes them hard to miss: after every compile the console ends with a status line (`⚠ 3 warnings in 2 files; nerack build will fail until they are fixed`), and HTML pages show a small warnings badge. A file that fails to compile keeps the previous version running: the console says so after the error, and pages show the error in an overlay until the file compiles again. `nerack lsp` runs a language server configured for the project; point your editor's LSP client at it.
```bash
nerack lsp
```

An editor that already runs its own clangd works too. Each dev start writes `.build/dev/compile_commands.json` with every file's compile flags and, if the project has no `.clangd` file, one that points clangd at it.

### Coding Agents

Every `nerack`, `nerack build`, and `nerack lsp` writes `.build/ai/llms-full.txt`: the condensed reference for writing Nerack apps, matching the installed version of Nerack, so it is refreshed after every upgrade. Coding agents do not look in hidden folders on their own, so point yours at it from its instructions file (`AGENTS.md`, `CLAUDE.md`, `GEMINI.md`):
```markdown
Before writing Nerack code, read .build/ai/llms-full.txt (run `nerack` once if it is missing).
```

### Debugging

The interactive debugger is pipeline-aware: halt on a step, step through execution, and inspect the full context, including nested tables and records.
```bash
nerack debug
```

### Testing

Test an app from the outside, over its protocols. Start it against a throwaway data directory, so every run begins with fresh databases (migrations and seeds applied), then drive it with any HTTP or browser testing tool: [Hurl](https://hurl.dev) for request and response checks, [Playwright](https://playwright.dev) for real-browser flows such as Datastar or htmx interactions.

#### Hurl

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
DATA_DIR=$(mktemp -d) PORT=3001 nerack &
hurl --test tests/*.hurl
```

#### Playwright

```js
// playwright.config.js
import { defineConfig } from '@playwright/test';

export default defineConfig({
  testDir: 'tests',
  use: { baseURL: 'http://localhost:3001' },
  webServer: {
    command: 'DATA_DIR=$(mktemp -d) PORT=3001 nerack',
    url: 'http://localhost:3001',
  },
});
```
```js
// tests/todos.spec.js
import { test, expect } from '@playwright/test';

test('adding a todo shows it without a reload', async ({ page }) => {
  await page.goto('/todos');
  await page.getByPlaceholder('new todo').fill('milk');
  await page.getByRole('button', { name: 'add' }).click();
  await expect(page.locator('#todos')).toContainText('milk');
});
```
```bash
npx playwright test
```

### Deployment

`nerack` always runs in dev mode. For production, `nerack build` compiles the app into `output/app`, a single, fully optimized Linux binary with all of its modules and assets embedded. The production build is strict: a warning that dev mode lets through fails it, naming the file. It carries no compiler, file watcher, or language server. Everything Nerack produces for shipping goes in `output/`, and every project names its files the same way, so scripts and CI work unchanged across projects.
```bash
nerack build
```

`nerack docker` packages the same build as a Docker image archive, `output/app.oci.tar`, along with a `compose.yml` for it. The image is tagged with the project folder's name (`myapp:latest`), so images of different projects never collide; load it with `docker load -i output/app.oci.tar`. It sets `DATA_DIR=/data` and declares it a volume.
```bash
nerack docker
```
```yaml
services:
  myapp:
    image: myapp:latest
    ports: ["3000:3000"]
    env_file: .env       # COOKIE_SECRET=..., app settings
    volumes: ["data:/data"]
volumes:
  data:
```

A module whose folder holds an `export.h` is also exported: `nerack build` writes it into `output/` as a package (`.so`, `.a`, and `.h`) that another Nerack app drops into its `vendor/`, so apps share modules without sharing source (see [Exporting](./REFERENCE.md#exporting)).

`nerack clean` removes `.build/` and `output/`.
```bash
nerack clean
```

Nerack does not terminate TLS; place it behind a reverse proxy or load balancer (Nginx, Caddy, AWS ALB) for HTTPS.

One process uses the whole machine, so run one per host; tune its threads with `HTTP_REACTOR_COUNT` and `WORKER_COUNT`. Every setting and its default is listed in [Configuration](./REFERENCE.md#configuration).

### Observability

Each pipeline step emits OpenTelemetry spans, logs, and errors over OTLP, with no manual instrumentation. By default they go to a collector on localhost at the standard port, so running SigNoz, Jaeger, or any other OpenTelemetry backend locally is enough; if none is found, Nerack logs that and continues. Point it elsewhere with `--otel-endpoint` or the standard `OTEL_*` environment variables (see [Configuration](./REFERENCE.md#configuration)).

---

## Built With

| | |
|---|---|
| [C](https://en.cppreference.com/w/c) | Language standard |
| [libuv](https://libuv.org/) | Event loops, async I/O, file watching, shared thread pool |
| [h2o](https://h2o.examp1e.net/) | HTTP server, used by the `http` protocol module |
| [Mustach](https://gitlab.com/jobol/mustach) | Templating and string interpolation, used by every template step and SQL asset |
| [md4c](https://github.com/mity/md4c) | Markdown rendering, used by `markdown()` |
| [Jansson](https://github.com/akheron/jansson) | JSON parsing and generation, used by `json()` and `http_fetch()` |
| [curl](https://curl.se/) | HTTP client, used by `http_fetch()` |
| [SQLite](https://sqlite.org/) | Embedded database, used by the `sqlite` module |
| [microReticulum](https://github.com/attermann/microReticulum) | Reticulum mesh networking stack, used by the `rns` protocol module |
| [clangd](https://clangd.llvm.org/) | Language server |
| [LLDB](https://lldb.llvm.org/) | Debugger |
| [OpenTelemetry](https://opentelemetry.io/) | Traces, logs, errors |

---

## License

Nerack is released under the [MIT License](./LICENSE).

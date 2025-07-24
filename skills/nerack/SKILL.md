---
name: nerack
description: Writing Nerack apps in C. app.c and modules/ folders, http() and rns() resources, pipelines of steps, context and {{key}} interpolation, SQL and Mustache assets, events, routines, cron, and authentication. Use when creating or changing code or assets (.c, .h, .sql, .html, .md, .mu, .txt) in a Nerack project.
---

# Nerack: writing apps

Nerack is a declarative C framework for network apps. An app is a set of modules; each declares resources, databases, routines, and events whose pipelines (ordered lists of steps) turn a request into a response. Read Style, Syntax, and Gotchas before writing code, and use Signatures for exact field names.

## Mental model

- `app.c` at the project root is the app module (`app`). Every other module is a flat folder `modules/<name>/` with a `module.c`; the folder names it. `module(){...}` takes no name and runs once at boot to register declarations. A module body holds `http()`/`rns()` resources, `<engine>()` databases, `routine()`, `publish()`/`subscribe()`, `error()`/`repair()`, and `*_template()` assets. There is no module-level middleware: shared steps go in a resource's `.all`.
- A request runs the resource's `.all`, then the pipeline for its verb. Each step finishes before the next starts, or raises an error. A pipeline with no response step returns an empty success (HTTP 200).
- Context is a key/value store for one pipeline run (a request, routine, or event delivery). Scopes: bare keys (app scope: values the app controls, including its own signed cookies), `input:x` (raw parameters), `client:x` (request headers, lowercase, `-` as `_`), `server:client_ip|method|path|url`, `env:x` (environment variables, lowercased), `error:x`. `validate()` checks a key and promotes it unprefixed (`input:title` → `title`). Never authorize on `input:` or `client:` values.
- A value is a string, a table of records (query and fetch results), a blob (`{data, size}`: uploads, `BLOB` columns), or an opaque C pointer. `{{key}}` interpolates strings and tables into SQL, templates, URLs, and headers; SQL also binds blobs; opaque values are invisible to both.
- Templates and SQL are assets, never context. A step's template key names an asset of its own type: `html("todos", ...)` renders `todos.html`, `sql_query({"todos_db", "get_todos", ...})` runs `get_todos.sql`.
- The core knows no protocol. `http.h` and `rns.h` (Reticulum) are modules; one app can serve both, sharing databases, routines, and events.
- C23 with clang blocks (`^(){...}`), `nullptr`, `bool`, `constexpr`, designated initializers. App code never calls `malloc`/`free` or manages threads or locks.

## Style

- Hypermedia first: render HTML (http) or Micron (rns) on the server. Forms use POST-redirect-GET; live updates are `datastar()` patches over a resource's `.sse` channel. JSON endpoints only for API clients or a single-page app's backend.
- One module per domain slice (`todos`, `activity`), owning its databases, SQL, templates, resources, and events. Modules talk only through events and links to each other's pages, never each other's databases or assets.
- Logic lives in steps: SQL in `.sql` assets, markup in templates, only shaping and decisions in `run()`. Prefer a declarative step over C, and many small steps over one large one (see Gotchas).
- Key suffixes: `_s` rendered string, `_data` table, `_d` single-row table; validated inputs and flags bare (`title`, `is_urgent`). Database keys end in `_db`; SQL assets are verbs (`get_todos`), templates nouns (`todo_row`).

```
.
├── app.c              # the app module; shared resources such as home
├── layout.html        # app assets sit at the root; every module sees them
├── pricing.c          # root library: plain C every module may call ("pricing.h")
├── pricing.h
├── modules/
│   └── todos/         # the todos module (no subfolders)
│       ├── module.c
│       ├── format.c   # plain C only todos uses ("format.h")
│       ├── format.h
│       ├── todos.html # module assets are private to todos
│       ├── todos.mu   # same key, Micron, for rns()
│       └── get_todos.sql
├── overrides/         # changes to bundled and vendor modules
│   └── cookie_auth/   # module.c (empty module(){} for assets only), login.html
├── public/            # static files: {{public:file}}
├── vendor/            # prebuilt libraries: name.h, name.<arch>.so (dev), name.<arch>.a (production)
├── Dockerfile         # the Docker image (nerack docker writes a starter once; edit freely)
└── compose.yml        # how the image runs (likewise)
```
Module folder names are C identifiers, unique against `app`, bundled modules, and vendor libraries, and never `overrides`. Nerack never enters any other root folder, so docs, scripts, and `node_modules/` can sit beside the app. Never edit `.build/`, `.nerack/local/`, `.nerack/version`, or `output/` (`nerack pin` changes the version); `.nerack/arch`, the architectures the app ships for, is the project's to change. Running, checking, and building the app (`nerack`, `nerack info`, `nerack build`) are in the `nerack-cli` skill; shipping it is in `nerack-deploy`.

## Syntax

- Every call is a macro over a struct literal. Leading values are positional in field order (see Signatures), the rest are named: `sql_query({"todos_db","get_todos","todos_data"})` = `sql_query({.database_key="todos_db", .template_key="get_todos", .context_key="todos_data"})`.
- List steps take one `{...}` per item, and items run concurrently: `validate`, `sql_query`, `http_fetch`, `http_headers`, `http_cookies`. Write `validate({"input:title", not_empty_valid})`, never `validate("input:title", ...)`.
- Other steps take fields directly: `html("todos", "todos_s")`.
- A pipeline is a brace list of steps: `.get = {step, step}`. Module-level declarations are statements ending in `;`.
- Handlers are `{code, {steps}}`: `.errors = {{http_bad_request, {http_reroute("todos")}}}`; module-level `error(http_not_found, {steps});`.
- Every step also takes `.if_context_key`, `.unless_context_key`, `.map`, `.map_context_key` (per item in list steps).

## Signatures

Fields in positional order; `{...}` is a nested list, `, ...` a list step.
```c
// nerack.h
module(){ ... }
routine(.routine_key, {.steps}, .errors, .repairs, .accepts = {"key", ...}, .gives = {"key", ...});
error(.code, {.steps});
repair(.code, {.steps});
text_template(.template_key, .template);           // declares <key>.txt
validate({.context_key, .regex, .error_message, .optional, .default_value, .types = {"image/png", ...}, .max_bytes}, ...)
join(.parent_context_key, .parent_field_key, .child_context_key, .child_field_key, .parent_child_join_key)
run(^(){ ... })  |  run(.call = fn)                // .icall is the positional field
run_worker(^(){ ... })  |  run_worker(.call = fn)  // + .queue = "name": one call at a time
run_routine(.key)
nest({.steps})
text(.template_key, .context_key)                  // renders <key>.txt, no escaping
context(.context_key, .template)                   // renders a literal template, no escaping

// sqlite.h includes sql.h (other SQL engines: postgres.h, mysql.h, ..., same shape)
sqlite(.database_key, .connection, {.migrations}, {.seeds});
sql_query({.database_key, .template_key, .context_key, .error_on_empty}, ...)  // any SQL engine
sql_template(.template_key, .template, .engine);   // declares <key>.sql (.engine: <key>.<engine>.sql)

// http.h
http(.name, .url, .mime, .errors, .repairs, .all, .get, .post, .put, .patch, .delete, .sse = {.channel, {.steps}});
html_template(.template_key, .template);           // declares <key>.html
markdown_template(.template_key, .template);       // declares <key>.md
html(.template_key, .context_key)
markdown(.template_key, .context_key)
json(.table_key, .context_key, .record)          // .record: one row as an object
http_response(.context_key, .status, .mime)
http_file_response(.table_key)                     // one-row table: data, type, name
http_fetch({.method, .url, .context_key, .json_table_key, .json_record, .json, .text, .headers = {{"Name", "value"}, ...}}, ...)
http_sse(.channel, .event, .comment, .data = {"line", ...})
http_headers({.key, .value}, ...)
http_cookies({.key, .value}, {.key[, .value]}, ...) // first pair is the cookie, the rest its attributes
http_redirect(.key)                                // resource in this module
http_redirect_to(.module_key, .resource_key)       // resource in another module
http_redirect_external(.url)                       // off-site 302
http_reroute(.key)

// rns.h
rns(.name, .url, .errors, .repairs, .all, .get, .post, .put, .patch, .delete);
micron_template(.template_key, .template);         // declares <key>.mu
micron(.template_key, .context_key)
rns_response(.context_key)
rns_reroute(.key)

// pubsub.h
publish(.event, .with = {"key", ...});
subscribe(.event, {.steps}, .errors, .repairs);
emit(.key)

// task.h
dispatch(.key)
cron(.routine_key, .schedule);

// datastar.h: name every field after .channel (the second positional field is .elements)
datastar(.channel, .elements, .javascript, .target, .signals, .mode)

// cookie_auth.h (includes auth.h)
cookie_logged_in()  cookie_session()  cookie_login()  cookie_logout()  cookie_signup()  cookie_link()  cookie_sign_in()  auth_user()

// rns_auth.h (includes auth.h)
rns_logged_in()  rns_session()  rns_login()  rns_signup()  rns_link()  rns_sign_in()  auth_user()

// htmx.h, tailwind.h, daisyui.h: no steps, only partials ({{> htmx}}, {{> tailwind}}, {{> daisyui}}; datastar.h adds {{> datastar}})
```

## Gotchas

- List steps need inner braces: `validate({...})`, `sql_query({...})`, `http_fetch({...})`.
- No dot notation in templates: `{{#a}}{{b}}{{/a}}`, never `{{a.b}}`.
- Regex is POSIX extended: `[0-9]`, `[[:space:]]`, never `\d` or `\s`.
- Only resources and events cross modules; databases, routines, assets, and handlers resolve within the module, and reroutes stay in it. Context is the one shared namespace. Every name is checked at boot: an unknown name, `sql_query` on a non-SQL database, a `{{url:}}` without its module, or another module's database, routine, or asset fails it.
- An override (`overrides/<name>/module.c`) of a bundled or vendor module replaces its same-named resources, assets, routines, handlers, and databases (any engine of the same kind: SQL for SQL) and adds new ones; it cannot redeclare its events. Bundled database keys: `auth_db`, `cookie_auth_db`, `rns_auth_db`, `pubsub_db` (events), `task_db` (tasks), all SQLite in `DATA_DIR` by default. App modules cannot be overridden.
- Context keys are global to the run: give each output its own key.
- A template key names an asset, never a context value; never build a template or SQL from context.
- Includes: `html`/`markdown`/`json`/`http_*` need `http.h`; `micron`/`rns_response`/`rns_reroute` `rns.h`; `cookie_*` `cookie_auth.h`; other `rns_*` `rns_auth.h`; `publish`/`subscribe`/`emit` `pubsub.h`; `dispatch`/`cron` `task.h`; `datastar` `datastar.h`.
- A reroute runs the target's GET, not the current verb. Each resource's `.all` runs at most once per request.
- State-changing actions (logout, delete) are POST forms or Datastar actions, never links.
- A query result is a table: `{{#todo_data}}...{{/todo_data}}` even for one row.
- Templates hold no logic (no comparisons or arithmetic): compute a flag named for the question (`is_owner`, `is_overdue`) in SQL when it comes from the row (`owner_id = {{user_id}} as is_owner`; `0`/`1` are false/true), in `run()` when it needs several results or C, and test it with `{{#is_owner}}`. Empty states need no flag: `{{^todos}}` is true for a zero-row table. When the response itself changes (another template, a redirect), gate the step with `.if_context_key` instead.
- `run()` must not block; use `run_worker()`.
- `error_set()` does not stop the block: the step finishes, then the handler runs. Return right after it when nothing else should run.
- Break every job into small steps: several short `run()`/`run_worker()` steps, one action each, never one large block. Each step is its own span and log line, an error fires as soon as its step finishes, a repair reruns only that step, and a task resumes from it. To see what code does, split the step; do not add `printf`/`fprintf(stderr, ...)`. There is no logging call; `stdout`/`stderr` reach the logs, but use them only as a last resort, when one step must report something its span cannot show.
- Keep no mutable globals: pipelines run on many threads at once, and dev reloads reset a saved file's globals. Globals hold constants; changing state goes in a database, per-run state in context. Call only thread-safe library functions; put a library that is not on one `run_worker(.queue = ...)`.
- Events and tasks run at least once (a crash replays the event, or reruns the step that was running), so make subscribers and task steps with outside effects idempotent. Subscribers run after the emitting run, never inside it.
- Every file compiles with `-Wall -Wextra`; a format string that is not a literal is an error even in dev, and `nerack build` fails on any warning.
- No built-in test runner: apps are tested from outside, over their protocols, with whatever tools the project uses. Never write C harnesses against Nerack internals.

## Framework-handled (do not reimplement)

Per-run memory, SQL injection (bound parameters), XSS (escaping), CSRF (same-origin checks, `SameSite=Lax`; no tokens), concurrency and connection pooling, durable events and tasks, OpenTelemetry per step, compilation and hot reload.

## Reference

Read the file for what the task touches:

- [references/steps.md](references/steps.md): what each step does: validation, uploads, SQL, joins, rendering, responses, redirects and reroutes, headers and cookies, SSE and Datastar, `.map`, `.if_context_key`.
- [references/resources.md](references/resources.md): resources, assets and their visibility, templates and their helpers (`{{url:}}`, `{{sse:}}`, `{{method:}}`), layouts, Micron, errors and repairs.
- [references/events-tasks.md](references/events-tasks.md): events, routines, tasks, and cron.
- [references/bundled-modules.md](references/bundled-modules.md): the bundled modules: htmx, Datastar, Tailwind, DaisyUI, cookie and RNS authentication.
- [references/c-api.md](references/c-api.md): the C API inside `run()` and `run_worker()`, plain C and root libraries, vendor libraries, exports, and the constants (verbs, statuses, mime types, validators).
- [references/recipes.md](references/recipes.md): short patterns: custom validation, per-user databases, htmx fragments, JSON APIs, outbound calls, background and scheduled jobs, blocking C.
- [references/examples.md](references/examples.md): two complete modules: todos with an event subscriber, and a live chat over HTTP and RNS.

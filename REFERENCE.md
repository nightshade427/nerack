[← Back to README](./README.md)

# Reference

* [Project Layout](#project-layout)
* [Modules and Composition](#modules-and-composition)
* [Context](#context)
* [Assets](#assets)
* [Pipeline Steps](#pipeline-steps)
* [Conditionals](#conditionals)
* [Iteration](#iteration)
* [Imperative API](#imperative-api)
* [Templates](#templates)
* [Error and Repair Pipelines](#error-and-repair-pipelines)
* [Databases](#databases)
* [Event Pipelines](#event-pipelines)
* [Routines](#routines)
* [Protocols](#protocols)
  * [http](#http)
  * [rns](#rns)
* [Bundled Modules](#bundled-modules)
* [Static Files](#static-files)
* [Configuration](#configuration)
* [External Dependencies](#external-dependencies)
  * [Your own C code](#your-own-c-code)
  * [Exporting](#exporting)

## Project Layout

An app is a folder whose file names say what everything is. The root holds `app.c`, the app module, along with the app's assets and any plain C every module may use. Every other module gets a folder of its own, named after it, holding its `module.c`, its assets, and any C files of its own.

```
my_app/
├── app.c # module(){...}: the app module, named app
├── home.html # → app
├── layout.html # → app, and every module
├── 404.html # → app, and every module
├── pricing.c # plain C any module can call
├── pricing.h # #include "pricing.h"
├── todos/ # the todos module
│   ├── module.c # module(){...}
│   ├── format.c # plain C only todos uses
│   ├── format.h # #include "format.h"
│   ├── todos.html # → todos
│   ├── create_todos_table.sql # → todos
│   └── get_todos.sql # → todos
├── overrides/ # changes to bundled and vendor modules
│   └── cookie_auth/
│       ├── module.c # module(){ http("login", "/login", ...); }
│       └── login.html
├── public/ # static files, served directly
│   └── favicon.png
├── vendor/ # prebuilt libraries
└── output/ # what nerack build produces
```

The rules are few and checked on every build:

* The root holds `app.c` and, beside it, any other `.c` and `.h` files: plain C that the app module and every other module may use (see [Your own C code](#your-own-c-code)).
* Every other folder at the root is a module, named after the folder. It is flat (no subfolders) and holds a `module.c`, plus any other `.c` and `.h` files, which only that module uses. An `export.h` beside `module.c` is the module's public interface and also exports the module for other apps (see [Exporting](#exporting)); every other header is private.
* Only `app.c` and a `module.c` declare `module(){...}`; every other `.c` file is plain C.
* A folder's name is a C identifier, and it names the module, so names are unique by construction. No folder takes the name of the app module (`app`), of a bundled module (`http`, `cookie_auth`, ...), or of a library in `vendor/`; `overrides`, `public`, `vendor`, and `output` are reserved.
* `overrides/` holds one folder per overridden module, laid out like a module folder (see [Modules and Composition](#modules-and-composition)).

A file that breaks a rule fails dev mode and `nerack build` with its path and the rule.

## Modules and Composition

A module is declared with `module(){...}` in `app.c` or in a folder's `module.c`, and takes its name from where it sits: `app.c` declares `app`, `todos/module.c` declares `todos`. A module owns its own resources, databases, migrations, routines, and event contracts. Each module sees its own assets and the app's; see [Assets](#assets).

A module is a self-contained system: every resource, database, routine, asset, and handler it declares belongs to the module, its steps use them by bare name, and no other module can use its databases, routines, assets, or handlers. Only two things cross module boundaries. Resources do, so modules can link and redirect to each other's pages, always naming the module: `{{url:todos:todos}}` in a template, `http_redirect_to("todos", "todos")` in a pipeline. [Events](#event-pipelines) do, carrying only the keys declared in `publish()`; event names are global, so a subscriber names the event, never the module that publishes it. [Context](#context) is the one shared namespace: within a run, steps from every module read and write the same keys. A step from a bundled module resolves names in the module that provides it, not the caller's: `cookie_session()` called from `todos` loads the user through [auth](#auth), while `todos` itself cannot name `auth`'s database.

**Bundled** modules ship with Nerack (`http`, `auth`, `cookie_auth`, ...); **vendor** modules come prebuilt in `vendor/` (see [External Dependencies](#external-dependencies)); **app** modules are the project's own. To change a bundled or vendor module, override it: a folder in `overrides/` named after the module holds a `module.c`, and whatever that module declares patches the original. A resource replaces the original's resource with the same name and protocol (the whole resource, every verb, and it may change the URL); an asset replaces the original's asset with the same key and type; a routine or handler replaces the one with the same name or code; anything new is added to the module. The override runs as that module, so its pipelines use the module's databases, routines, and assets, and its templates also see the app's assets, such as the app's `layout`. Everything that refers to the module's names (`{{url:cookie_auth:login}}`, `cookie_logged_in()`'s redirect to `login`) reaches the override.

```
├── layout.html
└── overrides/
    └── cookie_auth/ # overrides the bundled cookie_auth module
        ├── module.c # module(){ http("login", "/login", ...); }
        └── login.html # replaces the built-in login page: {{< layout}}...{{/layout}}
```

An override always wins over the original, whatever the load order. An override can replace one of the module's databases by declaring a database under the same key, with any engine and connection (see [auth](#auth) for moving users to Postgres); the module's queries then run against the replacement, so its migrations must create the tables they expect. An override cannot redeclare an event the original publishes, since that would change its contract under every subscriber. To override only assets, give the override an empty `module(){}`. In dev, each override is logged to the console, and removing a definition from the override restores the original; production builds apply overrides silently.

Every name a module references is checked at boot, before any request is served: each template key must have a matching asset, each database key a database, each routine key a routine, each resource and event name a resource or event. An unknown name fails the boot, as does a `{{url:}}` or `{{sse:}}` without its module, or any name for another module's database, routine, asset, or handler.

**`module()`**: declares a module, named after where its file sits (`app.c` → `app`, `todos/module.c` → `todos`, `overrides/cookie_auth/module.c` → `cookie_auth`).
```c
// todos/module.c
module(){
  // resources, databases, routines, subscribers ...
}
```

**`error(...)` / `repair(...)`**: module-scoped error and repair handlers (see [Error and Repair Pipelines](#error-and-repair-pipelines)). They cover resources in the same module; a resource's own `.errors`/`.repairs` take precedence over them for the same code.

Example: a complete module in `blogs/module.c`. Nerack loads it as soon as the folder is in the project.

```c
#include <nerack.h>
#include <http.h>
#include <sqlite.h>

module(){
  sqlite(
    "blog_db",
    "file:blogs.db?mode=rwc",
    {"create_blogs_table", "create_comments_table"}
  );

  http("blog", "/blogs/:id",
    .get = {...}
  );
}
```


## Context

Pipelines read from and write to a shared, scoped key-value store that lives for one pipeline run: a request, a routine, or an event delivery. Every step draws inputs from context and writes outputs back.

Unprefixed names are app scope: values the app itself controls (query results, validated inputs, computed values, and its own cookies, which are signed). Prefixed scopes hold everything else, so an app key never overwrites a value the app does not control:

* `input:xxx`: raw request parameters, written by the client.
* `client:xxx`: other client-written request data, such as headers.
* `server:xxx`: request facts set by the server, such as the client's IP address.
* `env:xxx`: environment variables, for configuration and secrets.
* `error:xxx`: validation and error data.

`validate()` checks a value from any scope and promotes it to app scope under its unprefixed name.

Keys are global to the pipeline run. A step that writes a key already in use replaces its value, so take care not to overwrite a value another step still needs.

Context holds data, never templates or SQL. Those are [assets](#assets), which live apart from context: every template and SQL asset reads its values from the same context, but nothing written to context can become a template or a query.

Values are strings, tables of records (query and fetch results), blobs (raw bytes: uploaded files and `BLOB` columns), or opaque values set from C. Templates see strings and tables; SQL also binds blobs; opaque values are invisible to both. See [context](#context-2) in the Imperative API.

## Assets

An asset is a file of a type that a step of the app reads. Each module registers the types its steps read: the core `.txt` (`text()`), `http.h` `.html` and `.md` (`html()`, `markdown()`), `rns.h` `.mu` (`micron()`), and each database engine `.sql` (`sqlite.h`: queries, migrations, and seeds). Only the types of the modules the app includes are scanned, so an app without `rns.h` embeds no `.mu` files. Every other file, such as a SQLite database, a C header, or a test script, is never an asset. Each asset is available by key to the modules that can see it (see below).

The scan never looks in dotfiles and dot-folders (`.git/`, `.gitignore`, `.clangd`, and `.build/`, where Nerack builds), `output/` (what `nerack build` produces), `public/` (served as-is, see [Static Files](#static-files)), or `vendor/` (prebuilt libraries, see [External Dependencies](#external-dependencies)). By default it also skips `node_modules/` and documentation and agent files, whose name before the first dot is `README`, `CHANGELOG`, `CONTRIBUTING`, `LICENSE`, `COPYING`, `NOTICE`, `CLAUDE`, `AGENTS`, `GEMINI`, or `llms-full` in any letter case (`README.md`, `LICENSE.txt`, `CLAUDE.md`; a copy of `llms-full.txt`, which Nerack writes to `.build/ai/`, see [Coding Agents](./README.md#coding-agents)).

To change what the scan skips, add a `.nerackignore` file to any folder. It uses `.gitignore` syntax and applies to its folder and everything below it, with patterns relative to that folder. A pattern skips the files it matches; a pattern starting with `!` brings back a file that the defaults or a `.nerackignore` higher up skipped. A skipped `.c` file is not built. Skipped files never reach the production binary.

```
# todos/.nerackignore
drafts/
*.draft.html
!README.md
```

An asset's key is the filename's basename, the part before the first dot; the rest of the name says which step reads it. Each step looks only among its own file type, so every step has its own namespace:

* `text()` reads `<key>.txt`
* `html()` reads `<key>.html`
* `markdown()` reads `<key>.md`
* `micron()` reads `<key>.mu`
* `*_query()` and a database's `.migrations`/`.seeds` read `<key>.sql`

A module can add its own (`html_erb()` reads `<key>.html.erb`). One key can name a different file for each step: `html("todo", ...)` renders `todo.html`, `micron("todo", ...)` renders `todo.mu`, and `sqlite_query({"todos_db", "todo", ...})` runs `todo.sql`. A `{{> partial}}` or `{{< parent}}` reference resolves in the namespace of the step rendering the enclosing template, so `todo.html` with `{{> layout}}` pulls in `layout.html` and `todo.mu` pulls in `layout.mu`.

Assets and [context](#context) never mix. A step's template key always names an asset; the values the asset interpolates always come from context. A value written to context, by `validate()`, a query, a cookie, or `set_string()`, never becomes a template or SQL, and an asset never shows up as a context value: in a template, `{{todos}}` is the context key `todos`, not `todos.html`. So template keys and context keys never collide: `html("todos", "todos_s")` reads `todos.html` even when context also holds a `todos` value. A template key with no matching asset fails at boot (see [Modules and Composition](#modules-and-composition)).

A module looks up an asset in its own folder first, then among the app's assets at the root, then among the assets of the bundled and vendor modules it includes (which is how `{{> htmx}}` reaches the htmx runtime). The app module looks at the root, then the bundled and vendor modules. So assets beside `app.c` belong to the app module and are seen by every module, while assets in a module's folder belong to that module alone; an asset in a module's folder with the same key and type as one at the root wins for that module. A module never sees another module's folder:

```
.
├── app.c # the app module
├── home.html # → app (and every module)
├── layout.html # → app (and every module)
├── 404.html # → app (and every module)
└── todos/
    ├── module.c # the todos module
    ├── todos.html # → todos
    └── todo.html # → todos
```

An asset can also be declared in code instead of a file, with [`text_template()`](#text_template), [`sql_template()`](#sql_template), [`html_template()` and `markdown_template()`](#html_template-and-markdown_template), or [`micron_template()`](#rns). A declared asset is the file it stands for in every other way.

A bundled or vendor module ships its own assets and sees only those, never the app's; to replace one, or to give it one it expects the app to provide, put a file with the same key and type in an override module's folder (see [Modules and Composition](#modules-and-composition)).

In dev, the scan is live: editing an asset reloads that file into every module holding it, and saving a `.c` file recompiles it and reloads it with the files that call into it. A production build runs the scan once and embeds every module's assets, override modules' included, in the single production binary, where each module finds them under the same keys as in dev; see [Deployment](./README.md#deployment).

## Pipeline Steps

Steps are the units of work in a pipeline. Each one receives the current context, acts on it, and passes control to the next. All steps accept `.if_context_key`/`.unless_context_key` for [conditional execution](#conditionals), and `.map`/`.map_context_key` for concurrent fan-out across rows of a context table (see [Iteration](#iteration)).

* [validate](#validate)
* [join](#join)
* [run](#run)
* [run_worker](#run_worker)
* [emit](#emit)
* [run_routine](#run_routine)
* [dispatch](#dispatch)
* [nest](#nest)
* [text](#text)
* [context](#context-1)

Database queries are covered under [sqlite_query](#sqlite_query). These steps behave identically under every protocol. The remaining steps ship with a module and are documented alongside it. Any step works anywhere a step is accepted, as long as its module is `#include`d. A step's module need not match the protocol handling the request; `datastar()` called from an `rns()` pipeline patches an `http()` resource's `.sse` channel the same as it does from an `http()` pipeline.

### validate

Checks context values against regex patterns and, on success, moves each to app scope under its unprefixed name: `validate({"input:title", ...})` promotes `title`, `validate({"client:user_agent", no_html_valid})` promotes `user_agent`, `validate({"server:method", ...})` promotes `method`. An unprefixed key is checked in place. Request parameters (query string, form body, URL params, and a JSON body's top-level keys) arrive as `input:name`. On failure, errors land in `error:name` and `err_invalid` (400) triggers the nearest [error/repair pipeline](#error-and-repair-pipelines). All validations in one call complete before the error fires, so all errors are available together for form re-rendering.

Built-in regex macros are defined in `nerack.h`, with `url_valid`, `ipv4_valid`, `no_html_valid`, and `hex_color_valid` added by `http.h`. Patterns use libc regex (POSIX extended, `regcomp` with `REG_EXTENDED`): use bracket classes like `[0-9]`, since `\d` is not supported. Define your own the same way: `#define sku_valid "^[A-Z]{3}-[0-9]{4}$"`.

**`.context_key` *(by order)***: context key to validate, with its scope prefix (`input:title`).
```c
validate({.context_key = "input:title", .regex = "^[^[:space:]]+$", .error_message = "required"})
```

**`.regex` *(by order)***: regex pattern, or a built-in validator macro.
```c
validate({.context_key = "input:email", .regex = email_valid, .error_message = "bad email"})
```

**`.error_message` *(by order)***: human-readable error shown via `{{error_message:name}}`.
```c
validate({.context_key = "input:age", .regex = integer_valid, .error_message = "must be a number"})
```

**`.optional`**: skip validation when the parameter is absent.
```c
validate({.context_key = "input:filter", .regex = "^(active|done)$", .optional = true})
```

**`.default_value`**: default value injected when the parameter is absent.
```c
validate({.context_key = "input:page", .regex = integer_valid, .default_value = "1"})
```

**`.types`** / **`.max_bytes`**: checks for an uploaded file (see Request data under [http](#http)) in place of `.regex`. `.types` lists the allowed types, compared against the type sniffed from the file's contents, never the client's claim; `.max_bytes` caps its size. On success the file's one-row table moves to app scope like any field.
```c
validate({"input:avatar", .types = {"image/png", "image/jpeg"}, .max_bytes = 2000000, .error_message = "a PNG or JPEG up to 2 MB"})
```

Combined:
```c
validate(
  {.context_key = "input:email", .regex = email_valid, .error_message = "must be a valid email"},
  {.context_key = "input:title", .regex = not_empty_valid, .error_message = "cannot be empty"},
  {.context_key = "input:page", .regex = integer_valid, .error_message = "must be a number", .default_value = "1"},
  {.context_key = "input:filter", .regex = "^(active|done)$", .error_message = "must be 'active' or 'done'", .optional = true},
  {.context_key = "input:username", .regex = user_valid, .error_message = "3–32 letters, digits, or _, starting with a letter"}
)
```

For checks beyond regex (uniqueness, cross-field rules, lookups), pair `validate()` with a query and `run()`:
```c
validate({.context_key = "input:username", .regex = user_valid, .error_message = "3–32 letters, digits, or _, starting with a letter"}),
sqlite_query({"users_db", "find_username", "existing"}),
run(^(){
  table_handle rows = get("existing");
  if (rows && table_length(rows) > 0)
    error_set("username", (error){err_invalid, "already taken"});
})
```

**Built-in validators:**
- Strings: `not_empty_valid`, `alpha_valid`, `alphanumeric_valid`, `slug_valid`, `no_html_valid` *(http.h)*
- Numbers: `integer_valid`, `positive_integer_valid`, `float_valid`, `percent_valid`
- Identity: `email_valid`, `uuid_valid`, `user_valid`
- Dates & times: `date_valid`, `time_valid`, `datetime_valid`
- Web *(http.h)*: `url_valid`, `ipv4_valid`, `hex_color_valid`
- Codes: `zip_valid`, `phone_valid`, `cron_valid`
- Security: `token_valid`, `base64_valid`
- Boolean: `bool_valid`, `yes_no_valid`, `on_off_valid`

### join

Nests records from one context table into each matching record of another, like a SQL JOIN in memory. Useful when records come from separate databases or queries. After the step, each outer record gains a new field holding its matched inner records.

**`.parent_context_key` *(by order)***: outer table whose records receive nested children.
```c
.parent_context_key = "projects"
```

**`.parent_field_key` *(by order)***: field on the outer table to match against.
```c
.parent_field_key = "id"
```

**`.child_context_key` *(by order)***: inner table whose records get nested.
```c
.child_context_key = "todos"
```

**`.child_field_key` *(by order)***: field on the inner table that points at the outer.
```c
.child_field_key = "project_id"
```

**`.parent_child_join_key`**: new field on outer records holding the matched inner records. Defaults to the child table name.
```c
.parent_child_join_key = "todos"
```

Combined:
```c
join("projects", "id", "todos", "project_id")
```

Example: a blog post and its comments, fetched by two concurrent queries on one database, nested with `join()`, and rendered as one structure:

**`blog.html`**
```html
<article>
  {{#blog}}
    <h1>{{title}}</h1>
    <div>{{content}}</div>
    <h2>Comments</h2>
    <ul>
      {{#comments}}
        <li>{{body}}</li>
      {{/comments}}
    </ul>
  {{/blog}}
</article>
```

```c
http("blog", "/blogs/:id",
  .get = {
    validate({"input:id", positive_integer_valid}),

    // Fetch both concurrently: one sqlite_query() call, two items
    sqlite_query(
      {"blog_db", "get_blog", "blog"},
      {"blog_db", "get_comments", "comments"}
    ),

    // Nest each comment into its matching blog record
    join("blog", "id", "comments", "blog_id"),

    // Enter {{#blog}} first; after join(), comments lives inside each blog record
    html("blog", "blog_s"),
    http_response("blog_s")
  }
);
```

Context shape at each step:

```
after sqlite_query(): { blog: [{id, title, content}],
                        comments: [{id, blog_id, body}, ...] } // two sibling tables

after join(): { blog: [{id, title, content,
                         comments: [{id, blog_id, body}, ...]}] } // nested inside blog
```

### run

`run()` calls a C function or block inline on the reactor, with access to context through the [Imperative API](#imperative-api). It is where business logic and data shaping live: enriching query results, aggregating, transforming data between steps, and setting flags for [conditional](#conditionals) downstream steps. Use it for short, non-blocking work, and call `error_set()` to trigger an error/repair pipeline. Use `run_worker()` instead when the body would stall the reactor.

**`.icall` *(by order)***: inline block, for short logic specific to this pipeline. This one attaches each challenger's opponent id, so the template can render two voting forms with the right winner/loser pairing:
```c
run(^(){
  table_handle const t = get("challengers");
  record_handle const p0 = table_get(t, 0);
  record_handle const p1 = table_get(t, 1);
  record_set_string(p0, "opponent_id", record_get(p1, "id"));
  record_set_string(p1, "opponent_id", record_get(p0, "id"));
})
```

**`.call`**: reference to a named C function, for logic reuse across pipelines.
```c
run(.call = assign_opponents)
```

Inside blocks and `.call` functions, context, memory, errors, tables, and records are manipulated through the [Imperative API](#imperative-api).

### run_worker

`run_worker()` takes the same block or `.call` as `run()` but is for blocking or CPU-bound work: external C libraries, blocking I/O, heavy computation. The work is dispatched to the shared thread pool, releasing the reactor; the pipeline resumes on the original reactor when the call returns. Use it when the body would stall the reactor.

**`.icall` *(by order)***: inline block, run on the shared thread pool. This one renders Markdown through an external C library and frees its buffer when the run ends:
```c
run_worker(^(){
  char * const raw = third_party_render_md(get("markdown"));
  defer_free(raw);
  set_string("html", raw);
})
```

**`.call`**: reference to a named C function, run on the shared thread pool.
```c
run_worker(.call = resize_image)
```

### emit

Triggers an internal pub/sub event that the module declared with `publish()`. Subscribers in other modules react in their `subscribe()` pipelines, with no direct dependency on the emitter. Requires `#include <pubsub.h>`. See [Event Pipelines](#event-pipelines).

**`.key` *(by order)***: the event's name, as declared in this module's `publish()`; a module can emit only the events it publishes.
```c
emit("todo_created")
```

### run_routine

Runs a named routine inline as a step in the calling pipeline; control returns to the next step when it finishes. The routine must be defined with `routine(name, { ... })`. See [Routines](#routines).

**`.key` *(by order)***: name of a defined routine.
```c
run_routine("recount_todos")
```

### dispatch

Runs a named routine as a task: a durable background job; the calling pipeline continues immediately. The task reactor picks up queued tasks and runs them. A task is checkpointed after each step, so a crash mid-task resumes where it stopped. Requires `#include <task.h>`, which provides the persistent task tables. The routine must be defined with `routine(name, { ... })`. See [Routines](#routines).

**`.key` *(by order)***: name of a defined routine.
```c
dispatch("record_daily_stats")
```

### nest

Groups multiple steps into a single composite step. Useful when applying one `.if_context_key`/`.unless_context_key` to several steps without repeating it.

**`.steps` *(by order)***: array of steps that run as a unit.
```c
nest({sqlite_query({...}), emit("urgent_todo"), html("urgent", "urgent_s"), http_response("urgent_s")})
```

**`.if_context_key` / `.unless_context_key`**: condition applied to the whole group.
```c
nest({sqlite_query({...}), emit("urgent_todo"), html("urgent", "urgent_s"), http_response("urgent_s")},
  .if_context_key = "is_urgent")
```

### text

Renders a plain-text Mustache template asset into context: `text("greeting", ...)` reads `greeting.txt`. Output is plain text, so `{{x}}` is not HTML-escaped. Use it for plain-text responses, email bodies, and any other string built from context.

**`.template_key` *(by order)***: asset to render.
```c
text(.template_key = "greeting", .context_key = "greeting_s")
```

**`.context_key` *(by order)***: context key to write the rendered output to.
```c
// greeting.txt: hello {{name}}
text("greeting", "greeting_s"),
http_response("greeting_s", .mime = mime_text)
```

### text_template

Declares a plain-text template asset in code. `text_template()` is declared at module level, like `routine()`. `text_template("greeting", ...)` registers the asset that `greeting.txt` would, as if the file sat beside the module's `.c` file, so `text()`, partials, overrides, and the boot check treat it as that file. A declaration and a file with the same key in the same folder fail the boot.

**`.template_key` *(by order)***: asset key.
```c
text_template(.template_key = "greeting",
  .template = "hello {{name}}"
);
```

**`.template` *(by order)***: template source. It is code: pass a string literal, never a context value.
```c
text_template("greeting",
  "hello {{name}}"
);
```

### context

Renders a template given in the step into context: the step form of [`format()`](#context-2). Output is plain text, so `{{x}}` is not HTML-escaped. The template is a string literal known at boot, so a `{{url:}}` or `{{sse:}}` in it fails the boot, as in a `text()` asset. Use it for short strings; a longer or shared template belongs in a `text()` asset.

**`.context_key` *(by order)***: context key to write the rendered output to.
```c
context(.context_key = "greeting_s", .template = "hello {{name}}")
```

**`.template` *(by order)***: template to render. It is code: pass a string literal, never a context value.
```c
context("greeting_s", "hello {{name}}"),
http_response("greeting_s", .mime = mime_text)
```

## Conditionals

Every step accepts `.if_context_key` and `.unless_context_key`, naming a context variable. They work for any context value: validated inputs, query results, framework flags like `is_htmx`, or flags set from `run()`.

A value is false when the key is missing or holds `nullptr`, `""`, `"0"`, `"false"`, or a table with no rows; anything else is true, including an opaque value set from `run()`. Template sections use the same rule, so a SQLite boolean (`0`/`1`) or a `bool_valid` input works directly in either.

**`.if_context_key`**: context key; the step runs only when its value is true.
```c
html("fragment", "frag_s", .if_context_key = "is_htmx")
```

**`.unless_context_key`**: context key; the step runs only when its value is false.
```c
html("full_page", "page_s", .unless_context_key = "is_htmx")
```

For multi-state branching, set context flags from `run()`, then key downstream steps off them:

```c
run(.call = classify_todo),
html("urgent_confirmation", "urgent_s", .if_context_key = "is_urgent"),
http_response("urgent_s", .if_context_key = "is_urgent"),
html("standard_confirmation", "standard_s", .unless_context_key = "is_urgent"),
http_response("standard_s", .unless_context_key = "is_urgent")
```

## Iteration

`.map` and `.map_context_key` run a step once per row of a context table, all rows concurrently, like multiple items in `*_query()` or `http_fetch()`. With a `.context_key`, results are collected into a table aligned with the input, one entry per row. They differ in how each row reaches the step body: `.map` puts the row's fields in scope as bare `{{interpolations}}`; `.map_context_key` binds the row as a single-row table under a named key.

**`.map`**: name of a context table to iterate over. The row's fields land in scope as bare interpolations.
```c
// One request per row in `users`, all concurrent.
// Each row's `id` fills the URL; responses collected into `profiles`, aligned with `users`.
http_fetch({http_get, "https://api.users.dev/{{id}}", "profiles", .map = "users"})
```

**`.map_context_key`**: context key under which the current row is exposed as a single-row table. Pairs with `.map`. Needed when a step takes a key naming a table rather than reading bare fields, such as `.json_table_key` or `json()`.
```c
// POST each row of `todos` as its own JSON body, all concurrent.
// `.map_context_key = "todo_d"` presents the current row as the single-row table `todo_d`
// Responses are collected into `synced`, aligned with `todos`.
http_fetch({http_post, "https://api.sync.dev/todos", "synced", .json_table_key = "todo_d", .map = "todos", .map_context_key = "todo_d"})
```

## Imperative API

Functions called from `run()`/`run_worker()` blocks and `.call` functions to read and write context, allocate memory, raise errors, and manipulate tables, records, and blobs.

* [context](#context-2)
* [memory](#memory)
* [errors](#errors)
* [tables](#tables)
* [records](#records)
* [blobs](#blobs)

### context

Read, write, and test context keys, and resolve `{{interpolation}}` against the current scope.

**`get(name)`**: returns the value of `name` as a `void*`, or `nullptr` if absent. The caller must know what the key holds and assign it to the matching type.
```c
table_handle todos = get("todos");   // a query result
string title = get("title");          // a validated input
```

**`set_string(name, value)`**: writes a string to `name`, exposing it to downstream steps, templates, and SQL.
```c
set_string("title", "New title");
```

**`set_table(name, table)`**: writes a table to `name`, so templates can iterate it with `{{#name}}` and steps like `json()` and `join()` can read it.
```c
set_table("active_users", active);
```

**`set_blob(name, blob)`**: writes raw bytes to `name`, which SQL binds as a `BLOB` and `http_file_response()` can send. A `blob` is `{data, size}`; uploads and `BLOB` query columns produce the same type, so `get()` or `record_get()` on either returns a `blob_handle` (`blob const*`). Make a new blob with [`blob_new()`](#blobs).
```c
set_blob("thumbnail", blob_new(thumb, thumb_size));
```

**`set_opaque(name, ptr)`**: writes an opaque value (any pointer) to `name`, for passing C data between `run()` steps. Templates and SQL do not see it.
```c
set_opaque("parsed_config", cfg);
```

**`has(name)`**: returns true when `name` exists in the current scope.
```c
if (has("user_id")) { ... }
```

**`format(context_key, template)`**: resolves `{{name}}` interpolations in `template` against the current context and writes the result into `context_key`. Same scopes and helpers as templates, except `{{url:}}` and `{{sse:}}`, which belong to protocol templates; like `text()`, it does not escape. `template` is code: pass a string literal, never a context value, which would let request data become a template. [`context()`](#context-1) is its step form.
```c
format("greeting", "Hello, {{user_name}}");
string greeting = get("greeting");
```

Combined:
```c
run(^(){
  table_handle rows = get("todos");
  if (table_length(rows) > 5) {
    set_string("is_urgent", "1");
    format("banner", "{{user_name}} has more than 5 open todos");
  }
})
```

### memory

Pipeline-arena allocation and deferred cleanup of foreign pointers. Both clear when the run ends.

**`alloc(bytes)`**: returns a buffer from the pipeline arena, reclaimed automatically when the run ends.
```c
void *buf = alloc(256);
```

**`defer_free(ptr)`**: schedules `free()` for a pointer returned by an external library. Runs when the arena is released.
```c
void *out = third_party_alloc(256);
defer_free(out);
```

Combined:
```c
run_worker(^(){
  char *url = alloc(512);
  build_signed_url(url, 512, get("path"));
  set_string("signed_url", url);

  char *raw = third_party_render_md(get("markdown"));
  defer_free(raw);
  set_string("html", raw);
})
```

### errors

Raise field-scoped errors from `run()` to trigger error/repair pipelines. Keys land in the `error:name` scope, visible to templates as `{{error:name}}`, `{{error_code:name}}`, and `{{error_message:name}}`.

**`error_set(name, err)`**: associates an error with `name` and triggers the nearest [error or repair pipeline](#error-and-repair-pipelines).
```c
error_set("token", (error){ err_invalid, "token has expired" });
```

**`error_get(name)`**: returns the `error` previously set on `name`.
```c
error const e = error_get("token");
```

**`error_has(name)`**: returns true when `name` has an error.
```c
if (error_has("token")) { ... }
```

Combined:
```c
run(^(){
  string token = get("token");
  if (!token || strlen(token) < 16) {
    error_set("token", (error){
      err_invalid,
      "token must be at least 16 characters"
    });
  }
})
```

### tables

Tables are ordered collections of records, the shape `*_query()` produces and `http_fetch()` parses JSON into. Use these to build derived results.

**`table_new()`**: returns an empty table in the pipeline arena.
```c
table_handle t = table_new();
```

**`table_length(t)`**: returns the number of records in `t`.
```c
size_t n = table_length(get("todos"));
```

**`table_get(t, i)`**: returns the record at index `i`, or `nullptr` if out of range.
```c
record_handle first = table_get(get("todos"), 0);
```

**`table_add(t, r)`**: appends `r` to `t`.
```c
table_add(t, record_new());
```

**`table_remove(t, r)`**: removes record `r` from `t`.
```c
table_remove(t, r);
```

**`table_remove_at(t, i)`**: removes the record at index `i`.
```c
table_remove_at(t, 0);
```

Combined:
```c
run(^(){
  table_handle source = get("raw_users");
  table_handle active = table_new();
  for (size_t i = 0; i < table_length(source); i++) {
    record_handle u = table_get(source, i);
    string status = record_get(u, "status");
    if (status && strcmp(status, "active") == 0) {
      table_add(active, u);
    }
  }
  set_table("active_users", active);
})
```

### records

Records are name-value bags, the shape of one row from `*_query()` or one object from `http_fetch()`. A field holds a string, a nested table (as `join()` and nested JSON produce), a blob, or an opaque value, with the same rules as [Context](#context).

**`record_new()`**: returns an empty record in the pipeline arena.
```c
record_handle r = record_new();
```

**`record_get(r, name)`**: returns the value of `name` on `r` as a `void*`, or `nullptr` if absent. As with `get()`, the caller must know what the field holds and assign it to the matching type.
```c
string title = record_get(r, "title");
```

**`record_set_string(r, name, value)`**, **`record_set_table(r, name, table)`**, **`record_set_blob(r, name, blob)`**, **`record_set_opaque(r, name, ptr)`**: write a field on `r`, with the same types and visibility as the matching [`set_*()`](#context). A nested table is walkable as `{{#name}}` inside the record's section.
```c
record_set_string(r, "title", "New title");
record_set_table(r, "tags", tags);
record_set_blob(r, "data", blob_new(bytes, size));
record_set_opaque(r, "raw", parsed);
```

**`record_remove(r, name)`**: removes `name` from `r`.
```c
record_remove(r, "draft");
```

Combined:
```c
run(^(){
  table_handle todos = get("todos");
  for (size_t i = 0; i < table_length(todos); i++) {
    record_handle t = table_get(todos, i);
    string title = record_get(t, "title");
    if (title && strlen(title) > 40) {
      record_set_string(t, "is_long", "1");
    }
  }
})
```

### blobs

A blob is raw bytes, `{data, size}`, the type uploads and `BLOB` query columns produce. Build new blobs with `blob_new()`.

**`blob_new(data, size)`**: returns a `blob_handle` holding `{data, size}` in the pipeline arena. It does not copy `data`, which must outlive the run (`alloc`, `defer_free`, or bytes already in context).
```c
unsigned char * const thumb = alloc(thumb_size);
make_thumbnail(get("avatar"), thumb, thumb_size);
set_blob("thumbnail", blob_new(thumb, thumb_size));
```

## Templates

Templates are Mustache assets, rendered by [`text()`](#text) in the core and by each protocol's render steps: `html()` and `markdown()` (MDM, Mustache with Markdown) from [http](#http), `micron()` from [rns](#rns). The language and helpers below are shared by all of them, and by SQL assets (see [sqlite_query](#sqlite_query)). The full Mustache base spec is supported except dot notation: `{{a.b}}` does not work; use `{{#a}}{{b}}{{/a}}`.

Base-spec features:
- **Interpolation**: `{{name}}` (escaped for the output: HTML in `html()`/`markdown()`, Micron markup in `micron()`, none in `text()`), `{{{name}}}` or `{{&name}}` (unescaped).
- **Sections**: `{{#name}}...{{/name}}` renders when true (see [Conditionals](#conditionals) for the rule) and iterates over tables.
- **Inverted sections**: `{{^name}}...{{/name}}` renders when false.
- **Comments**: `{{! ignored }}`.
- **Set delimiters**: `{{=<% %>=}}`.
- **Partials**: `{{> name }}` inlines the asset `name` of the same type as the enclosing template, rendered against the current scope. A partial or parent with no matching asset fails the render with its name and where it was looked for; it never renders as empty.
- **Layout inheritance**: `{{< parent}}{{$block}}override{{/block}}{{/parent}}` renders `parent` with each `{{$block}}default{{/block}}` block replaced by the override. Any asset declaring blocks can be a parent.

A key that is missing, `nullptr`, or holds a value templates cannot render (anything but a string or table) renders as empty, and a section on it is false (in templates an opaque value counts as false). Missing keys are normal (`{{^user}}` on a signed-out page) and are not logged; a value templates cannot render is logged to the console and telemetry.

Built-in helpers use `{{helper:args}}` syntax. Arguments are colon-separated, in order; each can be a literal or a context key.

**`{{precision:field:N}}`**: format a numeric value with N decimal places.
```html
<p>Total: ${{precision:total:2}}</p>
```

**`{{input:field}}`**: raw, unvalidated request parameter from the `input` scope. Used to repopulate form fields after a validation error.
```html
<input name='title' value='{{input:title}}'>
```

**`{{error:field}}`**: truthy when `field` has an error. Used as a Mustache section to conditionally render markup.
```html
{{#error:title}}
  <span class='error'>invalid</span>
{{/error:title}}
```

**`{{error_message:field}}`**: human-readable message for a field error, from `validate()`'s message or from `error_set()`.
```html
<span>{{error_message:title}}</span>
```

**`{{error_code:field}}`**: error code associated with a field error (e.g. `400`, `404`).
```html
<p>Code: {{error_code:title}}</p>
```

**`{{url:module:resource}}`**: resolve a resource to its URL. The first two segments are always the module and the resource; anything after them is arguments. Always name the module, even your own, so a template links the same way wherever it renders: in its own module, or as a layout or partial shared by several. Bundled resources are named the same way (`{{url:cookie_auth:login}}`). `:params` in the URL pattern are read from the current scope by matching key names, so the same helper works per row inside a section. Extra `:key=value` arguments become query parameters, and a bare `:key` argument takes its value from the current scope.

The name is looked up among the resources of the protocol the template renders for: `http()` for `html()` and `markdown()`, `rns()` for `micron()`. Only those protocol templates, and `http` steps that build URLs such as `http_redirect_external()`, take `{{url:}}` and `{{sse:}}`. In a `text()` or SQL asset, or a `context()` template, they fail the boot, and in a `format()` template, which is only known when it runs, they raise `err_internal`.
```html
<a href='{{url:todos:todos}}'>All</a> <!-- /todos -->

<a href='{{url:activity:activity}}'>Activity</a> <!-- a resource in the activity module -->

{{#todos_data}}
  <a href='{{url:todos:todo}}'>{{title}}</a> <!-- /todos/5, filled per row -->
{{/todos_data}}

<a href='{{url:todos:todos:filter=done}}'>Done</a> <!-- /todos?filter=done -->

<a href='{{url:todos:org_todo}}'>Open</a> <!-- /orgs/{{org}}/todos/{{id}}, every slot filled from scope -->
```

**`{{method:verb}}`**: emits a hidden `_method` field so an HTML form, which can only send GET or POST, reaches a `put`, `patch`, or `delete` pipeline. The router honors `_method` only on a POST. A form that posts to `.post` needs no helper. Micron uses it for every non-GET verb; see [rns](#rns).
```html
<form method='post' action='{{url:todos:todos}}'>
  <input name='title'>
  <button>Add</button>
</form>

<form method='post' action='{{url:todos:todo}}'>
  {{method:delete}} <!-- <input type='hidden' name='_method' value='delete'> -->
  <button>Delete</button>
</form>
```
JavaScript clients such as Datastar and htmx send the real method and need no helper: `@delete('{{url:todos:todo}}')`.

**`{{sse:module:resource}}`** *(http)*: the URL of a resource's [`.sse`](#http-pipelines) channel: the same as `{{url:module:resource}}` with `_method=sse` added. It is a GET, so it is safe to use anywhere a URL is accepted.
```html
<div data-init="@get('{{sse:todos:todos}}')"></div> <!-- /todos?_method=sse -->
```

**`{{asset:filename}}`**: resolve a file in `public/` to a cache-busted URL (content checksum + immutable cache headers). See [Static Files](#static-files).
```html
<link rel='stylesheet' href='{{asset:styles.css}}'>
```

## Error and Repair Pipelines

When a step fails, execution halts and Nerack looks for a handler matching the error code. It checks the resource's own `.errors`/`.repairs` first, then the `error()`/`repair()` handlers in that resource's module, and uses the first match. A resource handler takes precedence over the module's for the same code.

Errors are terminal: a request's error handler sends the response, and every error handler ends the run. Repairs are resumable: they fix the context, then the failed step runs again and the pipeline continues from there. In a step with several items, only the items that failed run again; the others keep their results. Each failure gets one repair: if the step fails again with the same code, resolution goes to errors. A repair only fixes context and never sends a response, and a step that fails inside a repair pipeline goes straight to errors. Repairs resolve first; if no matching repair is found, resolution falls through to errors. Unhandled errors fall through to Nerack's internal handler, which renders the template asset named after the error code in the protocol's format (`404.html` for [http](#http), `404.mu` for [rns](#rns)), or sends the error message as plain text when there is no such asset, with that code as the status. Every unhandled error also surfaces in the console and telemetry.

The `error` scope is shared across `validate()` failures and `error_set()` calls: `{{error:name}}`, `{{error_code:name}}`, `{{error_message:name}}`. The raw input value remains in `input:name` for re-rendering forms.

**Resource-scoped (`.errors` / `.repairs` fields):**
```c
http("todos", "/todos",
  .post = { ... },
  .errors = {
    {http_not_found, {
      html("404", "not_found_s"),
      http_response("not_found_s")
    }},
    {http_bad_request, {
      html("form", "form_s"),
      http_response("form_s")
    }}
  },
  .repairs = {
    {http_not_authorized, {run(.call = refresh_session_token)}}
  }
);
```

**Module-scoped (`error()` / `repair()` calls):**
```c
module(){
  error(http_error, {
    html("500", "error_s"),
    http_response("error_s")
  });
  error(http_not_found, {
    html("404", "not_found_s"),
    http_response("not_found_s")
  });
  repair(http_not_authorized, {run(.call = refresh_session_token)});
  // ... resources ...
}
```

Error codes are integers. `nerack.h` names the ones the core raises: `err_invalid` (400, from `validate()`), `err_unauthorized` (401), `err_not_found` (404, from `.error_on_empty`), and `err_internal` (500, an unhandled failure). They share HTTP's numbers, and each protocol presents them its own way. `http.h` aliases them as `http_bad_request`, `http_not_authorized`, `http_not_found`, and `http_error`, so either name works in an `http()` resource. Define your own for domain-specific errors, e.g. `#define err_quota_exceeded 723`.

## Databases

Each database engine is a module. Activate one by `#include`ing its header (`#include <sqlite.h>`), then register one or more databases with `<engine>(...)`. Every engine's header includes `database.h`, which holds the configs all engines share: `database_config` (the fields below) and `database_query_config`. Migrations and seeds are forward-only and index-based: they run in array order, each applied once, with new ones appended to the end. Both are tracked in a `nerack_meta` table.

Multi-tenant databases use `{{interpolation}}` in `.connection`. One pipeline can query several databases, of any engines; see [join](#join) for combining their results.

**`.database_key`**: identifier referenced by the first value of `*_query()` steps.
```c
.database_key = "todos_db"
```

**`.connection`**: engine-specific connection string. Supports `{{interpolation}}` for multi-tenancy.
```c
.connection = "file:{{user_id}}_todo.db?mode=rwc"
```

**`.migrations`**: array of SQL migration entries, applied once each in order. Each entry names a SQL asset (`<key>.sql`).
```c
.migrations = {"create_todos_table", "create_comments_table"}
```

**`.seeds`**: array of SQL seed entries, applied once each in order. Each entry names a SQL asset (`<key>.sql`).
```c
.seeds = {"seed_todos"}
```

Combined:
```c
#include <sqlite.h>

sqlite(
  .database_key = "blog_db",
  .connection = "file:{{user_id}}_blog.db?mode=rwc",
  .migrations = {"create_blogs_table", "create_comments_table"},
  .seeds = {"seed_blogs"}
);
```

### sqlite_query

The bundled SQLite engine provides `sqlite_query()`; an engine added as its own module provides `<engine>_query()` (`postgres_query()`, `redis_query()`, and so on) of the same shape. All share the same `database_query_config`. By order: first value is the database `.database_key`, second is the `.template_key` naming the SQL asset (`<key>.sql`), third is the `.context_key` for the result table (even single-row results are tables). Multiple items in one step run concurrently.

SQL assets are templates like any other: sections, inverted sections, partials, comments, and helpers all work as in [Templates](#templates). The difference is interpolation: each `{{value}}` becomes a prepared-statement placeholder bound to that value, never text spliced into the SQL; `{{{value}}}` and `{{&value}}` bind the same way. A value absent from the current scope binds as SQL `NULL`, not an empty string. A table expands to one placeholder per row, bound to the row's first column, which makes it a ready-made list for `IN`; an empty table is an empty list. A blob binds as a `BLOB`, and a `BLOB` column comes back as a blob.
```sql
-- get_todos.sql
select * from todos
where user_id = {{user_id}}
{{#filter}}and status = {{filter}}{{/filter}}

-- get_todos_by_ids.sql: ids is a table, so this is in (?, ?, ...)
select * from todos where id in ({{ids}})
```

A section over a table repeats its SQL once per row, with the row's fields in scope. Wrapped in a transaction, that inserts many rows atomically:
```sql
-- add_tags.sql
begin;
{{#tags}}
insert into tags (todo_id, name) values ({{todo_id}}, {{name}});
{{/tags}}
commit;
```

Each item runs its whole SQL asset atomically and in isolation: its statements never interleave with other queries. A transaction therefore lives inside one asset: put `BEGIN`/`COMMIT` in the SQL. If a statement fails, or the script ends with a transaction still open, it is rolled back before the step raises its error. A transaction cannot span steps or items; concurrent items are independent of each other, so work that must be atomic belongs in one asset. When a script has several statements, the result table is the last statement that returns rows (a final `SELECT` or `RETURNING`).

**`.database_key` *(by order)***: database name, matching the name a `<engine>(...)` was registered with.
```c
sqlite_query({.database_key = "todos_db", .template_key = "get_todos", .context_key = "todos_data"})
```

**`.template_key` *(by order)***: SQL asset to run; `"get_todos"` runs `get_todos.sql`.
```c
sqlite_query({.database_key = "todos_db", .template_key = "get_todos", .context_key = "todos_data"})
```

**`.context_key` *(by order)***: context key for the result table. Optional; omit it when the result is not needed (e.g. an insert without `RETURNING`).
```c
sqlite_query({.database_key = "todos_db", .template_key = "create_todo"}) // no result captured
sqlite_query({.database_key = "todos_db", .template_key = "get_todos", .context_key = "todos_data"}) // result under "todos_data"
```

**`.error_on_empty`**: when true, raise `err_not_found` (404) if the query affects/returns zero rows (in a multi-statement script, the last statement that returns rows). Default false.
```c
sqlite_query({.database_key = "todos_db", .template_key = "get_todo", .context_key = "todo", .error_on_empty = true})
```

**`.if_context_key` / `.unless_context_key`** *(per item)*: conditionally include or skip individual queries while running the others concurrently.
```c
sqlite_query(
  {.database_key = "db", .template_key = "get_todos", .context_key = "todos_data"},
  {.database_key = "db", .template_key = "get_urgent", .context_key = "urgent", .if_context_key = "show_urgent"}
)
```

Combined:
```c
sqlite_query(
  {.database_key = "todos_db", .template_key = "get_todos", .context_key = "todos_data"},
  {.database_key = "todos_db", .template_key = "get_todo", .context_key = "todo", .error_on_empty = true},
  {.database_key = "todos_db", .template_key = "get_urgent", .context_key = "urgent", .if_context_key = "show_urgent"}
)
```

### sql_template

Declares a SQL asset in code. `sql_template()` is declared at module level, like `routine()`. `sql_template("count_todos", ...)` registers the asset that `count_todos.sql` would, as if the file sat beside the module's `.c` file, so `*_query()` steps, `.migrations`, `.seeds`, partials, overrides, and the boot check treat it as that file. A declaration and a file with the same key in the same folder fail the boot.

**`.template_key` *(by order)***: asset key.
```c
sql_template(.template_key = "count_todos",
  .template = "select count(*) as n from todos where user_id = {{user_id}}"
);
```

**`.template` *(by order)***: SQL source. It is code: pass a string literal, never a context value.
```c
sql_template("count_todos",
  "select count(*) as n from todos where user_id = {{user_id}}"
);
```

### Engines

SQLite is the bundled engine: `#include <sqlite.h>`, then `sqlite(...)` to register and `sqlite_query({...})` as a pipeline step.

```c
#include <sqlite.h> // sqlite("...", "file:app.db?mode=rwc", ...); sqlite_query({...});
```

SQLite databases are files in `DATA_DIR` (see [Configuration](#configuration)), owned by the one Nerack process that serves the app; that process scales across the machine's cores with its reactor and worker counts. To share data across several processes or machines, use a networked engine instead.

Any other engine is added as its own module the same way, registering `<engine>(...)` and providing `<engine>_query({...})`. Its header includes `<database.h>`, so it shares `database_config` and `database_query_config` from [Databases](#databases) and [sqlite_query](#sqlite_query); only `.connection` is engine-specific. The shape an added engine takes:

```c
#include <postgres.h> // postgres("...", "postgres://...", ...); postgres_query({...});
#include <mysql.h> // mysql("...", "mysql://...", ...); mysql_query({...});
#include <redis.h> // redis("...", "redis://...", ...); redis_query({...});
#include <duckdb.h> // duckdb("...", "duckdb:analytics.db", ...); duckdb_query({...});
```

## Event Pipelines

Internal pub/sub for cross-module communication. The publisher does not know who listens; the subscriber does not know who emits. Adding a subscriber means adding a new module with a `subscribe(...)` call; the publisher does not change. Activate with `#include <pubsub.h>` in any module that publishes or subscribes.

Event names are global: an event is a fact about the app (`todo_created`, `order_placed`), not an address, so a subscriber names only the event, never the module that publishes it, and a publisher can move to another module without breaking any subscriber. Each event has one publisher: two modules publishing the same name, or a subscriber to a name no module publishes, fail the boot. A module can emit only events it publishes.

Events are durable. When a publisher is declared, Nerack creates a `nerack_events` database to track delivery. If the process crashes, undelivered events replay on the next boot.

**`publish(event, .with = {...})`**: declares an outbound event contract. First value is the event's name; `.with` lists context keys to pass along.
```c
publish("todo_created",
  .with = {"user_id", "title"}
);
```

**`subscribe(event, { steps })`**: registers a subscriber pipeline for an event, by its name.
```c
subscribe("todo_created", {
  sqlite_query({"activity_db", "insert_activity"})
});
```

**`emit(event)`**: a pipeline step that fires the event (see [emit](#emit)).
```c
emit("todo_created")
```

**`.errors` / `.repairs`** *(per subscriber)*: each `subscribe(...)` can declare its own handlers, resolved the same way as resource pipelines (the subscriber's own handlers, then its module's). See [Error and Repair Pipelines](#error-and-repair-pipelines).
```c
subscribe("todo_created", {
  sqlite_query({"activity_db", "insert_activity"})
}, .errors = {{err_internal, {run(.call = log_subscriber_failure)}}});
```

Combined:
```c
// todos/module.c: publisher
module(){
  publish("todo_created",
    .with = {"user_id", "title"}
  );
  publish("todo_deleted",
    .with = {"user_id", "todo_id"}
  );

  http("todos", "/todos",
    .post = {
      validate({"input:title", not_empty_valid}),
      sqlite_query({"todos_db", "insert_todo"}),
      emit("todo_created"),
      http_redirect("todos")
    }
  );
}

// activity/module.c: subscriber
module(){
  subscribe("todo_created", {
    sqlite_query({"activity_db", "insert_created_activity"})
  });
  subscribe("todo_deleted", {
    sqlite_query({"activity_db", "insert_deleted_activity"})
  });
}
```

## Routines

A routine is a named, reusable pipeline, defined inside a module with `routine(name, { pipeline }, ...)`. Registration and invocation are separate; a routine runs in one of three ways:

* `run_routine("name")` runs it inline as a step in the calling pipeline.
* `dispatch("name")` runs it as a task: a durable background job on the task reactor; the caller continues immediately. Requires `#include <task.h>`.
* `cron("name", schedule)` dispatches it on a schedule, with no caller. Requires `#include <task.h>`.

Tasks are durable: `task.h` creates the persistent task tables and checkpoints context after each step, so a crash mid-task resumes at the step where it stopped on the next boot. Scheduled runs are dispatched, so they are durable too.

Any pipeline, including a routine's, can call `run()`, `run_worker()`, `run_routine()`, and `dispatch()` (the last requires `task.h`).

**`.routine_key` *(by order)***: routine name, invoked via `run_routine("name")`, `dispatch("name")`, or `cron("name", ...)`.
```c
routine("recount", {
  sqlite_query({"db", "recount_todos"})
});
```

**`.steps` *(by order)***: the routine's pipeline body, a brace block.
```c
routine("name", {
  sqlite_query({...}),
  emit("done"),
  dispatch("followup")
});
```

**`.accepts`**: context keys to pull from the caller into the routine.
```c
routine("recount_todos", {
  sqlite_query({"db", "recount"})
}, .accepts = {"user_id"});
```

**`.errors` / `.repairs`** *(per routine)*: each routine can declare its own handlers, resolved the same way as resource pipelines (the routine's own handlers, then its module's). See [Error and Repair Pipelines](#error-and-repair-pipelines).
```c
routine("send_invoice", {
  http_fetch({http_get, "https://api.billing.dev/invoices/{{invoice_id}}", "inv"})
}, .repairs = {{err_unauthorized, {run(.call = refresh_billing_token)}}});
```

Combined:
```c
// on-demand: dispatched via dispatch("recount_todos")
routine("recount_todos", {
  sqlite_query({"todos_db", "recount"})
}, .accepts = {"user_id"});

// recurring: dispatched on schedule, no caller
routine("daily_digest", {
  sqlite_query({"todos_db", "digest"}),
  emit("digest_ready")
});
cron("daily_digest", "0 8 * * *");
```

### cron

Dispatches a routine on a schedule. `cron()` is declared at module level, like `routine()`, and requires `#include <task.h>`. Each time the schedule fires, the routine is [dispatched](#dispatch), so a scheduled run is a durable task like any other. A routine can have several schedules, one `cron()` each.

If the app was down when one or more runs were due, the routine runs once at boot, not once per missed run. If the previous scheduled run of the routine is still queued or running when the schedule fires, that run is skipped and logged.

**`.routine_key` *(by order)***: name of a defined routine.

**`.schedule` *(by order)***: standard five-field cron schedule.
```c
cron("daily_digest", "0 8 * * *");
cron("daily_digest", "0 20 * * 5"); // and Friday evenings
```

## Protocols

A protocol module registers itself with Nerack and runs its own reactor threads, each with its own event loop, alongside the task reactor and the shared thread pool. `http` is the reference implementation and the one most apps include; `rns` serves Reticulum destinations from the same pipeline model. A new protocol defines its own pipelines, request/response handling, and serialization format, then registers as a module. An app can run several protocols at once, sharing databases, routines, and events between them, and a single pipeline can cross protocols; see [Pipeline Steps](#pipeline-steps).

Protocols are hypermedia-first: resources return rendered documents (HTML, Micron) whose links and forms drive the next request, even on protocols like RNS that lack those semantics natively.

### Pipeline composition

A request runs the resource's `.all` steps first, then the verb pipeline. Cross-cutting setup such as session loading, tenant resolution, and access checks goes in `.all`, where it runs ahead of every verb on that resource. A pipeline that finishes without sending a response gets an empty success response from its protocol (an empty `200` for HTTP), so a pipeline whose real output goes elsewhere, such as a `datastar()` patch over SSE, needs no response step.

```c
// todos/module.c: session loads, resources require login
#include <nerack.h>
#include <http.h>
#include <sqlite.h>
#include <cookie_auth.h>

module(){
  http("todos", "/todos",
    .all = {cookie_logged_in(), cookie_session()},
    .get = {
      sqlite_query({"todos_db", "get_todos", "todos_data"}),
      html("todos", "todos_s"),
      http_response("todos_s")
    },
    .post = {
      validate({"input:title", not_empty_valid}),
      sqlite_query({"todos_db", "create_todo"}),
      http_redirect("todos")
    }
  );

  http("todo", "/todos/:id",
    .all = {cookie_logged_in(), cookie_session(), validate({"input:id", positive_integer_valid})},
    .delete = {
      sqlite_query({"todos_db", "delete_todo", .error_on_empty = true}),
      http_redirect("todos")
    }
  );
}
```

For `DELETE /todos/5` the steps run in this order: the resource `.all` steps (`cookie_logged_in()`, `cookie_session()`, `validate({"input:id", ...})`), then the verb pipeline (`sqlite_query({"todos_db", "delete_todo", .error_on_empty = true})`, `http_redirect("todos")`).

### http

Activate with `#include <http.h>`. It provides the resource/pipeline model (`http()`), HTTP verbs, fetch, response, headers and cookies, redirect and reroute, SSE, and HTML/Markdown/JSON rendering. `html()` renders Mustache and `markdown()` renders MDM (Mustache with Markdown); see [Templates](#templates). Pipelines render HTML on the server, forms follow POST-redirect-GET, and live updates arrive as [Datastar](#datastar) or [htmx](#htmx) patches over SSE. `json()` and `.mime = mime_json` serve JSON to API clients or as a backend-for-frontend (BFF) for single-page apps.

#### HTTP Pipelines

Nerack is resource-based, not route-based. Each `http(...)` defines a named resource and URL with HTTP verb pipelines. `{{url:module:resource}}`, `http_redirect()`, `http_redirect_to()`, and `http_reroute()` all identify the target by resource name and read its `:params` from the current scope by matching key names. Templates always name the module; `http_redirect()` and `http_reroute()` take a resource of their own module, and `http_redirect_to()` takes one in another. Names are per module and per protocol: two modules can each have a `home`, and so can an `http()` and an `rns()` resource in one module. URL patterns are not: two `http()` resources with the same pattern fail the boot, unless one overrides the other. Path specificity is automatic: exact matches (`/todos/active`) take priority over parameterized matches (`/todos/:id`) regardless of definition order.

Clients select a verb with the request method. HTML forms, limited to GET and POST, reach `put`, `patch`, and `delete` with a `_method` field on a POST, which `{{method:verb}}` emits; SSE clients connect with a GET carrying `_method=sse`, which `{{sse:module:resource}}` emits (see [Templates](#templates)).

**CSRF.** State-changing requests (POST, PUT, PATCH, DELETE) are accepted only when the browser marks them same-origin (`Sec-Fetch-Site`, falling back to `Origin`), and session cookies are `SameSite=Lax`. Forms, Datastar, and htmx need no token or hidden field. A client that is not a browser, such as an API client, a server calling the app, or a test tool like Hurl, sends neither header by default, so its state-changing requests are rejected unless it sends an `Origin` header matching the app's own origin.

**Request data.** Each request puts these keys in context:

* `server:client_ip`: the client's IP address. Behind a proxy, it is read from `X-Forwarded-For` only when the request comes from an address listed in `env:trusted_proxies`.
* `server:method`, `server:path`, `server:url`: the request method, path, and full URL.
* `client:<name>`: each request header, named in lowercase with `-` as `_` (`client:user_agent`). The client writes these, so check them with [validate](#validate) before relying on them, and never authorize on them.
* Cookies the app set with [http_cookies](#http_headers-and-http_cookies), under their own names.
* `input:<field>` for each file in a multipart form: a one-row table with `name` (the client's filename, untrusted), `type` (sniffed from the contents), `size` (bytes), and `data` (the bytes, as a blob). The body streams into memory that is released when the run ends; nothing touches disk. Uploads larger than `env:max_upload_bytes` (default 10 MB) fail with `err_invalid` and `error:<field>` set. Text fields in the same form arrive as ordinary `input:` strings. Validate the file with `.types`/`.max_bytes`, then store `data` in a database:
```sql
-- save_avatar.sql
{{#avatar}}
insert or replace into avatars (user_id, name, type, data)
values ({{user_id}}, {{name}}, {{type}}, {{data}});
{{/avatar}}
```
Serve it back with [http_file_response](#http_file_response). Large files belong in object storage, reached with `http_fetch()`, rather than in the database.

**`.name` *(by order)***: resource name, unique among the module's `http()` resources, used by `{{url:module:resource}}`, `{{sse:module:resource}}`, `http_redirect()`, `http_redirect_to()`, and `http_reroute()`.
```c
http("todos", "/todos", .get = { ... });
```

**`.url` *(by order)***: URL pattern. Supports `:params`.
```c
http("todo", "/todos/:id", .get = { ... });
```

**`.all`**: shared steps that run before every verb pipeline on the resource.
```c
http("todo", "/todos/:id",
  .all = { validate({"input:id", positive_integer_valid, "must be a number"}) },
  .get = { ... },
  .delete = { ... }
);
```

**`.mime`**: default response content type. Values: `mime_html`, `mime_text`, `mime_event_stream`, `mime_json`, `mime_javascript` (default `mime_html`).
```c
http("feed", "/feed.json", .mime = mime_json, .get = { ... });
```

**`.get` `.post` `.put` `.patch` `.delete`**: verb pipelines: ordered arrays of steps that transform a request into a response.
```c
http("todos", "/todos",
  .get = {
    sqlite_query({"db", "get_todos", "todos_data"}),
    html("todos", "todos_s"),
    http_response("todos_s")
  },
  .post = {
    validate({"input:title", not_empty_valid}),
    sqlite_query({"db", "create_todo"}),
    http_redirect("todos")
  }
);
```

**`.sse`**: persistent SSE channel. The first value is the channel name (supports `{{interpolation}}`); the connect steps follow in their own brace group. Clients connect with `{{sse:module:resource}}` (`?_method=sse`); `.all` runs before the connect steps, as it does for any verb.
```c
http("todos", "/todos",
  .sse = {"todos", {
    sqlite_query({"db", "get_todos", "todos_data"}),
    http_sse(.event = "initial"),
    http_sse(.data = {"{{id}} {{title}}"}, .map = "todos_data")
  }}
);
```
`http_sse()` writes the components you pass straight to the connection, so a record can be built across calls: the `event:` line here, then one `data:` line per row from the `.map`ped call.

**`.errors` / `.repairs`**: resource-scoped error and repair pipelines. See [Error and Repair Pipelines](#error-and-repair-pipelines).
```c
http("todos", "/todos",
  .post = { ... },
  .errors = {{http_bad_request, {
    html("form", "form_s"),
    http_response("form_s")
  }}}
);
```

Combined:
```c
http("todo", "/todos/:id",
  .all = {validate({"input:id", positive_integer_valid, "must be a number"})},
  .get = {
    sqlite_query({"todos_db", "get_todo", "todo", .error_on_empty = true}),
    html("todo", "todo_s"),
    http_response("todo_s")
  },
  .patch = {
    validate({"input:title", not_empty_valid, "required"}),
    sqlite_query({"todos_db", "update_todo"}),
    http_redirect("todo")
  },
  .delete = {
    sqlite_query({"todos_db", "delete_todo"}),
    http_redirect("todos")
  },
  .sse = {"todo:{{id}}", {http_sse(.event = "ready")}},
  .errors = {{http_not_found, {
    html("404", "not_found_s"),
    http_response("not_found_s")
  }}}
);
```

#### http_fetch

Makes one or more HTTP requests and stores responses in context. JSON parses into tables and records (nested tables for nested JSON); plain-text responses are stored as strings. Like `*_query()`, multiple items in one step run concurrently.

By order: first value is the `.method`, second the `.url`, third the `.context_key` for the response. `http_fetch({http_get, "https://api.dev/now", "weather"})` is the same as spelling all three fields out.

**`.method` *(by order)***: HTTP method. Defaults to `http_get`. Values: `http_get`, `http_post`, `http_put`, `http_patch`, `http_delete`.
```c
http_fetch({.url = "https://api.dev/charge", .context_key = "r", .method = http_post})
```

**`.url` *(by order)***: request URL; supports `{{interpolation}}`. As in [`http_redirect_external()`](#http_redirect-http_redirect_to-http_redirect_external-and-http_reroute), values after the `?` are percent-encoded and values before it are inserted as written.
```c
http_fetch({.url = "https://api.weather.dev/forecast?city={{city}}", .context_key = "w"})
```

**`.context_key` *(by order)***: context key for the response.
```c
http_fetch({.url = "https://api.weather.dev/now", .context_key = "weather"})
```

**`.headers`**: array of name/value pairs.
```c
http_fetch({.url = "https://api.dev/me", .context_key = "r", .headers = {{"Authorization", "Bearer {{token}}"}}})
```

**`.json_table_key`** / **`.json`**: JSON request body. `.json_table_key` names a context key whose value is serialized; `.json` is a literal JSON string (supports `{{interpolation}}`); each value is JSON-escaped as string contents, so write `"{{title}}"` inside quotes, and a value that arrived as a number or boolean (see [json](#html-markdown-and-json)) can stand bare (`{{count}}`).
```c
http_fetch({.url = "https://api.dev/charge", .context_key = "receipt", .method = http_post, .json_table_key = "order"})
```

**`.text`**: context key sent as the plain-text request body.
```c
http_fetch({.url = "https://api.dev/log", .context_key = "r", .method = http_post, .text = "raw_body"})
```

**`.if_context_key` / `.unless_context_key`** *(per item)*: conditionally include or skip individual requests while running others concurrently.
```c
http_fetch(
  {.url = "https://api.weather.dev/now", .context_key = "weather"},
  {.url = "https://api.quotes.dev/random", .context_key = "quote", .if_context_key = "show_quote"}
)
```

Combined, single request:
```c
http_fetch({.url = "https://api.payments.dev/charge",
  .context_key = "receipt",
  .method = http_post,
  .json_table_key = "order",
  .headers = {
    {"Authorization", "Bearer {{api_key}}"},
    {"Idempotency-Key", "{{order_id}}"}
  }
})
```

Combined, concurrent fan-out:
```c
http_fetch(
  {.url = "https://api.weather.dev/now?city={{city}}", .context_key = "weather"},
  {.url = "https://api.news.dev/headlines?topic={{topic}}", .context_key = "news"},
  {.url = "https://api.quotes.dev/random", .context_key = "quote"}
)
```

#### http_sse

Pushes a Server-Sent Event. With `.channel`, the event broadcasts to all clients on that channel. Without it, the event returns to the requesting client. Each call writes the components it is given straight to the connection, so consecutive calls build one record: `.event` on one, `.data` on the next. See [HTTP Pipelines](#http-pipelines).

**`.channel` *(by order)***: channel to broadcast on; supports `{{interpolation}}`.
```c
http_sse(.channel = "todos", .event = "new_todo", .data = {"{{todo}}"})
```

**`.event`**: SSE `event:` line value.
```c
http_sse(.event = "ping")
```

**`.data`**: array of strings, one per SSE `data:` line (multi-line data). Each supports `{{interpolation}}`; a value containing line breaks continues on further `data:` lines, which the client joins back together. A line break in an interpolated `.event` or `.comment` value raises `err_internal`.
```c
http_sse(.event = "msg", .data = {"line one", "line two"})
```

**`.comment`**: SSE `:` comment line value, useful for keep-alives.
```c
http_sse(.comment = "keep-alive")
```

Combined:
```c
http_sse(.channel = "todos",
  .event = "todo_updated",
  .data = {"id: {{todo_id}}", "title: {{title}}"},
  .comment = "broadcast at {{timestamp}}"
)
```

#### html, markdown, and json

Renders content into the pipeline context. `html()` renders Mustache and `markdown()` renders Markdown-with-Mustache; `json()` serializes a context table to JSON.

**`.template_key` *(by order)***: asset to render; `html()` reads `<key>.html`, `markdown()` reads `<key>.md`.
```c
html(.template_key = "todos", .context_key = "todos_s")
```

**`.context_key` *(by order)***: context key to write the rendered output to.
```c
html(.template_key = "todos", .context_key = "todos_s")
```

Markdown-with-Mustache:
```c
// welcome.md: # Welcome, {{user_name}}
markdown("welcome", "welcome_s")
```

**`json(.table_key, .context_key)`**: `.table_key` *(by order)* is the context table to serialize; `.context_key` *(by order)* receives the JSON string. Values are strings in context, but ones that arrived as numbers or booleans (SQLite `INTEGER`/`REAL` columns, JSON numbers and booleans from `http_fetch()`) are written back as JSON numbers and booleans; everything else, including values set with `set_string()`, is written as a JSON string.
```c
sqlite_query({"todos_db", "get_todos", "todos_data"}),
json(.table_key = "todos_data", .context_key = "todos_j")
```

#### html_template and markdown_template

Declare an HTML or Markdown-with-Mustache template asset in code. Both are declared at module level, like `http()`. `html_template("todo_row", ...)` registers the asset that `todo_row.html` would, and `markdown_template()` the one a `.md` file would, as if the file sat beside the module's `.c` file, so `html()`/`markdown()`, partials, overrides, and the boot check treat it as that file. A declaration and a file with the same key and type in the same folder fail the boot.

**`.template_key` *(by order)***: asset key.
```c
html_template(.template_key = "todo_row",
  .template = "<li>{{title}}</li>"
);
```

**`.template` *(by order)***: template source. It is code: pass a string literal, never a context value.
```c
html_template("todo_row",
  "<li>{{title}}</li>"
);
markdown_template("welcome",
  "# Welcome, {{user_name}}"
);
```

#### http_response

Sends a pipeline context value as the HTTP response.

**`.context_key` *(by order)***: key of the rendered content to send.
```c
http_response(.context_key = "todos_s")
```

**`.status`**: HTTP response status (default `http_ok`). Values: `http_ok` (200), `http_created` (201), `http_redirected` (302), `http_bad_request` (400), `http_not_authorized` (401), `http_not_found` (404), `http_error` (500).
```c
http_response(.context_key = "not_found_s", .status = http_not_found)
```

**`.mime`**: override the response content type. Values: `mime_html`, `mime_text`, `mime_event_stream`, `mime_json`, `mime_javascript`.
```c
http_response(.context_key = "plain_s", .mime = mime_text)
```

Combined:
```c
html("not_found", "not_found_s"),
http_response(.context_key = "not_found_s", .status = http_not_found)
```

#### http_file_response

Sends a stored file as the HTTP response. **`.table_key` *(by order)*** names a one-row table with the columns an upload has: `data` (a blob, or a string) is the body, `type` the content type, and `name` the filename, so no `.mime` is needed. The response carries `X-Content-Type-Options: nosniff` and a `Content-Disposition` naming `name`: `inline` for images (except SVG), `attachment` for everything else, so an uploaded HTML or SVG file downloads instead of running as your site. Who may fetch the file is up to the resource's own steps, such as the `.all` gate here:
```c
http("avatar", "/users/:id/avatar",
  .all = {cookie_logged_in(), validate({"input:id", positive_integer_valid})},
  .get = {
    sqlite_query({"app_db", "get_avatar", "avatar_d", .error_on_empty = true}), // select name, type, data ...
    http_file_response("avatar_d")
  }
);
```
A table straight from an upload works too, so a file can be echoed back without storing it.

#### http_headers and http_cookies

Set HTTP response headers and cookies declaratively; values support `{{interpolation}}`. An interpolated value containing a carriage return, line feed, or NUL raises `err_internal`, so request data cannot add headers.

**`.key` / `.value` *(by order)***: name and value of a header, or of a cookie and its attributes.

`http_headers()` sets one response header per pair:
```c
http_headers({"X-Request-Id", "{{request_id}}"}, {"Cache-Control", "no-store"})
```

`http_cookies()` takes the cookie as the first pair and its attributes as the rest:
```c
http_cookies({"session", "{{sid}}"}, {"HttpOnly"}, {"SameSite", "Lax"}, {"Max-Age", "3600"})
```

Every cookie is signed with the key in `env:cookie_secret`; in dev mode, when it is unset, Nerack generates a random one, and a production build refuses to start without it (see [Configuration](#configuration)). On later requests, a cookie whose signature checks out is put back in context under its own name (`{{sid}}`), alongside the app's other values. Unsigned and tampered cookies are dropped, so a client can neither change a cookie's value nor add a cookie of its own.

#### http_redirect, http_redirect_to, http_redirect_external, and http_reroute

`http_redirect()` returns a 302 to the client, causing the browser to navigate. `http_reroute()` re-enters the router server-side and runs the target resource's `.all` and GET pipeline within the same request. The whole context carries over, and the response keeps the status of the error being handled, so a form re-rendered after a failed `validate()` returns 400. A resource's `.all` runs at most once per request: rerouting to the resource that is already handling the request goes straight to its GET. A request that reroutes too many times (a loop) fails with `err_internal`. Both take only the name of a resource in their own module. To send the visitor to another module's resource, use `http_redirect_to()`: the browser makes that request, so it can cross modules. There is no cross-module reroute, because a reroute runs the target's pipelines in-process. `:params` in the target's URL pattern are read from the current context by matching key names.

**Resource name *(by order)***: target resource name. Required `:params` are read from context by name.
```c
http_redirect("todos") // 302 to /todos
http_redirect("todo") // 302 to /todos/{{id}}, id read from context
http_redirect("org_todo") // 302 to /orgs/{{org}}/todos/{{id}}, org and id read from context
http_reroute("todo") // run the todo GET pipeline in-process, id read from context
```

**`http_redirect_to(module, resource)`**: returns a 302 to a resource in another module. `.module_key` *(by order)* names the module, `.resource_key` *(by order)* the resource; its `:params` are read from context like `http_redirect()`'s.
```c
http_redirect_to("activity", "activity") // 302 to the activity module's /activity
http_redirect_to("cookie_auth", "account") // 302 to the built-in account page
```

**`http_redirect_external(url)`**: returns a 302 to a URL outside the app, such as an identity provider's sign-in page. It is the only step that sends a visitor off-site; `http_redirect()` and `http_redirect_to()` take only resource names. `.url` *(by order)* is interpolated: values after the `?` are percent-encoded, so they cannot add parameters; values before it are inserted as written, so keep request data out of the scheme, host, and path, or the step becomes an open redirect. `.url` also takes `{{url:module:resource}}`, typically for the `redirect_uri` an identity provider sends the visitor back to.
```c
http_redirect_external("{{env:idp_url}}/authorize?client_id={{env:idp_client_id}}&state={{state}}")
```

### rns

Activate with `#include <rns.h>`. It provides the resource/pipeline model for [Reticulum](https://reticulum.network/) destinations: `rns()` declares a resource the same way `http()` does, with the same verb pipelines and the same `.all`/`.errors`/`.repairs` fields. There is no `.mime` and no `.sse`; Reticulum has no MIME negotiation, and delivery is message-based.

Pages are written in Micron, Reticulum's markup, rendered by `micron()`.

**`.name` *(by order)***: resource name, unique among the module's `rns()` resources, used by `{{url:module:resource}}` and `rns_reroute()`.
```c
rns("todos", "/todos", .get = { ... });
```

**`.url` *(by order)***: destination path. Supports `:params`.
```c
rns("todo", "/todos/:id", .get = { ... });
```

**`.all`**: shared steps that run before every verb pipeline on the resource.
```c
rns("todo", "/todos/:id",
  .all = {rns_logged_in(), rns_session(), validate({"input:id", positive_integer_valid, "must be a number"})},
  .get = { ... }
);
```

**`.get` `.post` `.put` `.patch` `.delete`**: verb pipelines, identical in shape to their `http()` counterparts.

**`.errors` / `.repairs`**: resource-scoped error and repair pipelines. See [Error and Repair Pipelines](#error-and-repair-pipelines).

**Request data.** Each request puts these keys in context:

* `input:<field>`: each Micron link field (`|title`, `|finished=1`) and each URL `:param`, the counterparts of HTTP's form fields.
* `server:method`, `server:path`, `server:url`: the request method (from `{{method:verb}}`, else GET), path, and full URL.

Reticulum has no headers and no IP addresses, so there is no `client:` scope and no `server:client_ip`. The peer's identity reaches the app only through [rns_auth](#rns_auth), as `user_id`.

To know who is calling, add [rns_auth](#rns_auth): `rns_logged_in()` gates a resource and `rns_session()` loads `user`, for the same users as [cookie_auth](#cookie_auth).

**`micron(template_key, context_key)`**: renders a Micron template asset into context, the way `html()` renders Mustache. Micron assets are ordinary files (`micron("todos", ...)` reads `todos.mu`), so Mustache interpolation and sections work inside them; `{{x}}` escapes Micron markup, so a value cannot inject formatting or links.

**`micron_template(template_key, template)`**: declares a Micron template asset in code, at module level, like `rns()`: `micron_template("todo_row", ...)` registers the asset that `todo_row.mu` would, as if the file sat beside the module's `.c` file. See [text_template](#text_template).
```c
micron_template("todo_row",
  "`[{{title}}`{{url:todos:todo}}]"
);
```

**`rns_response(context_key)`**: sends a context value back to the requesting peer.

**`{{url:module:resource}}`** and **`{{method:verb}}`**: resolve `rns()` resources too. `{{url:module:resource}}` gives the destination path. Micron links carry no request method, so every non-GET verb, `post` included, is selected with `{{method:verb}}`, which `micron()` renders as a `_method=<verb>` link field, `|`-separated from any others:
```
`[add`{{url:todos:todos}}`{{method:post}}|title]
`[done`{{url:todos:todo}}`{{method:patch}}|finished=1]
```

**`rns_reroute(name)`**: re-enters the router in-process and runs the target resource's `.all` and GET pipeline, like `http_reroute()`, and likewise only within its own module. `:params` are read from context by matching key names.

Combined:
```c
#include <nerack.h>
#include <rns.h>
#include <rns_auth.h>
#include <sqlite.h>

module(){
  rns("todos", "/todos",
    .all = {rns_logged_in(), rns_session()},
    .get = {
      sqlite_query({"todos_db", "get_todos", "todos_data", .error_on_empty = true}),
      micron("todos", "todos_s"),
      rns_response("todos_s")
    },
    .post = {
      validate({"input:title", not_empty_valid, "cannot be empty"}),
      sqlite_query({"todos_db", "create_todo"}),
      rns_reroute("todos")
    },
    .errors = {
      {err_not_found, {micron("no_todos", "no_todos_s"), rns_response("no_todos_s")}}
    }
  );
}
```

The same module can serve both protocols: declare `http(...)` and `rns(...)` resources side by side, sharing queries, routines, and event subscriptions, and render Mustache for one and Micron for the other. Resource names are per protocol, so a module serving the same path over both can give both resources the same name: `{{url:todos:home}}` reaches the `http()` one from an HTML template and the `rns()` one from a Micron template.

## Bundled Modules

Beyond the [protocols](#protocols) and the SQLite [engine](#engines), these modules are bundled. Activate each one by `#include`ing its header.

* [htmx](#htmx)
* [datastar](#datastar)
* [tailwind](#tailwind)
* [daisyui](#daisyui)
* [auth](#auth)
* [cookie_auth](#cookie_auth)
* [rns_auth](#rns_auth)

### htmx

Activate with `#include <htmx.h>`. It serves the htmx runtime as the `{{> htmx }}` partial and sets the `is_htmx` context flag on requests carrying the `HX-Request` header. Pair the flag with `.if_context_key`/`.unless_context_key` to return a fragment to htmx and a full page to a direct visit, or use `hx-boost` to upgrade ordinary links and forms into AJAX swaps.

```c
#include <nerack.h>
#include <http.h>
#include <htmx.h>

module(){
  http("todos", "/todos",
    .get = {
      sqlite_query({"todos_db", "get_todos", "todos_data"}),
      html("todos_fragment", "frag_s", .if_context_key = "is_htmx"),
      http_response("frag_s", .if_context_key = "is_htmx"),
      html("todos_page", "page_s", .unless_context_key = "is_htmx"),
      http_response("page_s", .unless_context_key = "is_htmx")
    }
  );
}
```

Include the runtime once in the page `<head>`:
```html
<head>{{> htmx }}</head>
<body hx-boost='true'>...</body>
```

### datastar

Activate with `#include <datastar.h>`. It serves the Datastar runtime as the `{{> datastar }}` partial and provides `datastar()` for pushing reactive fragment and signal patches over an SSE channel. A page opens an SSE connection (a resource `.sse` channel); pipelines push patches to that channel, and Datastar applies them in the DOM.

`datastar()` patches a context value into the page by target (element id or CSS selector). The first value is the channel (supports `{{interpolation}}`).

**`.channel` *(by order)***: channel to push to. Omit it to patch only the client that sent the current Datastar request: the patch goes back as that request's response, the way `http_sse()` without a channel answers the requester. Use this for feedback meant for one tab, such as a validation error.
```c
html("todo_row", "todo_row_s"),
datastar(.channel = "todos", .target = "#todo-list", .mode = append_mode, .elements = "todo_row_s")
```
```c
.errors = {
  {http_bad_request, {
    html("todo_error", "todo_error_s"), // <p id="todo_error">{{error_message:title}}</p>
    datastar(.target = "#todo_error", .elements = "todo_error_s")
  }}
}
```

**`.target`**: element id or CSS selector for the element to patch; supports `{{interpolation}}`.
```c
.target = "#todo-{{id}}"
```

**`.mode`**: how the rendered fragment is applied to the target (a `datastar_mode`). Default `outer_mode`.
```c
.mode = replace_mode
```

**`.elements`**: context key holding the rendered HTML fragment to patch in. Not required for `remove_mode`.
```c
.elements = "todo_row_s"
```

**`.signals`**: context key of a one-row table whose fields are merged into the client's signals, serialized like [`json()`](#html-markdown-and-json) (numbers and booleans stay typed, nested tables become nested objects).
```c
.signals = "ui_state"
```

**`.javascript`**: JavaScript to execute on the client.
```c
.javascript = "window.scrollTo(0, document.body.scrollHeight)"
```

**Patch modes (`datastar_mode`):** `outer_mode`, `inner_mode`, `replace_mode`, `prepend_mode`, `append_mode`, `before_mode`, `after_mode`, `remove_mode`.

Example: a POST inserts a row, gets it back with `RETURNING`, and appends it to every connected client's list:

```c
http("todos", "/todos",
  // Each browser opens this channel and listens for patches.
  .sse = {"todos"},

  .post = {
    validate({"input:title", not_empty_valid}),
    // RETURNING gives the new row back; capture it under "todo".
    sqlite_query({"todos_db", "insert_todo", "todo", .error_on_empty = true}),
    // Render the new row, then patch it into the list for everyone on the channel.
    html("todo_row", "todo_row_s"),
    datastar(.channel = "todos",
      .target = "#todo-list",
      .mode = append_mode,
      .elements = "todo_row_s"
    )
  }
);
```

Removing an element needs only a selector:
```c
datastar(.channel = "todos", .target = "#todo-{{id}}", .mode = remove_mode)
```

Datastar sets the `is_datastar` context flag on requests it originates, usable with `.if_context_key`/`.unless_context_key`. The signals a Datastar request sends (a JSON body, or the `datastar` query parameter on GET) land in `input:` scope, so `validate()` validates them like form fields.

Include the runtime once in the page `<head>`:
```html
<head>{{> datastar }}</head>
```

### tailwind

Activate with `#include <tailwind.h>`. It compiles the Tailwind utility classes used across the project's templates and serves the stylesheet as the `{{> tailwind }}` partial. Use Tailwind classes directly in templates; there is no build step or config file.

```html
<head>{{> tailwind }}</head>
<body class='bg-gray-950 text-white min-h-screen'>
  <h1 class='text-3xl font-bold text-center mb-8'>Vote for which is roundest</h1>
</body>
```

### daisyui

Activate with `#include <daisyui.h>`. It compiles the DaisyUI classes used across the project's templates and serves the stylesheet as the `{{> daisyui }}` partial. Use DaisyUI classes directly in templates; there is no build step or config file.

```html
<head>{{> daisyui }}</head>
<body>
  <button class="btn">Click Me</button>
</body>
```

### auth

The user store behind [cookie_auth](#cookie_auth) and [rns_auth](#rns_auth); each includes it, so apps never include `auth.h` themselves. It owns the users database: each user's id, email (unique), short name, and salted password hash, plus pending link codes. Credentials that belong to one protocol live with that protocol's module, keyed by `user_id`: HTTP sessions in [cookie_auth](#cookie_auth), bound Reticulum identities in [rns_auth](#rns_auth). Apps never query any of these databases: an app knows the current user only through `user_id` and the `user` record (`id`, `email`, `short_name`) that the front-end modules load.

**`auth_user()`**: finds or creates the user for a verified `email` in context, and puts its id in `user_id`. A new user takes `short_name` from context, and has no password, so it can sign in only through the credential that created it (or one linked later). It trusts `email` completely: use it only after an external check, such as an identity provider's verified response, never on a form value. Pair it with `cookie_sign_in()` or `rns_sign_in()`, typically in an [override of cookie_auth](#signing-in-through-sso) that signs users in through an organization's SSO.
```c
auth_user(),
cookie_sign_in()
```

**Events.** `auth` publishes these, each carrying `user_id`; the front ends subscribe to them, and so can apps (`#include <pubsub.h>`):
* `user_created`: a user signed up or was created by `auth_user()`.
* `user_credentials_changed`: the user's password changed. `cookie_auth` deletes the user's old sessions (they already stopped working; see Sessions under [cookie_auth](#cookie_auth)).
* `user_deleted`: the user is gone. `cookie_auth` deletes their sessions and `rns_auth` unbinds their identities; an app deletes the data it keyed by `user_id`.
```c
subscribe("user_deleted", {
  sqlite_query({"todos_db", "delete_user_todos"}) // delete from todos where user_id = {{user_id}}
});
```
Delivery is durable but asynchronous: subscribers run shortly after the change, never inside the request that made it. Anything that must take effect immediately, such as revoking sessions, does not wait on an event.

Because both front ends share it, a user is one user on every protocol: sign up over HTTP, link a Reticulum identity, and `user_id` is the same in `http()` and `rns()` pipelines, so app data keyed by `user_id` follows the user across protocols.

**Database.** `auth` keeps its users in a SQLite database under the key `auth_db`. To move them, for example to a Postgres database shared by several Nerack processes, override the module and declare `auth_db` with another engine (see [Modules and Composition](#modules-and-composition)). The override's migrations, in its folder, create the same tables as the bundled ones, in that engine's dialect. `cookie_auth`'s sessions (`cookie_auth_db`) and `rns_auth`'s identities (`rns_auth_db`) move the same way.
```
└── overrides/
    └── auth/
        ├── module.c # module(){ postgres("auth_db", ...); }
        └── create_users_table.sql
```
```c
// overrides/auth/module.c
#include <nerack.h>
#include <postgres.h>

module(){
  postgres("auth_db", "{{env:auth_database_url}}", {"create_users_table", "create_link_codes_table"});
}
```

### cookie_auth

Activate with `#include <cookie_auth.h>`. It provides cookie-based authentication for HTTP as pipeline steps, over the users in [auth](#auth). The module is self-contained (per [SCS](./README.md#philosophy)): including the header mounts the `/login`, `/logout`, `/signup`, and `/account` resources and ships their pages, so `{{url:cookie_auth:login}}`, `{{url:cookie_auth:logout}}`, and `{{url:cookie_auth:account}}` resolve with no app code. `cookie_logged_in()` and `cookie_session()` are ordinary steps, independent of each other; place them in `.all` or a verb pipeline, alone or together, as the resource needs.

Users live in [auth](#auth), which apps never query: an app knows the current user only through `user_id` and the `user` record. Signup takes an email (unique; a duplicate re-renders the form with an error), a password, and a short name, each checked with [validate](#validate). Passwords are stored only as salted hashes.

Ownership is the app's job. Scope each query to the current user and let `.error_on_empty` turn another user's row into a 404:
```sql
-- delete_todo.sql
delete from todos where id = {{id}} and user_id = {{user_id}}
```
```c
sqlite_query({"todos_db", "delete_todo", .error_on_empty = true})
```

**Sessions.** Signing in creates a session, stored in the module's own database, and sets a signed, `HttpOnly` session cookie holding its random id; the cookie never carries the user id itself. A session expires after `env:session_days` (default 30) without use, renewed as it is used (at most once a day), and after `env:session_max_days` (default 90) however active, which forces a fresh login. Each device signs in to its own session. Logging out deletes that session on the server, so a copied cookie stops working too. A password change ends every other session at once: each session check also rejects a session started before the user's last credential change, and the request that changed the password gets a fresh session. Deleting the user ends all their sessions.

**`cookie_logged_in()`**: checks for a live session. If there is none (no cookie, or an expired or deleted session), redirects to the login page, which authenticates the visitor, starts a session, and redirects back. When there is one, it puts the session's `user_id` in context, so later steps can interpolate `{{user_id}}` in SQL and connection strings. It does not load the user record; add `cookie_session()` when the pipeline also needs `user`.
```c
http("todos", "/todos", .all = {cookie_logged_in(), cookie_session()}, .get = { ... });
```

**`cookie_session()`**: when there is a live session, puts its `user_id` in context and loads that user's record as `user`, with `id`, `email`, and `short_name` (never the password hash). Use it wherever pipelines or templates read the current user. On a public page such as an about or landing page, use it alone with no gate: the header can then show the user when signed in, a sign-in link otherwise.
```c
http("about", "/about", .all = {cookie_session()}, .get = { ... });
```
```html
{{#user}}Signed in as {{short_name}} · <form method='post' action='{{url:cookie_auth:logout}}'><button>log out</button></form>{{/user}}
{{^user}}<a href='{{url:cookie_auth:login}}'>sign in</a>{{/user}}
```

**`cookie_link()`**: issues a one-time link code for the signed-in user into `link_code`. A code works once and expires after 10 minutes. The built-in `/account` page uses it so a user can link a Reticulum identity: entering the code on the [rns_auth](#rns_auth) link page, while identified, binds that identity to this user. The code proves the user holds both the HTTP session and the identity key; an identity hash typed into a form would not.

**`cookie_sign_in()`**: signs in the user whose id is in `user_id`: starts a session, sets its cookie, and returns the visitor to the page that sent them to sign in (or `/` when there is none), as a successful `cookie_login()` does after its password check. Use it after `auth_user()` when a user is verified some other way; it ends the pipeline, so no redirect step follows it.

**`cookie_login()` / `cookie_logout()` / `cookie_signup()` / `cookie_link()`**: the actions behind the module's built-in resources. Change them by overriding the module (see [Modules and Composition](#modules-and-composition)): `overrides/cookie_auth/module.c`. To change only how a built-in page looks, add `login.html`, `signup.html`, or `account.html` to that folder. To change a resource's URL or steps, declare it there under the same name (`login`, `logout`, `signup`, `account`), calling the matching action. A replacement that leaves out the action no longer does that job; `cookie_logged_in()` still redirects to whatever `login` is.
```c
// overrides/cookie_auth/module.c
module(){
  http("login", "/login",
    .get = {
      html("login", "login_s"),
      http_response("login_s")
    },
    .post = {cookie_login()}
  );
  http("logout", "/logout", .post = {cookie_logout()});
  http("signup", "/signup",
    .get = {
      html("signup", "signup_s"),
      http_response("signup_s")
    },
    .post = {cookie_signup()}
  );
  http("account", "/account",
    .all  = {cookie_logged_in(), cookie_session()},
    .get  = {html("account", "account_s"), http_response("account_s")},
    .post = {cookie_link(), html("account", "account_s"), http_response("account_s")}
  );
}
```

Combined: gate the resources that need auth, then read user fields in that module's templates.
```c
// todos/module.c
#include <nerack.h>
#include <http.h>
#include <cookie_auth.h>

module(){
  http("todos", "/todos",
    .all = {cookie_logged_in(), cookie_session()},
    .get = {
      html("todos", "todos_s"),
      http_response("todos_s")
    }
  );
}
```
```html
<!-- once cookie_session() has loaded the user, templates can read it -->
{{#user}}
  <span>Hi, {{short_name}}</span>
{{/user}}

<!-- logout changes state, so it is a POST form, not a link -->
<form method='post' action='{{url:cookie_auth:logout}}'><button>Log out</button></form>
```

#### Signing in through SSO

To sign users in through an organization's identity provider, keep [auth](#auth)'s users and cookie_auth's sessions and change only how users sign in: [override](#modules-and-composition) cookie_auth's `login`, and add a module for the provider's callback.

```
├── overrides/
│   └── cookie_auth/
│       └── module.c # login sends the visitor to the identity provider
└── sso/
    └── module.c # the identity provider's callback
```

```c
// overrides/cookie_auth/module.c
#include <nerack.h>
#include <http.h>

module(){
  // cookie_logged_in() redirects to login, which now sends the visitor to the identity provider
  http("login", "/login",
    .get = {
      run(.call = new_sso_state), // sets state to a random token
      http_cookies({"sso_state", "{{state}}"}, {"HttpOnly"}, {"Max-Age", "600"}),
      http_redirect_external("{{env:idp_url}}/authorize?client_id={{env:idp_client_id}}"
        "&redirect_uri={{env:app_url}}{{url:sso:callback}}&state={{state}}")
    }
  );
}
```

```c
// sso/module.c
#include <nerack.h>
#include <http.h>
#include <cookie_auth.h>

module(){
  http("callback", "/sso/callback",
    .get = {
      validate({"input:code", token_valid}, {"input:state", token_valid}),
      http_fetch({http_post, "{{env:idp_url}}/token", "idp_user", .json = "{\"code\":\"{{code}}\"}"}),
      run(.call = read_verified_claims), // checks state against the sso_state cookie, sets email and short_name from the verified response
      auth_user(),                       // find or create the user, set user_id
      cookie_sign_in()                   // set the session cookie, return to the page that needed sign-in
    }
  );
}
```
`cookie_session()`, `{{url:cookie_auth:logout}}`, the `/account` page, Reticulum identity linking, and app data keyed by `user_id` keep working unchanged.

### rns_auth

Activate with `#include <rns_auth.h>`. It provides authentication for [rns](#rns) over the same users as [cookie_auth](#cookie_auth), stored in [auth](#auth); the identities bound to them live in this module's own database. A Reticulum identity is a credential bound to a user, like a passkey: the peer proves it holds the identity by identifying over its link, and the identity must be bound to a registered user before it signs anyone in. Including the header mounts the `login`, `signup`, and `link` resources (at `/login`, `/signup`, `/link`; `{{url:rns_auth:login}}` and so on) and ships their Micron pages.

An identified peer binds its identity in one of three ways:
* **Sign up** over RNS (email, password, short name): creates the user and binds the identity.
* **Log in** over RNS with email and password: binds the identity to an existing user, such as one who signed up over HTTP.
* **Link** with a code from the HTTP `/account` page ([`cookie_link()`](#cookie_auth)): binds without sending a password over RNS.

An identity binds to one user; a user can bind several identities. There is no logout: identification is per link, so a peer signs out by not identifying.

**`rns_logged_in()`**: passes when the peer has identified and its identity is bound to a user, and puts `user_id` in context. Otherwise it responds with the built-in login page in place of the rest of the pipeline: an anonymous peer is asked to identify and retry; an identified but unbound peer can log in, sign up, or enter a link code, then returns to the page it asked for. It does not load the user record; add `rns_session()` when the pipeline also needs `user`.
```c
rns("todos", "/todos", .all = {rns_logged_in(), rns_session()}, .get = { ... });
```

**`rns_session()`**: when the peer's identity is bound, loads `user` (`id`, `email`, `short_name`, the same record as `cookie_session()`). It never fails: an anonymous or unbound peer has no `user`. Use it alone on public pages.
```c
rns("home", "/", .get = {rns_session(), micron("home", "home_s"), rns_response("home_s")});
```
```
{{#user}}welcome, {{short_name}}{{/user}}{{^user}}`[sign in`{{url:rns_auth:login}}]{{/user}}
```

**`rns_sign_in()`**: binds the identified peer's identity to the user in `user_id`, so this and later requests from that identity are signed in, then returns the peer to the page it asked for (or `/` when there is none), as `rns_login()` does. Use it after `auth_user()` when a user is verified some other way; the peer must have identified. It ends the pipeline, so no reroute step follows it.

**`rns_login()` / `rns_signup()` / `rns_link()`**: the actions behind the module's built-in resources. Override them the same way as `cookie_auth`'s, in `overrides/rns_auth/module.c`: add `login.mu`, `signup.mu`, or `link.mu` to restyle a page, or declare `login`, `signup`, or `link` to change a resource, calling the matching action.
```c
// overrides/rns_auth/module.c
module(){
  rns("login", "/login",
    .get  = {micron("login", "login_s"), rns_response("login_s")},
    .post = {rns_login()}
  );
  rns("signup", "/signup",
    .get  = {micron("signup", "signup_s"), rns_response("signup_s")},
    .post = {rns_signup()}
  );
  rns("link", "/link",
    .get  = {micron("link", "link_s"), rns_response("link_s")},
    .post = {rns_link()}
  );
}
```

## Static Files

Files in `public/` are served directly. Reference them in templates with `{{asset:filename}}`, which resolves to a content-checksummed, cache-busted URL with immutable cache headers. Updates invalidate caches automatically; unchanged files cache forever. In dev they are read from disk and reload on change; a production build embeds them in the binary, so `public/` is fixed at build time and nothing is written to it at runtime (store uploads in a database).

```
public/
├── favicon.png
├── styles.css
└── logo.svg
```

```html
<link rel='icon' href='{{asset:favicon.png}}'>
<link rel='stylesheet' href='{{asset:styles.css}}'>
<img src='{{asset:logo.svg}}' alt='Logo'>
```

This differs from assets like SQL and HTML templates, which are embedded from the root and the module folders and read by steps by key (see [Assets](#assets)). `public/` holds opaque files served to the browser.

## Configuration

Nerack reads its settings from environment variables, and every non-secret one also from a command-line flag (lowercase, `_` as `-`: `PORT` → `--port`); a flag wins over its variable. Secrets come only from the environment, so they never show up in process listings. Every environment variable, Nerack's or the app's own, is also in context as `env:<lowercased name>` (`COOKIE_SECRET` → `env:cookie_secret`, `IDP_URL` → `env:idp_url`).

| Variable | Flag | Default | Purpose |
|---|---|---|---|
| `PORT` | `--port` | `3000` | HTTP listening port. |
| `HTTP_REACTOR_COUNT` | `--http-reactor-count` | CPU cores | HTTP reactor threads, all on the same port; one process uses the whole machine. |
| `WORKER_COUNT` | `--worker-count` | CPU cores | Threads for `run_worker()`, database calls, and other blocking work. |
| `DATA_DIR` | `--data-dir` | working directory | Base for relative SQLite paths (`file:todo.db`), including Nerack's own databases. |
| `COOKIE_SECRET` | none | dev: random each start | Key that signs every cookie. A production build of an app that uses `http.h` refuses to start without it. |
| `TRUSTED_PROXIES` | `--trusted-proxies` | none | Comma-separated addresses and CIDR ranges whose `X-Forwarded-For` is trusted for `server:client_ip`. |
| `MAX_UPLOAD_BYTES` | `--max-upload-bytes` | `10485760` (10 MB) | Largest accepted file upload. |
| `SESSION_DAYS` | `--session-days` | `30` | Days a [cookie_auth](#cookie_auth) session lasts without use. |
| `SESSION_MAX_DAYS` | `--session-max-days` | `90` | Days a session lasts however active. |
| `OTEL_EXPORTER_OTLP_ENDPOINT` and other `OTEL_*` | `--otel-endpoint` | `http://localhost:4318` | Where OpenTelemetry data goes; the standard `OTEL_*` variables apply, and `OTEL_SERVICE_NAME` defaults to the project folder's name. |

## External Dependencies

Third-party libraries go in `vendor/`, prebuilt, and so do packages exported by other Nerack apps (see [Exporting](#exporting)): a header for your code, plus the library twice, `<name>.so` for dev and `<name>.a` for production builds, both compiled for x86_64 Linux with musl. Put them flat in `vendor/` or in a folder each, as many as the app needs. Nerack links every one into the app, in dev and in production builds, whether or not your code calls it, so remove a library from `vendor/` to leave it out; you include its header by name (`<cmark.h>`). `vendor/` holds no source files. A library's license files (`LICENSE`, `COPYING`, `NOTICE`) in `vendor/` are added to the production build's third-party notices. Call into libraries from `run()`/`run_worker()` steps. Memory returned by a library that must be freed manually is registered with `defer_free()` so it is reclaimed when the run ends (see [memory](#memory)).

```
vendor/
├── cmark.h
├── cmark.so
├── cmark.a
└── yaml/
    ├── yaml.h
    ├── yaml.so
    └── yaml.a
```

```c
#include <nerack.h>
#include <cmark.h>

// plain C, called as a step with run_worker(.call = render_markdown)
void render_markdown(){
  string md = get("markdown");
  char *out = cmark_markdown_to_html(md, strlen(md), 0);
  defer_free(out); // library-owned pointer
  set_string("html_content", out);
}
```

Wire it into a pipeline as a `run_worker()` step, off the request reactor:
```c
run_worker(.call = render_markdown)
```

A library installed on the system rather than in `vendor/` is named with a command-line flag or environment variable, and Nerack links it with the app the same way.

### Your own C code

Plain C sits beside the code that uses it. A `.c` file at the root, next to `app.c`, is a root library: the app module and every other module may call it, through a header beside it that any file includes with quotes, `#include "pricing.h"`. A `.c` file in a module's folder, next to its `module.c`, is that module's own: only the module's files may call it, and they include its header the same way, `#include "format.h"`. A module's folder comes first, so a module's `format.h` wins over a root `format.h` for that module's files. Project headers are found only with quotes, as in any C project; angle brackets are for Nerack, the C library, and `vendor/` (`<nerack.h>`, `<string.h>`, `<greet.h>`), so a project header never hides one of those, whatever its name.

```
my_app/
├── app.c # includes "pricing.h"
├── pricing.c # plain C every module may call
├── pricing.h
├── orders/
│   ├── module.c # module(){...}, includes "pricing.h"
│   ├── format.c # plain C only orders may call
│   └── format.h
└── billing/
    └── module.c # module(){...}, includes "pricing.h"
```

Only `app.c` and a `module.c` declare `module()`; every other `.c` file registers nothing, it is plain C. Every file may use `nerack.h` (`alloc`, `defer_free`, the context helpers). Modules work with each other through resources, events, and context, never by calling each other's C, so a module's code that another module needs goes into a root library. Every build checks what each file calls: a module calling another module's code, a root library calling into a module, or any file outside the app module calling into `app.c` fails, naming both files. Global functions and variables share one namespace, as in any C program, so two files defining the same one fail dev mode and the production build alike, even in different modules; a helper only its own file uses is `static`. In dev, saving a `.c` file reloads it and the files that call into it, nothing else, and saving a header recompiles the files that include it. Every file compiles with the same warnings and hardening (`-Wall -Wextra`, stack protection, zero-initialized locals, control-flow protection). Dev mode reports warnings and runs anyway; `nerack build` treats them as errors. Two are errors in dev too: a format string that is not a literal, and too many initializers, such as a step list longer than 16 steps, which C would otherwise cut short with only a warning. Every file compiles as C23 by default; a file picks another standard in its name, before `.c`: `words.c11.c` compiles as C11 (any of `c89`, `c99`, `c11`, `c17`, `c23`, and their `gnu` forms, such as `gnu11`).

### Exporting

A module whose folder holds an `export.h` is also exported: `nerack build` writes it into `output/` as a package that another app puts in its `vendor/`, with `export.h` as the package's `bar.h`. The module stays part of its own app as before. `export.h` is the module's public interface, the functions and step macros the using app may call; the module's other headers stay private. The module's own files include it like any of their headers, `#include "export.h"`.

```
my_app/                     output/
├── app.c                   ├── app
├── fiz.c                   ├── bar.so
├── fiz.h                   ├── bar.a
└── bar/                    ├── bar.h
    ├── module.c            └── LICENSE.bar
    ├── export.h
    ├── quad.c
    ├── bar.html
    └── LICENSE
```

Copy a package's files into the other app's `vendor/`. Its code includes `<bar.h>`, and Nerack links the package like any other vendor library: `bar.so` in dev (built with debug info, so `nerack debug` steps into it), `bar.a` in production builds, `LICENSE.bar` into the third-party notices.

A package holds the module's own files and every root library they use (`fiz.c` above), so it works in an app that has none of them. Only what `export.h` declares is visible to the app that uses it; everything else in the package stays inside it, so a root library the package carries never clashes with one of the using app's own, even of the same name. A carried root library's global variables belong to the package's copy: when the module and the exporting app's other code share state through one, they no longer do in the using app, and `nerack build` warns about each such variable.

An exported module is a vendor module in the app that uses it. It keeps its name, and its folder's assets travel inside the package. The app's assets at the root do not: in the using app, the module sees its own assets and the bundled and vendor modules', never that app's root. When its templates use an asset of the exporting app's root, such as `{{< layout}}`, the using app provides one in `overrides/bar/`; a missing partial fails the render with its name and where it was looked for. Document what a module expects. The using app can change anything else from `overrides/bar/` too, like a bundled module (see [Modules and Composition](#modules-and-composition)).

A package carries what it needs, checked by the app that uses it:

* Its `export.h`, which the using app includes as `<name>.h`, must compile on its own. It may include Nerack headers, C library headers, and vendor headers, nothing from the exporting app (not a root header, not another header in the module's folder).
* A package that includes Nerack headers works only with a Nerack whose headers match; after upgrading Nerack, build the exporting app again.
* The vendor libraries and packages it uses must be in the using app's `vendor/` too; a missing one fails with its name. The bundled modules it includes are linked for it, so the using app need not include them.

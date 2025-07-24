# Resources, assets, templates, and errors

## Resources

- `http()` declares a named resource. Names are unique per module and protocol (`http("room")` and `rns("room")` can coexist, and two modules can each have `home`); URL patterns are unique per protocol app-wide. An exact path beats a parameterized one. `.mime` defaults to `mime_html`. The verb is the request method, or a `_method` field on a POST.
- `.sse = {"channel:{{interp}}", {steps}}` opens a persistent SSE channel; browsers connect with `{{sse:module:resource}}`. The steps run on connect, after `.all`; `.sse = {"room"}` just subscribes.
- `rns()` is `http()` without `.mime` and `.sse`; pages are Micron. Link fields (`|title`) and `:params` arrive as `input:<name>`; `server:method|path|url` are set; there is no `client:` scope or `server:client_ip`. The peer's identity reaches the app only through `rns_auth.h`.
- Non-browser clients (API clients, Hurl) must send an `Origin` matching the app on POST/PUT/PATCH/DELETE, or the CSRF check rejects them with `http_forbidden` (403).

## Assets

- Asset types come from included modules: `.txt` and `.json` (core), `.html`/`.md` (`http.h`), `.mu` (`rns.h`), `.sql` (`sql.h`, included by every SQL engine). No other file is an asset. The key is the name before the first dot (`get_todos.sql` → `get_todos`); each step reads only its own type, so one key can name a file of each type. `{{> partial}}`/`{{< parent}}` resolve in the type of the enclosing template.
- A `<key>.json` asset is a context value, the table form of `context()`: every pipeline of a module that sees it (its own folder, then the root, like any asset) starts with `<key>` set. An array of objects is a table (columns in the first row's key order, every row the same keys; numbers and booleans become text, `null` empty), a JSON string a string; anything else fails the boot. Never rendered. A step that sets the same key never runs (a dev warning, a `nerack build` error), so use it for fixed data no step produces, and for test mocks (`nerack-cli`). A task's or subscriber's `.accepts` key cannot have one.
- A module sees its own folder, then the root, then the bundled and vendor modules it includes; the app module sees the root, then those. No module sees another module's folder. A bundled or vendor module sees only its own; give it or replace an asset from `overrides/<name>/`, whose templates also see the root.
- `*_template(key, source)` declares an asset in code, exactly as if `<key>.<type>` sat beside the declaring `.c`. A declaration and a file with the same key and type in one folder fail the boot.
- The scan reads the root's files, `modules/`, and `overrides/`; it never enters dot-folders or any other root folder. A `.nerackignore` (`.gitignore` syntax) in any folder skips more or re-includes with `!`; the root one holds a block between `# >>> nerack` and `# <<< nerack` that `nerack pin` rewrites for its version, skipping docs such as `README*`, `LICENSE*`, `CLAUDE*`, `AGENTS*`. Never edit the block; add the project's lines below it (`!README.md` brings one back).
- `public/` files are served as-is; link them with `{{public:file}}` (cache-busted).

## Templates

Mustache, escaped for the output (HTML in `html`/`markdown`, Micron in `micron`; `text` does not escape). The base spec except dot notation: `{{x}}` · `{{{x}}}`/`{{&x}}` raw · `{{#x}}..{{/x}}` section or loop · `{{^x}}..{{/x}}` inverted · `{{! c }}` · `{{=<% %>=}}` · `{{> name}}` partial · `{{< parent}}{{$block}}override{{/block}}{{/parent}}` inheritance (any template with `{{$block}}default{{/block}}` is a layout). A missing key, `nullptr`, opaque value, or blob renders empty; a missing partial fails the render.

No logic in templates: the query returns the flags a page tests, and the template only tests and loops.
```sql
select id, title, owner_id = {{user_id}} as is_owner, due_at < unixepoch() as is_overdue
from todos
```
```html
{{#todos}}<li {{#is_overdue}}class='overdue'{{/is_overdue}}>{{title}} {{#is_owner}}<button>delete</button>{{/is_owner}}</li>{{/todos}}
{{^todos}}<p>No todos yet</p>{{/todos}}
```

Helpers, colon-separated, each argument a literal or context key. Missing or unusable arguments (unset key, non-number to `precision:`, `url:` missing a `:param`) render empty and are logged; `input:` and the `error` helpers render empty/false silently when there is no such parameter or error:
- `{{url:module:resource}}`: a resource's URL, always naming the module, even your own (`{{url:todos:todos}}`, `{{url:cookie_auth:login}}`). `:params` fill from the current scope, so it works per row in a section; extra `:key=value` arguments become query parameters (`{{url:todos:todos:filter=done}}`), and a bare `:key` takes its value from scope (`{{url:todos:todos:filter}}`). Resolves `http()` resources in `html`/`markdown`, `rns()` in `micron`; fails the boot in `text`, SQL, and `context()`.
- `{{sse:module:resource}}`: the URL that connects to `.sse` (a GET with `_method=sse`).
- `{{method:verb}}`: a hidden `_method` field so a POST form reaches `put`/`patch`/`delete`. In Micron it is a link field, needed for every non-GET verb, `post` included: `` `[add`{{url:todos:todos}}`{{method:post}}|title] ``. Datastar and htmx send real methods.
- `{{input:field}}` raw parameter (repopulate forms) · `{{secret:name}}` a secret, the only way to read one (outbound headers, connection strings; never in pages) · `{{error:field}}` truthy on error · `{{error_message:field}}` · `{{error_code:field}}` · `{{precision:field:N}}` · `{{public:file}}`.
- `{{date:field:format_key}}`: Unix seconds → UTC string, strftime pattern from context key `format_key` (never a literal; set it with `context("day_format", "%b %d, %Y")`); English names; a non-integer is unusable. Prefer SQL returning the formatted string (`strftime('%Y-%m-%d', created_at, 'unixepoch')`); use `date:` only when a step needs the raw timestamp. Relative dates ("2 days ago") are computed in `run()` as plain strings.
- Datastar attributes: `data-bind:title`, `data-on:click="@post('{{url:todos:todos}}')"`, `data-init="@get('{{sse:todos:todos}}')"`.

A layout is any template with `{{$block}}default{{/block}}`; a page fills it with `{{< layout}}{{$block}}...{{/block}}{{/layout}}`. A page fills only the blocks it names (the rest keep their defaults), so an empty `{{$head}}{{/head}}` lets one page add its own stylesheet or script. Put the layout at the root to share it with every module, and load the bundled runtimes in its `<head>`:
```html
<!-- layout.html -->
<!doctype html>
<html>
  <head>
    <title>{{$title}}Todos{{/title}}</title>
    {{> datastar}}
    {{> tailwind}}
    {{$head}}{{/head}}
  </head>
  <body>
    {{#user}}{{short_name}} <form method='post' action='{{url:cookie_auth:logout}}'><button>log out</button></form>{{/user}}
    {{^user}}<a href='{{url:cookie_auth:login}}'>sign in</a>{{/user}}
    {{$body}}{{/body}}
  </body>
</html>

<!-- modules/todos/todos.html -->
{{< layout}}
  {{$title}}My todos{{/title}}
  {{$head}}<link rel='stylesheet' href='{{public:todos.css}}'>{{/head}}
  {{$body}}<ul>...</ul>{{/body}}
{{/layout}}
```

## Micron

`micron()` renders `.mu` assets, Reticulum's page markup, line by line; Mustache works inside, and `{{x}}` escapes Micron, so a value cannot add formatting, links, or fields. A layout works the same way (`layout.mu` with `{{$body}}{{/body}}`).
- Lines: `>Title`, `>>Section`, `>>>Subsection` headings (text below a heading is indented under it); `-` alone is a divider; a line starting with `#` is a comment, never sent.
- Inline, toggled by a backtick: `` `! `` bold, `` `* `` italic, `` `_ `` underline, two backticks in a row reset all; `` `F0af `` / `` `f `` foreground color (three hex digits) / reset, `` `B222 `` / `` `b `` background; `` `c `` center, `` `l `` left, `` `r `` right, `` `a `` default alignment; `` `= `` toggles a literal block. Escape a literal backtick as `` \` ``.
- Links: `` `[label`{{url:todos:todos}}] ``. After its URL, a link lists the fields it submits, `|`-separated: `` `[add`{{url:todos:todos}}`{{method:post}}|title] `` sends the `title` field; `name=value` sends a fixed value (`|finished=1`), `*` sends every field. Each reaches the pipeline as `input:<name>`. Every non-GET verb needs `{{method:verb}}`.
- Fields: `` `<title`> `` a text field named `title`, `` `<title`{{input:title}}> `` with a default, `` `<24|title`> `` 24 wide, `` `<40x5|notes`> `` 40 by 5 lines, `` `<!|password`> `` masked; `` `<?|done|1`>done `` a checkbox (`` `<?|done|1|*`>done `` checked), `` `<^|color|red`>red `` a radio button (one per value, same field name).
```
>Todos
{{#todos_data}}
`[{{title}}`{{url:todos:todo}}]
{{/todos_data}}
-
`<32|title`{{input:title}}> `[add`{{url:todos:todos}}`{{method:post}}|title]
{{#error:title}}`Ff00{{error_message:title}}`f{{/error:title}}
```

## Errors and repairs

A failing step looks up its code in the resource's (or routine's, or subscriber's) `.repairs`/`.errors`, then the module's `repair()`/`error()`. A repair fixes context, then the failed step reruns (only its failed items); a second failure, or a failure inside a repair, goes to errors; repairs never respond. An error handler ends the run, and a request's sends the response. Unhandled, a code from 400 to 599 renders the asset named after it (`404.html`, `404.mu`), or the message as text, with that status; any other code (an app's own, a `fetch_error`) is answered the same way as 500 (`500.html`, `500.mu`), and the log keeps the real code. Codes: `err_invalid` 400, `err_unauthorized` 401, `err_forbidden` 403, `err_not_found` 404, `err_internal` 500 (`http.h` aliases `http_bad_request`, `http_not_authorized`, `http_forbidden`, `http_not_found`, `http_error`); `http_fetch` raises 1xxx and 16xx (`fetch_error`, see the C API file). Custom codes avoid those ranges: `#define err_quota 723`.

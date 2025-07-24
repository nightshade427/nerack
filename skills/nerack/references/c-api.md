# C API, plain C, and constants

## Imperative API (in run, run_worker, and .call functions)

Keep each block to one action and chain several `run()`/`run_worker()` steps for a longer job: the steps are the log (a span each), and errors, repairs, and task resumes work per step. `printf`/`stderr` reach the logs but are a last resort, never the way to trace a job.

```c
typedef char const * string;
typedef struct { int const code; string const message; } error;
typedef struct { void const * const data; size_t const size; } blob;  // blob_handle: blob const*; table_handle, record_handle: opaque

void*  get(string key);                      // nullptr if absent; assign to the stored type: string,
                                             // table_handle, blob_handle, or the opaque pointer's type.
                                             // A table is the one in context: edits show in later steps
void   set_string(string key, string value);
void   set_table(string key, table_handle t);
void   set_blob(string key, blob_handle b);
void   set_opaque(string key, void const * p);  // C only: templates and SQL never see it; dropped at task checkpoints
       // set_* never copy: values must outlive the run (alloc, defer_free, or already in context)
bool   has(string key);
void   format(string key, string tmpl);      // renders a template (pass a literal, never request data) into key, no escaping, no {{url:}}/{{sse:}}

void*  alloc(size_t bytes);                  // freed when the run ends
void   defer_free(void const * p);           // free() a library-owned pointer when the run ends

void   error_set(string key, error e);       // writes error:key; the step finishes, then the nearest handler runs
error const * error_get(string key);         // nullptr when key has no error
bool   error_has(string key);

table_handle  table_new();                   // table_new/record_new/blob_new live until the run ends; never free them
size_t        table_length(table_handle t);
record_handle table_get(table_handle t, size_t i);  // nullptr if out of range
void          table_add(table_handle t, record_handle r);  // shares r: a record from another table is in both
void          table_remove(table_handle t, record_handle r);
void          table_remove_at(table_handle t, size_t i);

record_handle record_new();
void*         record_get(record_handle r, string key);
void          record_set_string(record_handle r, string key, string value);
void          record_set_table(record_handle r, string key, table_handle t);
void          record_set_blob(record_handle r, string key, blob_handle b);
void          record_set_opaque(record_handle r, string key, void const * p);
void          record_remove(record_handle r, string key);

blob_handle   blob_new(void const * data, size_t size);  // data not copied
```

## Plain C

- A `.c` at the root is a root library every module may call; a `.c` in a module folder (`modules/<name>/`) is that module's own. Include project headers with quotes (`#include "pricing.h"`; a module's folder comes before the root); `<...>` is for Nerack, the C library, and `vendor/`.
- Only `app.c` and `module.c` declare `module()`. Modules never call each other's C: a module calling another module's code, a root library calling a module, or anything outside `app` calling `app.c` fails the build. Global symbols are one namespace; keep file-only helpers `static`.
- A `.c` compiles as C23 unless its name picks a standard (`words.c11.c`).
- `vendor/` holds prebuilt libraries (`name.h`, and per architecture `name.<arch>.so` for dev and `name.<arch>.a` for production, `<arch>` being `x86_64` or `aarch64`), flat or a folder each, never source. Dev needs the `.so` of the machine developed on, `nerack build` the `.a` of each architecture in `.nerack/arch`. Nerack does not build them: take them from the library's distribution or build from source, against musl for Linux on that architecture (Alpine packages work; Debian/Ubuntu `-dev` packages are glibc and do not link). Include as `<name.h>`, call from `run_worker()`, and register library allocations with `defer_free()`.
- Pipelines run on many threads at once: keep no mutable globals (a dev reload also resets them), and call only thread-safe library functions. Wrap a library that is not thread-safe in a module whose steps call it with `run_worker(.call = fn, .queue = "lib")`, so calls run one at a time, and export that module for other apps.
- An `export.h` beside `module.c` exports the module: `nerack build` writes `output/<name>.h`, `LICENSE.<name>`, and `<name>.<arch>.so` and `.a` for both architectures, for another app's `vendor/`. `export.h` may include only Nerack, C library, and vendor headers. The package carries the root libraries the module uses and its own folder's assets, not the app's root assets: a using app supplies any it expects (such as `layout`) in `overrides/<name>/`.
- Every file compiles with `-Wall -Wextra`, stack protection, and zero-initialized locals; a non-literal format string is an error even in dev, and `nerack build` fails on any warning.

## Constants

Verbs: `http_get http_post http_put http_patch http_delete`.
Fetch errors (`http.h`, `fetch_error`): 1000 + the remote status (`fetch_bad_request`1400 `fetch_not_authorized`1401 `fetch_forbidden`1403 `fetch_not_found`1404 `fetch_conflict`1409 `fetch_too_many_requests`1429 `fetch_internal`1500 `fetch_bad_gateway`1502 `fetch_unavailable`1503 `fetch_gateway_timeout`1504; any other as its number, `1418`), and no response: `fetch_timeout`1601 `fetch_refused`1602 `fetch_dns`1603 `fetch_tls`1604.
Status: `http_ok`200 `http_created`201 `http_redirected`302 `http_bad_request`400 `http_not_authorized`401 `http_forbidden`403 `http_not_found`404 `http_error`500.
Mime: `mime_html mime_text mime_event_stream mime_json mime_javascript`.
Datastar modes: `outer_mode inner_mode replace_mode prepend_mode append_mode before_mode after_mode remove_mode`.
Validators (`nerack.h`): `not_empty_valid alpha_valid alphanumeric_valid slug_valid integer_valid positive_integer_valid float_valid percent_valid email_valid uuid_valid user_valid date_valid time_valid datetime_valid zip_valid phone_valid cron_valid token_valid base64_valid bool_valid yes_no_valid on_off_valid`; (`http.h`): `no_html_valid url_valid ipv4_valid hex_color_valid`. Custom: `#define sku_valid "^[A-Z]{3}-[0-9]{4}$"`.

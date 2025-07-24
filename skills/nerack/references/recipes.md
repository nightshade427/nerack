# Recipes

```c
// Custom validation beyond regex
validate({"input:username", user_valid, "3–32 letters, digits, or _, starting with a letter"}),
sql_query({"users_db", "find_username", "existing_data"}),
run(^(){ if (table_length(get("existing_data")) > 0) error_set("username", (error){err_invalid, "already taken"}); }),

// Signed-in resource with a per-user database
sqlite("notes_db", "file:{{user_id}}_notes.db?mode=rwc", {"create_notes_table"});
http("notes", "/notes", .all = {cookie_logged_in(), cookie_session()}, .get = { ... });

// htmx: a fragment for htmx requests, the full page otherwise
html("todos_fragment", "page_s", .if_context_key = "is_htmx"),
html("todos_page", "page_s", .unless_context_key = "is_htmx"),
http_response("page_s")

// JSON for an API client
http("api_todos", "/api/todos", .mime = mime_json,
  .get = {sql_query({"todos_db", "get_todos", "todos_data"}), json("todos_data", "todos_s"), http_response("todos_s")}
);

// JSON for one item: an object, not an array of one
http("api_todo", "/api/todos/:id", .mime = mime_json,
  .all = {validate({"input:id", positive_integer_valid})},
  .get = {sql_query({"todos_db", "get_todo", "todo_d", .error_on_empty = true}), json("todo_d", "todo_s", .record = true), http_response("todo_s")}
);

// Outbound call failures: remote statuses are 1000 + status, no response is 16xx
http("weather", "/weather",
  .get = {http_fetch({http_get, "https://api.weather.dev/now", "weather_data"}), html("weather", "weather_s"), http_response("weather_s")},
  .errors = {{fetch_not_found, {html("no_weather", "weather_s"), http_response("weather_s")}},
             {fetch_timeout, {html("weather_down", "weather_s"), http_response("weather_s", .status = http_error)}}},
  .repairs = {{fetch_not_authorized, {run(.call = refresh_weather_token)}}}
);

// A routine runs inline in the caller's context: count_d is there for the next step
routine("count_todos", {sql_query({"todos_db", "count_todos", "count_d"})});
// in a pipeline: run_routine("count_todos"), html("summary", "summary_s")

// A library that is not thread-safe: one queue, one call at a time
run_worker(.call = render_with_legacy_lib, .queue = "legacy_lib")

// Concurrent outbound calls, then a durable background job and a daily one
http_fetch({http_get, "https://api.weather.dev/now", "weather_data"}, {http_get, "https://api.quotes.dev/random", "quote_data"}),
queue_task("notify_new_todo"),
task("notify_new_todo", {http_fetch({http_post, "https://api.push.dev/notify", .json = "{\"text\":\"{{title}}\"}"})}, .accepts = {"title"});
cron("daily_digest", "0 8 * * *", {sql_query({"todos_db", "digest"})});

// Blocking third-party C on the thread pool
run_worker(^(){ char * const out = cmark_markdown_to_html(get("md"), strlen(get("md")), 0); defer_free(out); set_string("md_s", out); })
```

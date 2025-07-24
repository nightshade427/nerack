[← Back to README](./README.md)

# Guide

This guide builds a todo app one concept at a time. See the [Reference](./REFERENCE.md) for full options on each step, helper, and field. Nerack discovers assets by file location and seeds each one into the context of the module that owns it (see [Assets](./REFERENCE.md#assets)).

* [1. Pages and Templates](#1-pages-and-templates)
* [2. Show Data](#2-show-data)
* [3. Accept Input](#3-accept-input)
* [4. Nested Data](#4-nested-data)
* [5. Calling APIs](#5-calling-apis)
* [6. Tasks](#6-tasks)
* [7. Modules and Events](#7-modules-and-events)

### 1. Pages and Templates

Each `http(...)` declares a named resource and URL, and each verb pipeline is a list of steps. `html("home", "home_s")` renders the template asset `home` into context key `home_s`, and `http_response("home_s")` sends it. `{{url:verb:name}}` builds a link to a resource: the verb first, then the resource name. Any `:params` in the pattern are filled from the current scope by matching key names.

Both pages share a layout, so `home` doubles as the layout: it declares the nav and a `{{$body}}` block whose default is the welcome page. The `todos` page extends it with `{{< home}}...{{/home}}`, overriding that block. Any template that declares a `{{$block}}` can be a parent; there is no special layout type.

**`home.mustache.html`**
```html
<html>
  <body>
    <nav><a href='{{url:get:home}}'>Home</a> · <a href='{{url:get:todos}}'>My Todos</a></nav>
    <main>
      {{$body}}
        <h1>Welcome</h1>
      {{/body}}
    </main>
  </body>
</html>
```

**`todos.mustache.html`**
```html
{{< home}}
  {{$body}}
    <h1>My Todos</h1>
    <p>Nothing yet.</p>
  {{/body}}
{{/home}}
```

**`todo.c`**
```c
#include <nerack.h>
#include <http.h>

module(todo){
  http("home", "/",
    .get = {
      html("home", "home_s"),
      http_response("home_s")
    }
  );
  http("todos", "/todos",
    .get = {
      html("todos", "todos_s"),
      http_response("todos_s")
    }
  );
}
```

See [HTTP Pipelines](./REFERENCE.md#http-pipelines) and [Templates](./REFERENCE.md#templates).

### 2. Show Data

Bring in SQLite with `#include <sqlite.h>`, declare a database with `sqlite(...)`, and read with `sqlite_query()`. SQL files are assets like templates: `get_todos.sql` becomes the asset `get_todos`.

Three new SQL files:

**`create_todos_table.sql`**
```sql
CREATE TABLE IF NOT EXISTS todos (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  title TEXT NOT NULL
);
```

**`seed_todos.sql`**
```sql
INSERT INTO todos(title) VALUES('Learn Nerack');
```

**`get_todos.sql`**
```sql
select id, title from todos;
```

Render the rows Nerack stores under `todos_data`:

**`todos.mustache.html`**
```diff
 {{< home}}
   {{$body}}
     <h1>My Todos</h1>
-    <p>Nothing yet.</p>
+    <ul>
+      {{#todos_data}}
+        <li>{{title}}</li>
+      {{/todos_data}}
+    </ul>
   {{/body}}
 {{/home}}
```

Wire up the module, database, and query:

**`todo.c`**
```diff
 #include <nerack.h>
 #include <http.h>
+#include <sqlite.h>

 module(todo){
+  sqlite(
+    "todos_db",
+    "file:todos.db?mode=rwc",
+    {"create_todos_table"},
+    {"seed_todos"}
+  );
+
   http("home", "/",
     .get = {
       html("home", "home_s"),
       http_response("home_s")
     }
   );
   http("todos", "/todos",
     .get = {
+      sqlite_query({"todos_db", "get_todos", "todos_data"}),
       html("todos", "todos_s"),
       http_response("todos_s")
     }
   );
 }
```

The three positional arguments are the database name, the SQL asset, and the context key for the result table (`todos_data`). The template walks the result with `{{#todos_data}}...{{/todos_data}}`. Migrations and seeds run on first connection. See [Databases](./REFERENCE.md#databases) and [query](./REFERENCE.md#query).

### 3. Accept Input

Add a `.post` verb that validates, inserts, and redirects (POST-redirect-GET). A resource-scoped `.errors` handler re-renders the form on validation failure.

**`create_todo.sql`**
```sql
insert into todos(title) values({{title}});
```

Add the form, repopulating the field and showing the error after a failed submit:

**`todos.mustache.html`**
```diff
 {{< home}}
   {{$body}}
     <h1>My Todos</h1>
+    <form method='post' action='{{url:post:todos}}'>
+      <input name='title' value='{{input:title}}'>
+      {{#error:title}}
+        <span class='error'>{{error_message:title}}</span>
+      {{/error:title}}
+      <button>Add</button>
+    </form>
     <ul>
       {{#todos_data}}
         <li>{{title}}</li>
       {{/todos_data}}
     </ul>
   {{/body}}
 {{/home}}
```

Add a `.post` verb and an `.errors` handler:

**`todo.c`**
```diff
 #include <nerack.h>
 #include <http.h>
 #include <sqlite.h>

 module(todo){
   sqlite(
     "todos_db",
     "file:todos.db?mode=rwc",
     {"create_todos_table"},
     {"seed_todos"}
   );

   http("home", "/",
     .get = {
       html("home", "home_s"),
       http_response("home_s")
     }
   );

   http("todos", "/todos",
     .get = {
       sqlite_query({"todos_db", "get_todos", "todos_data"}),
       html("todos", "todos_s"),
       http_response("todos_s")
-    }
+    },
+    .post = {
+      input({"title", not_empty_input}),
+      sqlite_query({"todos_db", "create_todo"}),
+      http_redirect("todos")
+    },
+    .errors = {
+      {http_bad_request, {http_reroute("todos")}}
+    }
   );
 }
```

The form posts to `{{url:post:todos}}`. Because `post` is a state-changing verb, the helper appends both `http_method=post` and a fresh CSRF token to the action URL, so the form needs no hidden fields of its own.

`input()` validates and promotes `title` to app scope; the `{{title}}` in `create_todo.sql` binds as a prepared-statement parameter. On failure, `http_bad_request` triggers the handler, which `reroute`s back into the GET pipeline in-process. The `input:` and `error:` scopes survive the reroute, so the form repopulates with `{{input:title}}` and shows `{{error_message:title}}`. See [input](./REFERENCE.md#input), [Error and Repair Pipelines](./REFERENCE.md#error-and-repair-pipelines), and [redirect and reroute](./REFERENCE.md#redirect-and-reroute).

### 4. Nested Data

A `/todos/:id` page fetches a todo and its comments concurrently, then nests the comments inside the todo with `join()`.

Three new SQL files and one new template:

**`create_comments_table.sql`**
```sql
CREATE TABLE IF NOT EXISTS comments (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  todo_id INTEGER NOT NULL REFERENCES todos(id),
  body TEXT NOT NULL
);
```

**`get_todo.sql`**
```sql
select id, title from todos where id = {{id}};
```

**`get_comments.sql`**
```sql
select id, todo_id, body from comments where todo_id = {{id}};
```

Enter `{{#todo_data}}` first; after the join, `comments` lives inside each todo record:

**`todo.mustache.html`**
```html
{{< home}}
  {{$body}}
    {{#todo_data}}
      <h1>{{title}}</h1>
      <h2>Comments</h2>
      <ul>
        {{#comments}}
          <li>{{body}}</li>
        {{/comments}}
      </ul>
    {{/todo_data}}
  {{/body}}
{{/home}}
```

Link each list item to its detail page. `{{url:get:todo}}` resolves the `todo` resource's pattern (`/todos/:id`) and fills `:id` from the current scope, which inside `{{#todos_data}}` is the current row:

**`todos.mustache.html`**
```diff
       {{#todos_data}}
-        <li>{{title}}</li>
+        <li><a href='{{url:get:todo}}'>{{title}}</a></li>
       {{/todos_data}}
```

Register the migration and add a `todo` resource:

**`todo.c`**
```diff
 #include <nerack.h>
 #include <http.h>
 #include <sqlite.h>

 module(todo){
   sqlite(
     "todos_db",
     "file:todos.db?mode=rwc",
-    {"create_todos_table"},
+    {"create_todos_table", "create_comments_table"},
     {"seed_todos"}
   );

   http("home", "/",
     .get = {
       html("home", "home_s"),
       http_response("home_s")
     }
   );

   http("todos", "/todos",
     .get = {
       sqlite_query({"todos_db", "get_todos", "todos_data"}),
       html("todos", "todos_s"),
       http_response("todos_s")
     },
     .post = {
       input({"title", not_empty_input}),
       sqlite_query({"todos_db", "create_todo"}),
       http_redirect("todos")
     },
     .errors = {
       {http_bad_request, {http_reroute("todos")}}
     }
   );
+
+  http("todo", "/todos/:id",
+    .get = {
+      input({"id", positive_integer_input}),
+      sqlite_query(
+        {"todos_db", "get_todo", "todo_data", .error_on_empty = true},
+        {"todos_db", "get_comments", "comments"}
+      ),
+      join("todo_data", "id", "comments", "todo_id"),
+      html("todo", "todo_s"),
+      http_response("todo_s")
+    }
+  );
 }
```

Both queries in one `sqlite_query()` call run concurrently. `join()` lifts `comments` inside each `todo_data` record, so the template reaches `{{#comments}}` from within `{{#todo_data}}`. `.error_on_empty = true` returns 404 when the id matches nothing. See [join](./REFERENCE.md#join) and [query](./REFERENCE.md#query).

### 5. Calling APIs

`http_fetch()` calls external HTTP services like a query calls a database. JSON parses into context tables; multiple items in one `http_fetch()` run concurrently.

Show the responses on the home page:

**`home.mustache.html`**
```diff
 <html>
   <body>
     <nav><a href='{{url:get:home}}'>Home</a> · <a href='{{url:get:todos}}'>My Todos</a></nav>
     <main>
-      {{$body}}
-        <h1>Welcome</h1>
-      {{/body}}
+      {{$body}}
+        <h1>Welcome</h1>
+        {{#weather}}
+          <p>{{city}}: {{precision:temp_c:0}}°C, {{conditions}}</p>
+        {{/weather}}
+        {{#quote}}
+          <blockquote>{{content}}, {{author}}</blockquote>
+        {{/quote}}
+      {{/body}}
     </main>
   </body>
 </html>
```

Fetch both services concurrently before rendering:

**`todo.c`**
```diff
 #include <nerack.h>
 #include <http.h>
 #include <sqlite.h>

 module(todo){
   sqlite(
     "todos_db",
     "file:todos.db?mode=rwc",
     {"create_todos_table", "create_comments_table"},
     {"seed_todos"}
   );

   http("home", "/",
     .get = {
+      http_fetch(
+        {http_get, "https://api.quotes.dev/random", "quote"},
+        {http_get, "https://api.weather.dev/now", "weather"}
+      ),
       html("home", "home_s"),
       http_response("home_s")
     }
   );

   http("todos", "/todos",
     .get = {
       sqlite_query({"todos_db", "get_todos", "todos_data"}),
       html("todos", "todos_s"),
       http_response("todos_s")
     },
     .post = {
       input({"title", not_empty_input}),
       sqlite_query({"todos_db", "create_todo"}),
       http_redirect("todos")
     },
     .errors = {
       {http_bad_request, {http_reroute("todos")}}
     }
   );

   http("todo", "/todos/:id",
     .get = {
       input({"id", positive_integer_input}),
       sqlite_query(
         {"todos_db", "get_todo", "todo_data", .error_on_empty = true},
         {"todos_db", "get_comments", "comments"}
       ),
       join("todo_data", "id", "comments", "todo_id"),
       html("todo", "todo_s"),
       http_response("todo_s")
     }
   );
 }
```

Both requests run concurrently under one `http_fetch()` call. The JSON parses into context tables the template walks with `{{#quote}}` and `{{#weather}}`. `http_fetch()` also supports other verbs, headers, request bodies, and interpolated URLs. See [fetch](./REFERENCE.md#fetch).

### 6. Tasks

A task is a named, reusable pipeline. Define it once with optional `.cron`; dispatch durable background runs with `dispatch("name")` (from `dispatch.h`).

Two new SQL files:

**`create_daily_stats_table.sql`**
```sql
CREATE TABLE IF NOT EXISTS daily_stats (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  recorded_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  todo_count INTEGER NOT NULL
);
```

**`record_daily_stats.sql`**
```sql
insert into daily_stats(todo_count) select count(*) from todos;
```

Register the migration, define the tasks, dispatch them from the POST:

**`todo.c`**
```diff
 #include <nerack.h>
 #include <http.h>
 #include <sqlite.h>
+#include <dispatch.h>

 module(todo){
   sqlite(
     "todos_db",
     "file:todos.db?mode=rwc",
-    {"create_todos_table", "create_comments_table"},
+    {"create_todos_table", "create_comments_table", "create_daily_stats_table"},
     {"seed_todos"}
   );

+  task("record_daily_stats", {
+    sqlite_query({"todos_db", "record_daily_stats"})
+  }, .cron = "0 0 * * *");
+
+  task("notify_new_todo", {
+    http_fetch({
+      http_post,
+      "https://api.push.dev/notify",
+      .json = "{\"text\":\"New todo: {{title}}\"}"
+    })
+  }, .accepts = {"title"});
+
   http("home", "/",
     .get = {
       http_fetch(
         {http_get, "https://api.quotes.dev/random", "quote"},
         {http_get, "https://api.weather.dev/now", "weather"}
       ),
       html("home", "home_s"),
       http_response("home_s")
     }
   );

   http("todos", "/todos",
     .get = {
       sqlite_query({"todos_db", "get_todos", "todos_data"}),
       html("todos", "todos_s"),
       http_response("todos_s")
     },
     .post = {
       input({"title", not_empty_input}),
       sqlite_query({"todos_db", "create_todo"}),
+      dispatch("notify_new_todo"),
       http_redirect("todos")
     },
     .errors = {
       {http_bad_request, {http_reroute("todos")}}
     }
   );

   http("todo", "/todos/:id",
     .get = {
       input({"id", positive_integer_input}),
       sqlite_query(
         {"todos_db", "get_todo", "todo_data", .error_on_empty = true},
         {"todos_db", "get_comments", "comments"}
       ),
       join("todo_data", "id", "comments", "todo_id"),
       html("todo", "todo_s"),
       http_response("todo_s")
     }
   );
 }
```

`.cron` and `dispatch(...)` both run the task on a task reactor, off the request reactors, so the POST returns immediately. Dispatched tasks are durable: a crash mid-task resumes on the next boot. To hand values to a task, list them under `.accepts`; `notify_new_todo` pulls in `title` that way. `dispatch()` comes from `dispatch.h`. See [Task Pipelines](./REFERENCE.md#task-pipelines).

### 7. Modules and Events

Split features into modules that talk through pub/sub events. A module is any `.c` file declaring `module(name){ ... }`, registering that module's resources, databases, tasks, and subscribers. Nerack discovers every `module(...)` on disk. A module's assets live in its `name/` folder and are private to it; assets at the project root are shared across modules (see [Assets](./REFERENCE.md#assets)).

This step moves todos into its own module and adds an `activity` module that records an entry whenever a todo is created. The top-level file becomes `main.c` and keeps just the `home` resource; `home.mustache.html` stays at the project root, shared across modules:

```
.
├── main.c
├── todos/
│   ├── todos.c
│   ├── todos.mustache.html
│   ├── todo.mustache.html
│   ├── create_todos_table.sql
│   ├── create_comments_table.sql
│   ├── create_daily_stats_table.sql
│   ├── seed_todos.sql
│   ├── record_daily_stats.sql
│   ├── get_todos.sql
│   ├── get_todo.sql
│   ├── get_comments.sql
│   └── create_todo.sql
├── activity/
│   ├── activity.c
│   ├── activity.mustache.html
│   ├── create_activity_table.sql
│   ├── get_activities.sql
│   └── insert_activity.sql
└── home.mustache.html
```

**`main.c`**
```diff
 #include <nerack.h>
 #include <http.h>
-#include <sqlite.h>
-#include <dispatch.h>

-module(todo){
+module(main){
-  sqlite(
-    "todos_db",
-    "file:todos.db?mode=rwc",
-    {"create_todos_table", "create_comments_table", "create_daily_stats_table"},
-    {"seed_todos"}
-  );
-
-  task("record_daily_stats", {
-    sqlite_query({"todos_db", "record_daily_stats"})
-  }, .cron = "0 0 * * *");
-
-  task("notify_new_todo", {
-    http_fetch({
-      http_post,
-      "https://api.push.dev/notify",
-      .json = "{\"text\":\"New todo: {{title}}\"}"
-    })
-  }, .accepts = {"title"});
-
   http("home", "/",
     .get = {
       http_fetch(
         {http_get, "https://api.quotes.dev/random", "quote"},
         {http_get, "https://api.weather.dev/now", "weather"}
       ),
       html("home", "home_s"),
       http_response("home_s")
     }
   );
-
-  http("todos", "/todos",
-    .get = {
-      sqlite_query({"todos_db", "get_todos", "todos_data"}),
-      html("todos", "todos_s"),
-      http_response("todos_s")
-    },
-    .post = {
-      input({"title", not_empty_input}),
-      sqlite_query({"todos_db", "create_todo"}),
-      dispatch("notify_new_todo"),
-      http_redirect("todos")
-    },
-    .errors = {
-      {http_bad_request, {http_reroute("todos")}}
-    }
-  );
-
-  http("todo", "/todos/:id",
-    .get = {
-      input({"id", positive_integer_input}),
-      sqlite_query(
-        {"todos_db", "get_todo", "todo_data", .error_on_empty = true},
-        {"todos_db", "get_comments", "comments"}
-      ),
-      join("todo_data", "id", "comments", "todo_id"),
-      html("todo", "todo_s"),
-      http_response("todo_s")
-    }
-  );
 }
```

Add an Activity link to the shared nav:

**`home.mustache.html`**
```diff
 <html>
   <body>
-    <nav><a href='{{url:get:home}}'>Home</a> · <a href='{{url:get:todos}}'>My Todos</a></nav>
+    <nav><a href='{{url:get:home}}'>Home</a> · <a href='{{url:get:todos}}'>My Todos</a> · <a href='{{url:get:activity}}'>Activity</a></nav>
     <main>
       {{$body}}
         <h1>Welcome</h1>
         {{#weather}}
           <p>{{city}}: {{precision:temp_c:0}}°C, {{conditions}}</p>
         {{/weather}}
         {{#quote}}
           <blockquote>{{content}}, {{author}}</blockquote>
         {{/quote}}
       {{/body}}
     </main>
   </body>
 </html>
```

The todos logic moves into the module unchanged, with a `publish()` declaration and an `emit()` step added. Both todo resources move with it:

**`todos/todos.c`**
```c
#include <nerack.h>
#include <http.h>
#include <sqlite.h>
#include <pubsub.h>
#include <dispatch.h>

module(todos){
  sqlite(
    "todos_db",
    "file:todos.db?mode=rwc",
    {"create_todos_table", "create_comments_table", "create_daily_stats_table"},
    {"seed_todos"}
  );

  publish("todo_created",
    .with = {"title"}
  );

  task("record_daily_stats", {
    sqlite_query({"todos_db", "record_daily_stats"})
  }, .cron = "0 0 * * *");

  task("notify_new_todo", {
    http_fetch({
      http_post,
      "https://api.push.dev/notify",
      .json = "{\"text\":\"New todo: {{title}}\"}"
    })
  }, .accepts = {"title"});

  http("todos", "/todos",
    .get = {
      sqlite_query({"todos_db", "get_todos", "todos_data"}),
      html("todos", "todos_s"),
      http_response("todos_s")
    },
    .post = {
      input({"title", not_empty_input}),
      sqlite_query({"todos_db", "create_todo"}),
      dispatch("notify_new_todo"),
      emit("todo_created"),
      http_redirect("todos")
    },
    .errors = {
      {http_bad_request, {http_reroute("todos")}}
    }
  );

  http("todo", "/todos/:id",
    .get = {
      input({"id", positive_integer_input}),
      sqlite_query(
        {"todos_db", "get_todo", "todo_data", .error_on_empty = true},
        {"todos_db", "get_comments", "comments"}
      ),
      join("todo_data", "id", "comments", "todo_id"),
      html("todo", "todo_s"),
      http_response("todo_s")
    }
  );
}
```

The `activity` module owns its own table, query, template, and subscriber. Nothing in it references the todos module:

**`activity/create_activity_table.sql`**
```sql
CREATE TABLE IF NOT EXISTS activities (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  kind TEXT NOT NULL,
  ref TEXT NOT NULL,
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

**`activity/insert_activity.sql`**
```sql
insert into activities(kind, ref) values('created', {{title}});
```

**`activity/get_activities.sql`**
```sql
select kind, ref, created_at from activities order by created_at desc;
```

**`activity/activity.mustache.html`**
```html
{{< home}}
  {{$body}}
    <h1>Activity</h1>
    <ul>
      {{#activities}}
        <li>{{kind}}: {{ref}} ({{created_at}})</li>
      {{/activities}}
    </ul>
  {{/body}}
{{/home}}
```

**`activity/activity.c`**
```c
#include <nerack.h>
#include <http.h>
#include <sqlite.h>
#include <pubsub.h>

module(activity){
  sqlite(
    "activity_db",
    "file:activity.db?mode=rwc",
    {"create_activity_table"}
  );

  subscribe("todo_created", {
    sqlite_query({"activity_db", "insert_activity"})
  });

  http("activity", "/activity",
    .get = {
      sqlite_query({"activity_db", "get_activities", "activities"}),
      html("activity", "activity_s"),
      http_response("activity_s")
    }
  );
}
```

When the POST calls `emit("todo_created")`, Nerack propagates the keys named in `publish(...).with` (`title`) to every subscriber. The `activity` module writes its row with no direct link to the publisher. Events are durable: undelivered ones replay after a crash. A third subscriber is another module with its own `subscribe(...)`, and the publisher still does not change. See [Modules and Composition](./REFERENCE.md#modules-and-composition) and [Event Pipelines](./REFERENCE.md#event-pipelines).

[← Back to README](./README.md)

# Guide

This guide builds a todo app one concept at a time, from an empty folder to a deployed container.

* [0. Set Up](#0-set-up)
* [1. Pages and Templates](#1-pages-and-templates)
* [2. Show Data](#2-show-data)
* [3. Accept Input](#3-accept-input)
* [4. Nested Data](#4-nested-data)
* [5. Calling APIs](#5-calling-apis)
* [6. Routines and Tasks](#6-routines-and-tasks)
* [7. Modules](#7-modules)
* [8. Events](#8-events)
* [9. Deploy](#9-deploy)

### 0. Set Up

Make a folder for the app and run `nerack` in it:
```bash
mkdir todos && cd todos
nerack
```

The first run sets the folder up as a project, writes a hello-world `app.c`, and starts the dev server on http://localhost:3000. Leave it running: every file you save from here on recompiles and reloads, so each step shows up in the browser as soon as you save it. Commit `.nerack/version` along with your code; it pins the app to this version of Nerack.

### 1. Pages and Templates

Each `http(...)` declares a named resource and URL, and each verb pipeline is a list of steps. `html("home", "home_s")` renders the template asset `home` into context key `home_s`, and `http_response("home_s")` sends it. `{{url:app:home}}` builds a link to a resource: the module that declares it (`app`, the module `app.c` declares), then the resource's name. Templates always name the module, so a template links correctly wherever it ends up rendering.

Both pages share a layout, so `home` doubles as the layout: it declares the nav and a `{{$body}}` block whose default is the welcome page. The `todos` page extends it with `{{< home}}...{{/home}}`, overriding that block. Any template that declares a `{{$block}}` can be a parent; there is no special layout type.

**`home.html`**
```html
<html>
  <body>
    <nav><a href='{{url:app:home}}'>Home</a> · <a href='{{url:app:todos}}'>My Todos</a></nav>
    <main>
      {{$body}}
        <h1>Welcome</h1>
      {{/body}}
    </main>
  </body>
</html>
```

**`todos.html`**
```html
{{< home}}
  {{$body}}
    <h1>My Todos</h1>
    <p>Nothing yet.</p>
  {{/body}}
{{/home}}
```

**`app.c`**
```c
#include <nerack.h>
#include <http.h>

module(){
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

### 2. Show Data

Include `sqlite.h`, declare a database with `sqlite(...)`, and read from it with `sql_query()`. SQL files are assets like templates: `get_todos.sql` becomes the asset `get_todos`.

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

**`todos.html`**
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

**`app.c`**
```diff
 #include <nerack.h>
 #include <http.h>
+#include <sqlite.h>

 module(){
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
+      sql_query({"todos_db", "get_todos", "todos_data"}),
       html("todos", "todos_s"),
       http_response("todos_s")
     }
   );
 }
```

The three positional arguments are the database name, the SQL asset, and the context key for the result table (`todos_data`). The template walks the result with `{{#todos_data}}...{{/todos_data}}`. Migrations and seeds run on first connection.

### 3. Accept Input

A form posts new todos. The POST pipeline validates, inserts, and redirects (POST-redirect-GET), and a resource-scoped `.errors` handler re-renders the form when validation fails.

**`create_todo.sql`**
```sql
insert into todos(title) values({{title}});
```

Add the form, repopulating the field and showing the error after a failed submit:

**`todos.html`**
```diff
 {{< home}}
   {{$body}}
     <h1>My Todos</h1>
+    <form method='post' action='{{url:app:todos}}'>
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

Add the `.post` pipeline and the `.errors` handler:

**`app.c`**
```diff
 #include <nerack.h>
 #include <http.h>
 #include <sqlite.h>

 module(){
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
       sql_query({"todos_db", "get_todos", "todos_data"}),
       html("todos", "todos_s"),
       http_response("todos_s")
-    }
+    },
+    .post = {
+      validate({"input:title", not_empty_valid}),
+      sql_query({"todos_db", "create_todo"}),
+      http_redirect("todos")
+    },
+    .errors = {
+      {http_bad_request, {http_reroute("todos")}}
+    }
   );
 }
```

The form posts to `{{url:app:todos}}`, which runs the `.post` pipeline. Nerack accepts state-changing requests only from your own pages, so the form needs no CSRF token or hidden fields.

`validate()` validates and promotes `title` to app scope; the `{{title}}` in `create_todo.sql` binds as a prepared-statement parameter. On failure, `http_bad_request` triggers the handler, which calls `http_reroute()` to run the GET pipeline in-process. The whole context survives the reroute, so the form repopulates with `{{input:title}}` and shows `{{error_message:title}}`.

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

**`todo.html`**
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

Link each list item to its detail page. `{{url:app:todo}}` resolves the `todo` resource's pattern (`/todos/:id`) and fills `:id` from the current scope, which inside `{{#todos_data}}` is the current row:

**`todos.html`**
```diff
       {{#todos_data}}
-        <li>{{title}}</li>
+        <li><a href='{{url:app:todo}}'>{{title}}</a></li>
       {{/todos_data}}
```

Register the migration and add a `todo` resource:

**`app.c`**
```diff
 #include <nerack.h>
 #include <http.h>
 #include <sqlite.h>

 module(){
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
       sql_query({"todos_db", "get_todos", "todos_data"}),
       html("todos", "todos_s"),
       http_response("todos_s")
     },
     .post = {
       validate({"input:title", not_empty_valid}),
       sql_query({"todos_db", "create_todo"}),
       http_redirect("todos")
     },
     .errors = {
       {http_bad_request, {http_reroute("todos")}}
     }
   );
+
+  http("todo", "/todos/:id",
+    .get = {
+      validate({"input:id", positive_integer_valid}),
+      sql_query(
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

Both queries in one `sql_query()` call run concurrently. `join()` lifts `comments` inside each `todo_data` record, so the template reaches `{{#comments}}` from within `{{#todo_data}}`. `.error_on_empty = true` returns 404 when the id matches nothing.

### 5. Calling APIs

`http_fetch()` calls external HTTP services like a query calls a database. JSON parses into context tables; multiple items in one `http_fetch()` run concurrently.

Show the responses on the home page:

**`home.html`**
```diff
 <html>
   <body>
     <nav><a href='{{url:app:home}}'>Home</a> · <a href='{{url:app:todos}}'>My Todos</a></nav>
     <main>
       {{$body}}
         <h1>Welcome</h1>
+        {{#weather}}
+          <p>{{city}}: {{precision:temp_c:0}}°C, {{conditions}}</p>
+        {{/weather}}
+        {{#quote}}
+          <blockquote>{{content}}, {{author}}</blockquote>
+        {{/quote}}
       {{/body}}
     </main>
   </body>
 </html>
```

Fetch both services concurrently before rendering:

**`app.c`**
```diff
 #include <nerack.h>
 #include <http.h>
 #include <sqlite.h>

 module(){
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
       sql_query({"todos_db", "get_todos", "todos_data"}),
       html("todos", "todos_s"),
       http_response("todos_s")
     },
     .post = {
       validate({"input:title", not_empty_valid}),
       sql_query({"todos_db", "create_todo"}),
       http_redirect("todos")
     },
     .errors = {
       {http_bad_request, {http_reroute("todos")}}
     }
   );

   http("todo", "/todos/:id",
     .get = {
       validate({"input:id", positive_integer_valid}),
       sql_query(
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

Both requests run concurrently under one `http_fetch()` call. The JSON parses into context tables the template walks with `{{#quote}}` and `{{#weather}}`.

### 6. Routines and Tasks

A routine is a named, reusable pipeline. Define it once, then run it as a task, a durable background job: on demand with `dispatch("name")`, or on a schedule with `cron("name", schedule)` (both from `task.h`).

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

Register the migration, define the routines, and dispatch one from the POST:

**`app.c`**
```diff
 #include <nerack.h>
 #include <http.h>
 #include <sqlite.h>
+#include <task.h>

 module(){
   sqlite(
     "todos_db",
     "file:todos.db?mode=rwc",
-    {"create_todos_table", "create_comments_table"},
+    {"create_todos_table", "create_comments_table", "create_daily_stats_table"},
     {"seed_todos"}
   );

+  routine("record_daily_stats", {
+    sql_query({"todos_db", "record_daily_stats"})
+  });
+  cron("record_daily_stats", "0 0 * * *");
+
+  routine("notify_new_todo", {
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
       sql_query({"todos_db", "get_todos", "todos_data"}),
       html("todos", "todos_s"),
       http_response("todos_s")
     },
     .post = {
       validate({"input:title", not_empty_valid}),
       sql_query({"todos_db", "create_todo"}),
+      dispatch("notify_new_todo"),
       http_redirect("todos")
     },
     .errors = {
       {http_bad_request, {http_reroute("todos")}}
     }
   );

   http("todo", "/todos/:id",
     .get = {
       validate({"input:id", positive_integer_valid}),
       sql_query(
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

`cron(...)` and `dispatch(...)` both run the routine as a task on the task reactor, off the request reactors, so the POST returns immediately. Every task, scheduled or dispatched, is durable: a crash mid-task resumes on the next boot. To hand values to a task, list them under `.accepts`; `notify_new_todo` pulls in `title` that way.

### 7. Modules

Split features into modules. Each module gets a folder in `modules/` named after it, holding its `module.c` and its assets. Everything a module declares is its own: other modules reach only its resources, by naming the module (`{{url:todos:todos}}`), and the events it publishes. Its assets are private to it, while the app's assets beside `app.c` are seen by every module.

This section moves todos into their own module. `app.c` keeps just the `home` resource, and `home.html` stays beside it, where every module can extend it:

```
.
├── app.c
├── home.html
└── modules/
    └── todos/
        ├── module.c
        ├── todos.html
        ├── todo.html
        ├── create_todos_table.sql
        ├── create_comments_table.sql
        ├── create_daily_stats_table.sql
        ├── seed_todos.sql
        ├── record_daily_stats.sql
        ├── get_todos.sql
        ├── get_todo.sql
        ├── get_comments.sql
        └── create_todo.sql
```

**`app.c`**
```diff
 #include <nerack.h>
 #include <http.h>
-#include <sqlite.h>
-#include <task.h>

 module(){
-  sqlite(
-    "todos_db",
-    "file:todos.db?mode=rwc",
-    {"create_todos_table", "create_comments_table", "create_daily_stats_table"},
-    {"seed_todos"}
-  );
-
-  routine("record_daily_stats", {
-    sql_query({"todos_db", "record_daily_stats"})
-  });
-  cron("record_daily_stats", "0 0 * * *");
-
-  routine("notify_new_todo", {
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
-      sql_query({"todos_db", "get_todos", "todos_data"}),
-      html("todos", "todos_s"),
-      http_response("todos_s")
-    },
-    .post = {
-      validate({"input:title", not_empty_valid}),
-      sql_query({"todos_db", "create_todo"}),
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
-      validate({"input:id", positive_integer_valid}),
-      sql_query(
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

Links name the module that declares each resource, so moving resources changes that part of the link: `home` stays in `app`, the todo resources now belong to `todos`. Because every link is qualified, `home.html` works unchanged as the layout for every module. Update the nav:

**`home.html`**
```diff
 <html>
   <body>
-    <nav><a href='{{url:app:home}}'>Home</a> · <a href='{{url:app:todos}}'>My Todos</a></nav>
+    <nav><a href='{{url:app:home}}'>Home</a> · <a href='{{url:todos:todos}}'>My Todos</a></nav>
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

The todos templates move into `modules/todos/` and point their links at the new module:

**`modules/todos/todos.html`**
```diff
-    <form method='post' action='{{url:app:todos}}'>
+    <form method='post' action='{{url:todos:todos}}'>
 ...
-        <li><a href='{{url:app:todo}}'>{{title}}</a></li>
+        <li><a href='{{url:todos:todo}}'>{{title}}</a></li>
```

The todos logic moves into the module unchanged, and both todo resources move with it:

**`modules/todos/module.c`**
```c
#include <nerack.h>
#include <http.h>
#include <sqlite.h>
#include <task.h>

module(){
  sqlite(
    "todos_db",
    "file:todos.db?mode=rwc",
    {"create_todos_table", "create_comments_table", "create_daily_stats_table"},
    {"seed_todos"}
  );

  routine("record_daily_stats", {
    sql_query({"todos_db", "record_daily_stats"})
  });
  cron("record_daily_stats", "0 0 * * *");

  routine("notify_new_todo", {
    http_fetch({
      http_post,
      "https://api.push.dev/notify",
      .json = "{\"text\":\"New todo: {{title}}\"}"
    })
  }, .accepts = {"title"});

  http("todos", "/todos",
    .get = {
      sql_query({"todos_db", "get_todos", "todos_data"}),
      html("todos", "todos_s"),
      http_response("todos_s")
    },
    .post = {
      validate({"input:title", not_empty_valid}),
      sql_query({"todos_db", "create_todo"}),
      dispatch("notify_new_todo"),
      http_redirect("todos")
    },
    .errors = {
      {http_bad_request, {http_reroute("todos")}}
    }
  );

  http("todo", "/todos/:id",
    .get = {
      validate({"input:id", positive_integer_valid}),
      sql_query(
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

### 8. Events

Modules talk through events: one module publishes an event, and any module subscribes to it by name, so neither depends on the other. This section adds an `activity` module that records an entry whenever a todo is created:

```
.
├── app.c
├── home.html
└── modules/
    ├── todos/
    │   └── ...
    └── activity/
        ├── module.c
        ├── activity.html
        ├── create_activity_table.sql
        ├── get_activities.sql
        └── insert_activity.sql
```

The todos module declares the event with `publish()`, naming the keys it carries, and fires it with `emit()` after creating a todo:

**`modules/todos/module.c`**
```diff
 #include <nerack.h>
 #include <http.h>
 #include <sqlite.h>
+#include <pubsub.h>
 #include <task.h>

 module(){
   sqlite(
     "todos_db",
     "file:todos.db?mode=rwc",
     {"create_todos_table", "create_comments_table", "create_daily_stats_table"},
     {"seed_todos"}
   );

+  publish("todo_created",
+    .with = {"title"}
+  );
+
 ...
     .post = {
       validate({"input:title", not_empty_valid}),
       sql_query({"todos_db", "create_todo"}),
       dispatch("notify_new_todo"),
+      emit("todo_created"),
       http_redirect("todos")
     },
 ...
```

The `activity` module owns its own table, query, template, and subscriber. Nothing in it references the todos module:

**`modules/activity/create_activity_table.sql`**
```sql
CREATE TABLE IF NOT EXISTS activities (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  kind TEXT NOT NULL,
  ref TEXT NOT NULL,
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

**`modules/activity/insert_activity.sql`**
```sql
insert into activities(kind, ref) values('created', {{title}});
```

**`modules/activity/get_activities.sql`**
```sql
select kind, ref, created_at from activities order by created_at desc;
```

**`modules/activity/activity.html`**
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

Add an Activity link to the nav:

**`home.html`**
```diff
-    <nav><a href='{{url:app:home}}'>Home</a> · <a href='{{url:todos:todos}}'>My Todos</a></nav>
+    <nav><a href='{{url:app:home}}'>Home</a> · <a href='{{url:todos:todos}}'>My Todos</a> · <a href='{{url:activity:activity}}'>Activity</a></nav>
```

**`modules/activity/module.c`**
```c
#include <nerack.h>
#include <http.h>
#include <sqlite.h>
#include <pubsub.h>

module(){
  sqlite(
    "activity_db",
    "file:activity.db?mode=rwc",
    {"create_activity_table"}
  );

  subscribe("todo_created", {
    sql_query({"activity_db", "insert_activity"})
  });

  http("activity", "/activity",
    .get = {
      sql_query({"activity_db", "get_activities", "activities"}),
      html("activity", "activity_s"),
      http_response("activity_s")
    }
  );
}
```

When the POST calls `emit("todo_created")`, Nerack publishes `todo_created` and propagates the keys named in `publish(...).with` (`title`) to every subscriber. The `activity` module subscribes by event name alone, so it does not depend on which module publishes it. Events are durable: undelivered ones replay after a crash. A third subscriber is another module with its own `subscribe(...)`, and the publisher still does not change.

### 9. Deploy

`nerack` always runs in dev mode. For production, `nerack build` compiles the app into one optimized Linux binary, `output/app.x86_64`, with every module and asset embedded (for arm64 servers, list `aarch64` in the project's `.nerack/arch`). The production build is strict: a warning that dev mode let through fails it.
```bash
nerack build
```

`output/app.x86_64` needs nothing else on the server, not even Docker: copy it there and run it, with `COOKIE_SECRET` set to sign cookies and `DATA_DIR` pointing where the databases should live. This step ships an image instead: `nerack docker` builds the same binary into a Docker image named after the folder and the architecture, `todos:latest-x86_64`; it needs Docker (or Podman through its `docker` command, from the `podman-docker` package). The first run writes two files at the project root: a `Dockerfile` that puts the app on Alpine, and a `compose.yml` that runs the image on port 3000 with a `/data` volume, where the databases (`todos.db`, `activity.db`) survive new releases. Both are yours from then on: commit them, and change them as the app grows.
```bash
nerack docker
```

Production signs cookies with `COOKIE_SECRET` and refuses to start without it. `compose.yml` hands it to the app as a Docker secret taken from the environment `docker compose` runs in, so it never sits in a file; on a server, the platform's secret store sets it:
```bash
COOKIE_SECRET=$(openssl rand -hex 32) docker compose up
```

The app now serves on port 3000. Put a reverse proxy such as Caddy or Nginx in front of it for HTTPS.

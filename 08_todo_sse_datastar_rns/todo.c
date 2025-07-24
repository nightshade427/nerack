#include <nerack.h>
#include <sqlite.h>
#include <http.h>
#include <cookie_auth.h>
#include <datastar.h>
#include <rns.h>
#include <rns_auth.h>

module(todo){
  sql_template("create_todos_table",
    "CREATE TABLE IF NOT EXISTS todos ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "user_id INTEGER NOT NULL,"
      "title TEXT NOT NULL,"
      "finished INTEGER CHECK(finished IN (1))"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_todos_user_id ON todos(user_id);"
  );
  sqlite(
    "todos_db",
    "file:todo.db?mode=rwc",
    {"create_todos_table"}
  );

  sql_template("get_todos",
    "select id, title, finished "
    "from todos "
    "where user_id = {{user_id}};"
  );
  sql_template("create_todo",
    "insert into todos(user_id, title)"
    "values({{user_id}}, {{title}})"
    "returning id, title, finished;"
  );
  sql_template("update_todo",
    "update todos "
    "set finished = {{finished}} "
    "where user_id = {{user_id}} "
      "and id = {{id}} "
    "returning id, title, finished;"
  );
  sql_template("delete_todo",
    "delete from todos "
    "where user_id = {{user_id}} "
      "and id = {{id}} "
    "returning id;"
  );
  html_template("layout",
    "<html>"
      "<head>"
        "<link rel='icon' href='{{asset:favicon.png}}'>"
        "{{$head}}"
        "{{/head}}"
      "</head>"
      "<body>"
        "<p>"
          "{{^user}}"
            "<a href='{{url:cookie_auth:login}}'>sign in</a>"
          "{{/user}}"
          "{{#user}}"
            "welcome, {{short_name}}"
          "{{/user}}"
        "</p>"
        "<nav>"
          "<a href='{{url:todo:home}}'>home</a> "
          "<a href='{{url:todo:about}}'>about us</a> "
          "<a href='{{url:todo:contact}}'>contact us</a> "
          "<a href='{{url:todo:todos}}'>todos</a>"
        "</nav>"
        "{{$body}}"
        "{{/body}}"
      "</body>"
    "</html>"
  );
  html_template("404",
    "{{< layout}}"
      "{{$body}}"
        "<p>not found</p>"
      "{{/body}}"
    "{{/layout}}"
  );
  html_template("500",
    "{{< layout}}"
      "{{$body}}"
        "<p>error</p>"
      "{{/body}}"
    "{{/layout}}"
  );
  html_template("todo_item",
    "<div id='todo_{{id}}'>"
      "{{^finished}}"
        "<input type='checkbox' data-on:click__prevent=\"@patch('{{url:todo:todo:finished=1}}')\"> "
      "{{/finished}}"
      "{{#finished}}"
        "<input type='checkbox' checked data-on:click__prevent=\"@patch('{{url:todo:todo}}')\"> "
      "{{/finished}}"
      "{{title}} "
      "<button data-on:click=\"@delete('{{url:todo:todo}}')\">delete</button>"
    "</div>"
  );
  html_template("todo",
    "{{#todo_data}}"
      "{{> todo_item}}"
    "{{/todo_data}}"
  );
  micron_template("layout",
    ">Nerack Todos\n"
    "\n"
    "{{#user}}welcome, {{short_name}}{{/user}}{{^user}}`[sign in`{{url:rns_auth:login}}]{{/user}}\n"
    "\n"
    "`[home`{{url:todo:home}}]  `[about us`{{url:todo:about}}]  `[contact us`{{url:todo:contact}}]  `[todos`{{url:todo:todos}}]\n"
    "\n"
    "-\n"
    "\n"
    "{{$body}}"
    "{{/body}}"
  );
  micron_template("404",
    "{{< layout}}"
      "{{$body}}"
        "not found\n"
      "{{/body}}"
    "{{/layout}}"
  );
  micron_template("500",
    "{{< layout}}"
      "{{$body}}"
        "error\n"
      "{{/body}}"
    "{{/layout}}"
  );

  html_template("home",
    "{{< layout}}"
      "{{$body}}"
        "<p>home</p>"
      "{{/body}}"
    "{{/layout}}"
  );
  http("home", "/",
    .get = {
      cookie_session(),
      html("home", "home_s"),
      http_response("home_s")
    }
  );

  html_template("about",
    "{{< layout}}"
      "{{$body}}"
        "<p>about us</p>"
      "{{/body}}"
    "{{/layout}}"
  );
  http("about", "/about",
    .get = {
      cookie_session(),
      html("about", "about_s"),
      http_response("about_s")
    }
  );

  html_template("contact",
    "{{< layout}}"
      "{{$body}}"
        "<p>contact us</p>"
      "{{/body}}"
    "{{/layout}}"
  );
  http("contact", "/contact",
    .get = {
      cookie_session(),
      html("contact", "contact_s"),
      http_response("contact_s")
    }
  );

  html_template("todos",
    "{{< layout}}"
      "{{$head}}"
        "{{> datastar }}"
      "{{/head}}"
      "{{$body}}"
        "<input type='text' placeholder='new todo' data-bind:title "
          "data-on:keydown=\"evt.key === 'Enter' && @post('{{url:todo:todos}}') && ($title = '');\""
        "> "
        "<button data-on:click=\"@post('{{url:todo:todos}}') && ($title = '')\">add</button>"
        "<p id='todo_error'></p>"
        "<div id='todos' data-init=\"@get('{{sse:todo:todos}}')\">"
          "{{#todos_data}}"
            "{{> todo_item}}"
          "{{/todos_data}}"
        "</div>"
      "{{/body}}"
    "{{/layout}}"
  );
  html_template("todo_error",
    "<p id='todo_error'>{{error_message:title}}</p>"
  );
  http("todos", "/todos",
    .all = {
      cookie_logged_in(),
      cookie_session()
    },
    .sse = {"todos:{{user_id}}"},
    .get = {
      sqlite_query({"todos_db", "get_todos", "todos_data"}),
      html("todos", "todos_s"),
      http_response("todos_s")
    },
    .post = {
      validate({"input:title", not_empty_valid, "cannot be empty"}),
      sqlite_query({"todos_db", "create_todo", "todo_data", .error_on_empty = true}),
      html("todo", "todo_s"),
      datastar("todos:{{user_id}}", .target = "#todos", .mode = prepend_mode, .elements = "todo_s"),
      html("todo_error", "todo_error_s"),
      datastar(.target = "#todo_error", .elements = "todo_error_s")
    },
    .errors = {
      {http_bad_request, {
        html("todo_error", "todo_error_s"),
        datastar(.target = "#todo_error", .elements = "todo_error_s")
      }}
    }
  );

  http("todo", "/todos/:id",
    .all = {
      cookie_logged_in(),
      cookie_session(),
      validate({"input:id", positive_integer_valid})
    },
    .patch = {
      validate({"input:finished", "^1$", "must be 1", .optional = true}),
      sqlite_query({"todos_db", "update_todo", "todo_data", .error_on_empty = true}),
      html("todo", "todo_s"),
      datastar("todos:{{user_id}}", .target = "#todo_{{id}}", .mode = replace_mode, .elements = "todo_s")
    },
    .delete = {
      sqlite_query({"todos_db", "delete_todo", .error_on_empty = true}),
      datastar("todos:{{user_id}}", .target = "#todo_{{id}}", .mode = remove_mode)
    }
  );

  micron_template("home",
    "{{< layout}}"
      "{{$body}}"
        "home\n"
      "{{/body}}"
    "{{/layout}}"
  );
  rns("home", "/",
    .get = {
      rns_session(),
      micron("home", "home_s"),
      rns_response("home_s")
    }
  );

  micron_template("about",
    "{{< layout}}"
      "{{$body}}"
        "about us\n"
      "{{/body}}"
    "{{/layout}}"
  );
  rns("about", "/about",
    .get = {
      rns_session(),
      micron("about", "about_s"),
      rns_response("about_s")
    }
  );

  micron_template("contact",
    "{{< layout}}"
      "{{$body}}"
        "contact us\n"
      "{{/body}}"
    "{{/layout}}"
  );
  rns("contact", "/contact",
    .get = {
      rns_session(),
      micron("contact", "contact_s"),
      rns_response("contact_s")
    }
  );

  micron_template("todos",
    "{{< layout}}"
      "{{$body}}"
        ">todos\n"
        "\n"
        "`<title`new todo>  `[add`{{url:todo:todos}}`{{method:post}}|title]\n"
        "{{#error:title}}{{error_message:title}}{{/error:title}}\n"
        "\n"
        "-\n"
        "\n"
        "{{^todos_data}}"
          "no todos\n"
        "{{/todos_data}}"
        "{{#todos_data}}"
          "{{> todo_item}}"
        "{{/todos_data}}"
      "{{/body}}"
    "{{/layout}}"
  );
  micron_template("todo_item",
    "{{^finished}}`[mark done`{{url:todo:todo}}`{{method:patch}}|finished=1]{{/finished}}{{#finished}}`[mark undone`{{url:todo:todo}}`{{method:patch}}]{{/finished}}  {{title}}  `[delete`{{url:todo:todo}}`{{method:delete}}]\n"
  );
  rns("todos", "/todos",
    .all = {
      rns_logged_in(),
      rns_session()
    },
    .get = {
      sqlite_query({"todos_db", "get_todos", "todos_data"}),
      micron("todos", "todos_s"),
      rns_response("todos_s")
    },
    .post = {
      validate({"input:title", not_empty_valid, "cannot be empty"}),
      sqlite_query({"todos_db", "create_todo", "todo_data", .error_on_empty = true}),
      html("todo", "todo_s"),
      datastar("todos:{{user_id}}", .target = "#todos", .mode = prepend_mode, .elements = "todo_s"),
      rns_reroute("todos")
    },
    .errors = {
      {err_invalid, {rns_reroute("todos")}}
    }
  );

  rns("todo", "/todos/:id",
    .all = {
      rns_logged_in(),
      rns_session(),
      validate({"input:id", positive_integer_valid})
    },
    .patch = {
      validate({"input:finished", "^1$", "must be 1", .optional = true}),
      sqlite_query({"todos_db", "update_todo", "todo_data", .error_on_empty = true}),
      html("todo", "todo_s"),
      datastar("todos:{{user_id}}", .target = "#todo_{{id}}", .mode = replace_mode, .elements = "todo_s"),
      rns_reroute("todos")
    },
    .delete = {
      sqlite_query({"todos_db", "delete_todo", .error_on_empty = true}),
      datastar("todos:{{user_id}}", .target = "#todo_{{id}}", .mode = remove_mode),
      rns_reroute("todos")
    }
  );
}

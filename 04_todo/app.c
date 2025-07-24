#include <nerack.h>
#include <http.h>
#include <sqlite.h>
#include <cookie_auth.h>

module(){
  sql_template("create_todos_table",
    "CREATE TABLE IF NOT EXISTS todos ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "title TEXT NOT NULL,"
      "finished INTEGER CHECK(finished IN (1))"
    ");"
  );
  sqlite(
    "todos_db",
    "file:{{user_id}}_todo.db?mode=rwc",
    {"create_todos_table"}
  );

  html_template("layout",
    "<html>"
      "<head>"
        "<link rel='icon' href='{{asset:favicon.png}}'>"
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
          "<a href='{{url:app:home}}'>home</a> "
          "<a href='{{url:app:about}}'>about us</a> "
          "<a href='{{url:app:contact}}'>contact us</a> "
          "<a href='{{url:app:todos}}'>todos</a>"
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

  sql_template("get_todos",
    "select id, title, finished "
    "from todos;"
  );
  sql_template("create_todo",
    "insert into todos(title)"
    "values({{title}});"
  );
  html_template("todos",
    "{{< layout}}"
      "{{$body}}"
        "<form action='{{url:app:todos}}' method='post'>"
          "<input type='text' name='title' placeholder='new todo' value='{{input:title}}' required> "
          "<button type='submit'>add</button>"
          "{{#error:title}}"
            "<p>{{error_message:title}}</p>"
          "{{/error:title}}"
        "</form>"
        "{{^todos_data}}"
          "<p>no todos</p>"
        "{{/todos_data}}"
        "{{#todos_data}}"
          "<div>"
            "<form action='{{url:app:todo}}' method='post' style='display:inline'>"
              "{{method:patch}}"
              "{{^finished}}"
                "<input type='checkbox' name='finished' value='1'> "
              "{{/finished}}"
              "{{#finished}}"
                "<input type='checkbox' name='finished' value='1' checked> "
              "{{/finished}}"
              "{{title}} "
              "<button type='submit'>save</button>"
            "</form> "
            "<form action='{{url:app:todo}}' method='post' style='display:inline'>"
              "{{method:delete}}"
              "<button type='submit'>delete</button>"
            "</form>"
          "</div>"
        "{{/todos_data}}"
      "{{/body}}"
    "{{/layout}}"
  );
  http("todos", "/todos",
    .all = {
      cookie_logged_in(),
      cookie_session()
    },
    .get = {
      sqlite_query({"todos_db", "get_todos", "todos_data"}),
      html("todos", "todos_s"),
      http_response("todos_s")
    },
    .post = {
      validate({"input:title", not_empty_valid, "cannot be empty"}),
      sqlite_query({"todos_db", "create_todo"}),
      http_redirect("todos")
    },
    .errors = {
      {http_bad_request, {http_reroute("todos")}}
    }
  );

  sql_template("update_todo",
    "update todos "
    "set finished = {{finished}} "
    "where id = {{id}};"
  );
  sql_template("delete_todo",
    "delete from todos "
    "where id = {{id}};"
  );
  http("todo", "/todos/:id",
    .all = {
      cookie_logged_in(),
      cookie_session(),
      validate({"input:id", positive_integer_valid})
    },
    .patch = {
      validate({"input:finished", "^1$", "must be 1", .optional = true}),
      sqlite_query({"todos_db", "update_todo"}),
      http_redirect("todos")
    },
    .delete = {
      sqlite_query({"todos_db", "delete_todo"}),
      http_redirect("todos")
    }
  );
}

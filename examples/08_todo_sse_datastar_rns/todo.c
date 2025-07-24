#include <nerack.h>
#include <sqlite.h>
#include <http.h>
#include <cookie_auth.h>
#include <datastar.h>
#include <rns.h>

module(todo){
  sqlite(
    "todos_db",
    "file:todo.db?mode=rwc",
    {"create_todos_table"}
  );

  http("http_home", "/",
    .get = {
      cookie_session(),
      html("home", "home_s"),
      http_response("home_s")
    }
  );

  http("http_about", "/about",
    .get = {
      cookie_session(),
      html("about", "about_s"),
      http_response("about_s")
    }
  );

  http("http_contact", "/contact",
    .get = {
      cookie_session(),
      html("contact", "contact_s"),
      http_response("contact_s")
    }
  );

  http("http_todos", "/todos",
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
      input({"title", not_empty_input}),
      sqlite_query({"todos_db", "create_todo", "todo_data", .error_on_empty = true}),
      html("todo", "todo_s"),
      datastar("todos:{{user_id}}", .target = "#todos", .mode = prepend_mode, .elements = "todo_s")
    }
  );

  http("http_todo", "/todos/:id",
    .all = {
      cookie_logged_in(),
      cookie_session(),
      input({"id", positive_integer_input})
    },
    .patch = {
      input({"finished", "^1$", "must be 1", .optional = true}),
      sqlite_query({"todos_db", "update_todo", "todo_data", .error_on_empty = true}),
      html("todo", "todo_s"),
      datastar("todos:{{user_id}}", .target = "#todo_{{id}}", .mode = replace_mode, .elements = "todo_s")
    },
    .delete = {
      sqlite_query({"todos_db", "delete_todo", .error_on_empty = true}),
      datastar("todos:{{user_id}}", .target = "#todo_{{id}}", .mode = remove_mode)
    }
  );

  rns("rns_home", "/",
    .get = {
      rns_identity(),
      micron("home", "home_s"),
      rns_response("home_s")
    }
  );

  rns("rns_about", "/about",
    .get = {
      rns_identity(),
      micron("about", "about_s"),
      rns_response("about_s")
    }
  );

  rns("rns_contact", "/contact",
    .get = {
      rns_identity(),
      micron("contact", "contact_s"),
      rns_response("contact_s")
    }
  );

  rns("rns_todos", "/todos",
    .all = {rns_identity()},
    .get = {
      sqlite_query({"todos_db", "get_todos", "todos_data"}),
      micron("todos", "todos_s"),
      rns_response("todos_s")
    },
    .post = {
      input({"title", not_empty_input}),
      sqlite_query({"todos_db", "create_todo", "todo_data", .error_on_empty = true}),
      html("todo", "todo_s"),
      datastar("todos:{{user_id}}", .target = "#todos", .mode = prepend_mode, .elements = "todo_s"),
      rns_reroute("rns_todos")
    }
  );

  rns("rns_todo", "/todos/:id",
    .all = {
      rns_identity(),
      input({"id", positive_integer_input})
    },
    .patch = {
      input({"finished", "^1$", "must be 1", .optional = true}),
      sqlite_query({"todos_db", "update_todo", "todo_data", .error_on_empty = true}),
      html("todo", "todo_s"),
      datastar("todos:{{user_id}}", .target = "#todo_{{id}}", .mode = replace_mode, .elements = "todo_s"),
      rns_reroute("rns_todos")
    },
    .delete = {
      sqlite_query({"todos_db", "delete_todo", .error_on_empty = true}),
      datastar("todos:{{user_id}}", .target = "#todo_{{id}}", .mode = remove_mode),
      rns_reroute("rns_todos")
    }
  );
}

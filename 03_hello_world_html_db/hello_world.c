#include <nerack.h>
#include <http.h>
#include <sqlite.h>

module(hello_world){
  sql_template("create_hello_world_table",
    "CREATE TABLE IF NOT EXISTS greetings ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "name TEXT NOT NULL"
    ");"
  );
  sql_template("seed_hello_world",
    "INSERT INTO greetings(name)"
    "VALUES('World');"
  );
  sqlite(
    "hello_db",
    "file:hello.db?mode=rwc",
    {"create_hello_world_table"},
    {"seed_hello_world"}
  );

  sql_template("get_hello_world",
    "select name "
    "from greetings "
    "limit 1;"
  );
  html_template("hello_world",
    "<html>"
      "<body>"
        "{{#hello_world_data}}"
          "<p>Hello {{name}}</p>"
        "{{/hello_world_data}}"
      "</body>"
    "</html>"
  );
  http("home", "/",
    .get = {
      sqlite_query({"hello_db", "get_hello_world", "hello_world_data"}),
      html("hello_world", "hello"),
      http_response("hello")
    }
  );
}

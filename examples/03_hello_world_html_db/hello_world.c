#include <nerack.h>
#include <http.h>
#include <sqlite.h>

module(hello_world){
  sqlite(
    "hello_db",
    "file:hello.db?mode=rwc",
    {"create_hello_world_table"},
    {"seed_hello_world"}
  );

  http("home", "/",
    .get = {
      sqlite_query({"hello_db", "get_hello_world", "hello_world_data"}),
      html("hello_world", "hello"),
      http_response("hello")
    }
  );
}

#include <nerack.h>
#include <http.h>
#include <htmx.h>
#include <sqlite.h>
#include <tailwind.h>

module(roundest){
  sqlite(
    "pokemon_db",
    "file::memory:?cache=shared",
    {"create_pokemons_table"},
    {"seed_pokemons"}
  );

  http("home", "/",
    .get = {
      sqlite_query({"pokemon_db", "get_challengers", "challengers"}),
      run(^(){
        auto const t = get("challengers");
        auto const p0 = table_get(t, 0);
        auto const p1 = table_get(t, 1);
        record_set(p0, "opponent_id", record_get(p1, "id"));
        record_set(p1, "opponent_id", record_get(p0, "id"));
      }),
      html("home", "home_s"),
      http_response("home_s")
    },
    .post = {
      input(
        {"winner", positive_integer_input},
        {"loser", positive_integer_input}
      ),
      sqlite_query({"pokemon_db", "vote"}),
      http_redirect("home")
    }
  );

  http("results", "/results",
    .get = {
      sqlite_query({"pokemon_db", "get_results", "results_data"}),
      html("results", "results_s"),
      http_response("results_s")
    }
  );
}

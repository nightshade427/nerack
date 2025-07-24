#include <nerack.h>
#include <http.h>
#include <htmx.h>
#include <sqlite.h>
#include <tailwind.h>

module(roundest){
  sql_template("create_pokemons_table",
    "CREATE TABLE IF NOT EXISTS pokemons ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "name TEXT NOT NULL,"
      "sprite TEXT NOT NULL,"
      "wins INTEGER NOT NULL DEFAULT 0,"
      "losses INTEGER NOT NULL DEFAULT 0"
    ");"
  );
  sql_template("seed_pokemons",
    "INSERT INTO pokemons(name, sprite) VALUES"
      "('bulbasaur',  'https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/1.png'),"
      "('charmander', 'https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/4.png'),"
      "('squirtle',   'https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/7.png'),"
      "('caterpie',   'https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/10.png'),"
      "('pidgey',     'https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/16.png'),"
      "('pikachu',    'https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/25.png'),"
      "('jigglypuff', 'https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/39.png'),"
      "('meowth',     'https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/52.png'),"
      "('psyduck',    'https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/54.png'),"
      "('machop',     'https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/66.png'),"
      "('magnemite',  'https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/81.png'),"
      "('gastly',     'https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/92.png'),"
      "('onix',       'https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/95.png'),"
      "('voltorb',    'https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/100.png'),"
      "('eevee',      'https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/133.png'),"
      "('snorlax',    'https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/143.png');"
  );
  sqlite(
    "pokemon_db",
    "file::memory:?cache=shared",
    {"create_pokemons_table"},
    {"seed_pokemons"}
  );

  html_template("home",
    "<!DOCTYPE html>"
    "<html>"
      "<head>"
        "<title>Roundest (Nerack Version)</title>"
        "{{> htmx }}"
        "{{> tailwind }}"
      "</head>"
      "<body hx-boost='true'>"
        "<div class='bg-gray-950 text-white flex flex-col justify-between font-geist min-h-screen border-t-2 border-red-300'>"
          "<nav class='flex flex-row justify-between items-center py-4 px-8'>"
            "<div class='flex items-baseline'>"
              "<a href='{{url:roundest:home}}' class='font-bold text-3xl'>"
                "Round<span class='text-red-300'>est</span>"
              "</a>"
              "<p class='text-gray-400 font-extralight pl-2 text-2xl'>"
                "(Nerack Version)"
              "</p>"
            "</div>"
            "<div class='flex flex-row items-center gap-8'>"
              "<a href='{{url:roundest:results}}' class='hover:underline text-lg'>Results</a>"
            "</div>"
          "</nav>"
          "<main>"
            "{{$body}}"
              "<div class='container mx-auto px-4'>"
                "<h1 class='text-3xl font-bold text-center mb-8'>Vote for which is roundest</h1>"
                "<div class='flex justify-center gap-16 items-center min-h-[80vh]'>"
                  "{{#challengers}}"
                    "<form action='{{url:roundest:home}}' method='post'>"
                      "<div class='flex flex-col items-center gap-4'>"
                        "<img src='{{sprite}}' alt='{{name}}' class='w-64 h-64 sprite'>"
                        "<div class='text-center'>"
                          "<span class='text-gray-500 text-lg'>#{{id}}</span>"
                          "<h2 class='text-2xl font-bold capitalize'>{{name}}</h2>"
                          "<input type='hidden' name='winner' value='{{id}}'>"
                          "<input type='hidden' name='loser' value='{{opponent_id}}'>"
                          "<button class='mt-4 px-8 py-3 bg-blue-500 text-white rounded-lg text-lg font-semibold hover:bg-blue-600 transition-colors'>"
                            "Vote"
                          "</button>"
                        "</div>"
                      "</div>"
                    "</form>"
                  "{{/challengers}}"
                "</div>"
              "</div>"
            "{{/body}}"
          "</main>"
          "<footer class='font-light text-center py-3 text-gray-500'>"
            "<a href='https://code.nightshadecoder.dev/nightshade/nerack' target='_blank'>Gitea</a>"
          "</footer>"
        "</div>"
      "</body>"
    "</html>"
  );

  sql_template("get_challengers",
    "select id, name, sprite "
    "from pokemons "
    "order by random()"
    "limit 2;"
  );
  sql_template("vote",
    "begin transaction;"
    "update pokemons "
      "set wins = wins + 1 "
      "where id = {{winner}};"
    "update pokemons "
      "set losses = losses + 1 "
      "where id = {{loser}};"
    "commit;"
  );
  http("home", "/",
    .get = {
      sqlite_query({"pokemon_db", "get_challengers", "challengers"}),
      run(^(){
        table_handle const t = get("challengers");
        record_handle const p0 = table_get(t, 0);
        record_handle const p1 = table_get(t, 1);
        record_set_string(p0, "opponent_id", record_get(p1, "id"));
        record_set_string(p1, "opponent_id", record_get(p0, "id"));
      }),
      html("home", "home_s"),
      http_response("home_s")
    },
    .post = {
      validate(
        {"input:winner", positive_integer_valid},
        {"input:loser", positive_integer_valid}
      ),
      sqlite_query({"pokemon_db", "vote"}),
      http_redirect("home")
    }
  );

  sql_template("get_results",
    "select id, name, sprite, wins, losses,"
      "round(100.0 * wins / nullif(wins + losses, 0), 1) as win_pct,"
      "row_number() over ("
        "order by 1.0 * wins / nullif(wins + losses, 0) desc nulls last,"
          "wins + losses desc,"
          "name"
      ") as rank "
    "from pokemons "
    "order by rank;"
  );
  html_template("results",
    "{{< home}}"
      "{{$body}}"
        "<div class='container mx-auto px-4 py-8 text-white'>"
          "<div class='grid gap-4'>"
            "{{#results_data}}"
              "<div class='flex items-center gap-6 p-6 bg-gray-800/40 rounded-lg shadow hover:shadow-md transition-shadow'>"
                "<div class='text-2xl font-bold text-gray-400 w-8'>"
                  "{{rank}}"
                "</div>"
                "<img src='{{sprite}}' alt='{{name}}' loading='lazy' class='w-20 h-20'>"
                "<div class='flex-grow'>"
                  "<div class='text-gray-400 text-sm'>#{{id}}</div>"
                  "<h2 class='text-xl font-semibold capitalize'>{{name}}</h2>"
                "</div>"
                "<div class='text-right'>"
                  "<div class='text-2xl font-bold text-blue-400'>"
                    "{{#win_pct}}{{win_pct}}%{{/win_pct}}{{^win_pct}}n/a{{/win_pct}}"
                  "</div>"
                  "<div class='text-sm text-gray-400'>"
                    "{{wins}}W - {{losses}}L"
                  "</div>"
                "</div>"
              "</div>"
            "{{/results_data}}"
          "</div>"
        "</div>"
      "{{/body}}"
    "{{/home}}"
  );
  http("results", "/results",
    .get = {
      sqlite_query({"pokemon_db", "get_results", "results_data"}),
      html("results", "results_s"),
      http_response("results_s")
    }
  );
}

select id, name, sprite, wins, losses,
 round(100.0 * wins / nullif(wins + losses, 0), 1) as win_pct,
 row_number() over (
   order by 1.0 * wins / nullif(wins + losses, 0) desc nulls last,
            wins + losses desc,
            name
 ) as rank
from pokemons
order by rank;

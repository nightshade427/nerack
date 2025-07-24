begin transaction;
update pokemons
 set wins = wins + 1
 where id = {{winner}};
update pokemons
 set losses = losses + 1
 where id = {{loser}};
commit;

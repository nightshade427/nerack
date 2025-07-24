update todos
set finished = {{finished}}
where user_id = {{user_id}}
  and id = {{id}}
returning id, title, finished;

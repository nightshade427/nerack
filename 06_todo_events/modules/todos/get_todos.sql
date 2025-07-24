select id, title, finished
from todos
where user_id = {{user_id}};

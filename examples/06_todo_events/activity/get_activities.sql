select action, title, created_at
from activity
where user_id = {{user_id}}
order by created_at desc
limit 50;

#include <nerack.h>
#include <http.h>
#include <sqlite.h>
#include <pubsub.h>
#include <cookie_auth.h>

module(activity){
  sqlite(
    "activity_db",
    "file:activity.db?mode=rwc",
    {"create_activity_table"}
  );

  subscribe("todo_created", {
    sqlite_query({"activity_db", "insert_activity"})
  });

  http("activity", "/activity",
    .get = {
      cookie_logged_in(),
      cookie_session(),
      sqlite_query({"activity_db", "get_activities", "activities"}),
      html("activity", "activity_s"),
      http_response("activity_s")
    }
  );
}

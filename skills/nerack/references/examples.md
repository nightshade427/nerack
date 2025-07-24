# Examples

## Example: modules and events

```c
// modules/todos/get_todos.sql    select id, title from todos;
// modules/todos/get_todo.sql     select id, title from todos where id = {{id}};
// modules/todos/get_comments.sql select id, todo_id, body from comments where todo_id = {{id}};
// modules/todos/create_todo.sql  insert into todos(title) values({{title}});
// modules/todos/todos.html
//   {{< layout}}{{$body}}
//     <form method='post' action='{{url:todos:todos}}'>
//       <input name='title' value='{{input:title}}'>
//       {{#error:title}}<span>{{error_message:title}}</span>{{/error:title}}
//     </form>
//     <ul>{{#todos_data}}<li><a href='{{url:todos:todo}}'>{{title}}</a></li>{{/todos_data}}</ul>
//   {{/body}}{{/layout}}

// modules/todos/module.c
#include <nerack.h>
#include <http.h>
#include <sqlite.h>
#include <pubsub.h>

module(){
  sqlite("todos_db", "file:todos.db?mode=rwc", {"create_todos_table"}, {"seed_todos"});
  publish("todo_created", .with = {"title"});

  http("todos", "/todos",
    .get = {
      sql_query({"todos_db", "get_todos", "todos_data"}),
      html("todos", "todos_s"),
      http_response("todos_s")
    },
    .post = {
      validate({"input:title", not_empty_valid, "cannot be empty"}),
      sql_query({"todos_db", "create_todo"}),
      emit("todo_created"),
      http_redirect("todos")                                  // POST-redirect-GET
    },
    .errors = {{http_bad_request, {http_reroute("todos")}}}   // re-render the form with input: and error:
  );

  http("todo", "/todos/:id",
    .all = {validate({"input:id", positive_integer_valid})},
    .get = {
      sql_query(                                           // both run concurrently
        {"todos_db", "get_todo", "todo_data", .error_on_empty = true},
        {"todos_db", "get_comments", "comments"}
      ),
      join("todo_data", "id", "comments", "todo_id"),
      html("todo", "todo_s"),
      http_response("todo_s")
    }
  );
}

// modules/activity/module.c: a subscriber, independent of todos
#include <nerack.h>
#include <sqlite.h>
#include <pubsub.h>

module(){
  sqlite("activity_db", "file:activity.db?mode=rwc", {"create_activity_table"});
  subscribe("todo_created", {sql_query({"activity_db", "insert_activity"})});
}
```

## Example: live chat over HTTP and RNS

A message posted from a browser (Datastar) or an RNS peer (Micron) is stored once and appended live in every connected browser.
```c
// chat/insert_message.sql  insert into messages(body) values({{body}}) returning id, body;
// chat/message.html        {{#message_d}}<li id='message-{{id}}'>{{body}}</li>{{/message_d}}
// chat/body_error.html     <p id='body_error'>{{error_message:body}}</p>
// chat/room.html
//   {{< layout}}{{$body}}
//     <ul id='messages' data-init="@get('{{sse:chat:room}}')">
//       {{#messages_data}}<li id='message-{{id}}'>{{body}}</li>{{/messages_data}}
//     </ul>
//     <input data-bind:body><button data-on:click="@post('{{url:chat:room}}')">Send</button>
//     <p id='body_error'></p>
//   {{/body}}{{/layout}}
// chat/room.mu
//   {{#messages_data}}{{body}}
//   {{/messages_data}}
//   `<body`>`[send`{{url:chat:room}}`{{method:post}}|body]

// chat/module.c
#include <nerack.h>
#include <http.h>
#include <rns.h>
#include <sqlite.h>
#include <datastar.h>

module(){
  sqlite("chat_db", "file:chat.db?mode=rwc", {"create_messages_table"});

  http("room", "/room",
    .sse = {"room"},                                          // browsers listen here
    .get = {
      sql_query({"chat_db", "get_messages", "messages_data"}),
      html("room", "room_s"),
      http_response("room_s")
    },
    .post = {
      validate({"input:body", not_empty_valid, "cannot be empty"}),
      sql_query({"chat_db", "insert_message", "message_d", .error_on_empty = true}),
      html("message", "message_s"),
      datastar("room", .target = "#messages", .mode = append_mode, .elements = "message_s")
    },                                                        // no response step: empty 200
    .errors = {{http_bad_request, {                           // no channel: only the sender
      html("body_error", "body_error_s"),
      datastar(.target = "#body_error", .elements = "body_error_s")
    }}}
  );

  rns("room", "/room",
    .get = {
      sql_query({"chat_db", "get_messages", "messages_data"}),
      micron("room", "room_s"),
      rns_response("room_s")
    },
    .post = {
      validate({"input:body", not_empty_valid, "cannot be empty"}),
      sql_query({"chat_db", "insert_message", "message_d", .error_on_empty = true}),
      html("message", "message_s"),                           // HTML for the browsers
      datastar("room", .target = "#messages", .mode = append_mode, .elements = "message_s"),
      rns_reroute("room")                                     // Micron back to the peer
    }
  );
}
```

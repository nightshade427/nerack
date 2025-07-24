#include <nerack.h>
#include <http.h>

module(hello){
  http("home", "/", .mime = mime_text,
    .get = {
      text("greeting", "greeting_s"),
      http_response("greeting_s")
    }
  );
}

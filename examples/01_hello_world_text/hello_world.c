#include <nerack.h>
#include <http.h>

module(hello_world){
  http("home", "/", .mime = mime_text,
    .get = {
      validate({"input:name", not_empty_valid, .default_value = "world"}),
      text("greeting", "hello"),
      http_response("hello")
    }
  );
}

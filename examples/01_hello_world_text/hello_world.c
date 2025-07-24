#include <nerack.h>
#include <http.h>

module(hello_world){
  context("greeting", "hello {{name}}");
  http("home", "/", .mime = mime_text,
    .get = {
      input({"name", not_empty_input, .default_value = "world"}),
      html("greeting", "hello"),
      http_response("hello")
    }
  );
}

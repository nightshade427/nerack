#include <nerack.h>
#include <http.h>

module(hello_world){
  http("home", "/", .mime = mime_text,
    .get = {
      validate({"input:name", not_empty_valid, .default_value = "world"}),
      context("hello", "hello {{name}}"),
      http_response("hello")
    }
  );
}

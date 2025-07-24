#include <nerack.h>
#include <http.h>

module(hello_world){
  http("home", "/",
    .get = {
      validate({"input:name", not_empty_valid, .default_value = "world"}),
      html("greeting", "hello"),
      http_response("hello")
    }
  );
}

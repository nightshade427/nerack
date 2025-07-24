#include <nerack.h>
#include <http.h>

module(hello_world){
  context("greeting",
    "<html>"
      "<head></head>"
      "<body>"
        "<p>Hello {{name}}</p>"
      "</body>"
    "</html>"
  );
  http("home", "/",
    .get = {
      input({"name", not_empty_input, .default_value = "world"}),
      html("greeting", "hello"),
      http_response("hello")
    }
  );
}

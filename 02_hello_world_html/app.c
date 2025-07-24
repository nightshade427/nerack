#include <nerack.h>
#include <http.h>

module(){
  html_template("greeting",
    "<html>"
      "<head></head>"
      "<body>"
        "<p>Hello {{name}}</p>"
      "</body>"
    "</html>"
  );
  http("home", "/",
    .get = {
      validate({"input:name", not_empty_valid, .default_value = "world"}),
      html("greeting", "hello"),
      http_response("hello")
    }
  );
}

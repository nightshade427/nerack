#include <nerack.h>
#include <http.h>

module(hello){
  context("greeting", "hello world");
  http("home", "/", .mime = mime_text,
    .get = {
      http_response("greeting")
    }
  );
}

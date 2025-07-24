#include <nerack.h>
#include <http.h>

module(hello){
  http("home", "/", .mime = mime_text,
    .get = {
      context("hello", "hello world"),
      http_response("hello")
    }
  );
}

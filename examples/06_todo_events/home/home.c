#include <nerack.h>
#include <http.h>
#include <cookie_auth.h>

module(home){
  http("home", "/",
    .get = {
      cookie_session(),
      html("home", "home_s"),
      http_response("home_s")
    }
  );
}

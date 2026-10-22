#include "connect_web_security.h"

#include <cassert>
#include <cstdio>

int main(){
  const char token[]="0123456789abcdef0123456789abcdef";
  assert(wxconnect::web_security::validCsrf(token,token));
  assert(!wxconnect::web_security::validCsrf(token,"0123456789abcdef0123456789abcdee"));
  assert(!wxconnect::web_security::validCsrf(token,"short"));
  assert(!wxconnect::web_security::validCsrf(token,nullptr));
  assert(wxconnect::web_security::sameOrigin("http://wxconnect.local","wxconnect.local"));
  assert(wxconnect::web_security::sameOrigin("https://192.168.4.1:8443","192.168.4.1:8443"));
  assert(!wxconnect::web_security::sameOrigin("http://evil.example","wxconnect.local"));
  assert(!wxconnect::web_security::sameOrigin("null","wxconnect.local"));
  assert(wxconnect::web_security::validSessionId("0123456789abcdef0123456789abcdef"));
  assert(!wxconnect::web_security::validSessionId("../0123456789abcdef0123456789"));
  assert(!wxconnect::web_security::validSessionId("0123456789ABCDEF0123456789ABCDEF"));
  std::puts("Connect web security checks passed: CSRF, same-origin and session paths.");
}

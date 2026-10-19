#include "../opg3d/src/main/cpp/url_diagnostics_2313.h"
#include <cassert>
int main() {
    using url_diagnostics_2313::redact;
    assert(redact("") == "");
    assert(redact("http://host/socket.io/") == "http://host/socket.io/");
    assert(redact("http://host/socket.io/?token=secret&x=1") == "http://host/socket.io/?<redacted>");
    assert(redact("http://host/path#secret") == "http://host/path?<redacted>");
    assert(redact("https://host/path?session=x#y").find("session") == std::string::npos);
}

#pragma once
#include <string>

namespace url_diagnostics_2313 {
// Socket/auth URLs can contain reusable bearer credentials. Diagnostics only
// need the origin/path. Mask fragments too; never log a partially parsed query.
inline std::string redact(const std::string& url) {
    const auto pos = url.find_first_of("?#");
    return pos == std::string::npos ? url : url.substr(0, pos) + "?<redacted>";
}
} // namespace url_diagnostics_2313

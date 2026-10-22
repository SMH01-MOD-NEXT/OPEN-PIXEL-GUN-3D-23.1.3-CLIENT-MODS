// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdlib>
#include <string>
namespace launcher::policy {
inline bool valid_hash(const std::string &s) {
  return s.size() == 64 &&
         s.find_first_not_of("0123456789abcdef") == std::string::npos;
}
inline bool endpoint(const std::string &raw, std::string *host, int *port) {
  std::string s = raw;
  if (s.compare(0, 7, "http://") == 0)
    s = s.substr(7);
  auto colon = s.rfind(':');
  if (colon == std::string::npos)
    return false;
  auto h = s.substr(0, colon), p = s.substr(colon + 1);
  if (h.empty() || h.size() > 253 || p.empty() || p.size() > 5 ||
      h.front() == '.' || h.front() == '-' || h.back() == '.' ||
      h.back() == '-' || h.find("..") != std::string::npos)
    return false;
  for (char c : h)
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '.' || c == '-'))
      return false;
  for (char c : p)
    if (c < '0' || c > '9')
      return false;
  int n = std::atoi(p.c_str());
  if (n < 1 || n > 65535)
    return false;
  *host = h;
  *port = n;
  return true;
}
inline bool release_allowed(long long build, long long highest,
                            long long size) {
  return build > 0 && build >= highest && size >= 64 &&
         size <= 32 * 1024 * 1024;
}
} // namespace launcher::policy

// SPDX-License-Identifier: GPL-3.0-or-later
#include "../opg3d/src/main/cpp/loader/policy.h"
#include <cassert>
#include <iostream>
int main() {
  using namespace launcher::policy;
  std::string host;
  int port;
  for (auto s :
       {"192.168.0.107:8080", "example.org:65535", "http://localhost:1"})
    assert(endpoint(s, &host, &port));
  for (auto s : {"host", "host:0", "host:65536", "host:-1", "host:3x",
                 "host:80/path", "host:80\r\nX:2", "https://host:443",
                 "user@host:80", "../host:80", "a..b:80", "host:", ":80"})
    assert(!endpoint(s, &host, &port));
  assert(valid_hash(std::string(64, 'a')));
  assert(!valid_hash(std::string(64, 'A')));
  assert(!valid_hash("../../private"));
  assert(release_allowed(3, 2, 1024));
  assert(!release_allowed(1, 2, 1024));
  assert(!release_allowed(0, 0, 1024));
  assert(!release_allowed(3, 2, 33 * 1024 * 1024));
  assert(!release_allowed(3, 2, 63));
  std::cout << "PASS loader endpoint/hash/rollback/size policy\n";
}

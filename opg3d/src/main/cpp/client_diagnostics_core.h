#pragma once
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>
namespace client_diagnostics_core {
inline std::string clean(const std::string &raw, size_t limit) {
  std::string out;
  size_t pos = 0;
  while (pos < raw.size() && pos < 16384 && out.size() < limit) {
    const size_t end = raw.find('\n', pos);
    std::string line =
        raw.substr(pos, (end == std::string::npos ? raw.size() : end) - pos);
    if (line.size() > 8192)
      line.resize(8192);
    std::string lower = line;
    for (char &c : lower)
      if (c >= 'A' && c <= 'Z')
        c = static_cast<char>(c - 'A' + 'a');
    bool sensitive = false;
    for (const auto key :
         {"authorization", "bearer", "password", "passwd", "secret", "token",
          "session", "appid", "receipt", "cookie", "device_id"})
      if (lower.find(key) != std::string::npos)
        sensitive = true;
    if (sensitive)
      line = "[sensitive line redacted]";
    else {
      std::string safe;
      for (size_t i = 0; i < line.size();) {
        if (static_cast<unsigned char>(line[i]) <= 32) {
          if (line[i] == ' ' || line[i] == '\t')
            safe += ' ';
          ++i;
          continue;
        }
        size_t j = i;
        while (j < line.size() && static_cast<unsigned char>(line[j]) > 32)
          ++j;
        const std::string word = line.substr(i, j - i);
        if (word.find("://") != std::string::npos)
          safe += "[url]";
        else if (word.find('@') != std::string::npos)
          safe += "[email]";
        else if (word.find('/') != std::string::npos ||
                 word.find('\\') != std::string::npos)
          safe += "[path]";
        else {
          size_t run = 0;
          bool opaque = false;
          for (unsigned char c : word) {
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                (c >= '0' && c <= '9') || c == '_' || c == '+' || c == '.' ||
                c == '=' || c == '-') {
              if (++run >= 48)
                opaque = true;
            } else
              run = 0;
          }
          safe += opaque ? "[opaque]" : word;
        }
        i = j;
      }
      line = safe;
    }
    if (!out.empty())
      out += '\n';
    out += line;
    if (end == std::string::npos)
      break;
    pos = end + 1;
  }
  if (out.size() > limit) {
    size_t n = limit;
    while (n && (static_cast<unsigned char>(out[n]) & 0xc0) == 0x80)
      --n;
    out.resize(n);
  }
  return out;
}
inline std::string json(const std::string &value) {
  std::string out = "\"";
  for (unsigned char c : value) {
    if (c == '"' || c == '\\') {
      out += '\\';
      out += static_cast<char>(c);
    } else if (c == '\n')
      out += "\\n";
    else if (c >= 32)
      out += static_cast<char>(c);
  }
  return out + '"';
}
struct Event {
  std::string id, code, level, message, stack, endpoint, player;
  uint64_t revision = 0;
  int attempts = 0;
};
// Bounded, non-blocking producer. Network I/O belongs to the single worker.
struct Queue {
  std::mutex mutex;
  std::deque<Event> pending;
  std::unordered_map<std::string, uint64_t> recent;
  bool push(Event event, uint64_t now) {
    std::unique_lock<std::mutex> lock(mutex, std::try_to_lock);
    if (!lock.owns_lock() || pending.size() >= 32)
      return false;
    event.message = clean(event.message, 1024);
    event.stack = clean(event.stack, 2048);
    const std::string key = event.endpoint + '\n' + event.player + '\n' +
                            event.code + '\n' + event.message + '\n' +
                            event.stack + '\n' + std::to_string(event.revision);
    auto it = recent.find(key);
    if (it != recent.end() && now >= it->second && now - it->second < 30000)
      return false;
    if (recent.size() >= 64) {
      for (auto i = recent.begin(); i != recent.end();) {
        if (now >= i->second && now - i->second >= 30000)
          i = recent.erase(i);
        else
          ++i;
      }
      if (recent.size() >= 64)
        return false;
    }
    recent[key] = now;
    pending.push_back(std::move(event));
    return true;
  }
  bool pop(Event &event) {
    std::lock_guard<std::mutex> lock(mutex);
    if (pending.empty())
      return false;
    event = std::move(pending.front());
    pending.pop_front();
    return true;
  }
  void retry(Event event) {
    std::lock_guard<std::mutex> lock(mutex);
    if (++event.attempts < 3 && pending.size() < 32)
      pending.push_back(std::move(event));
  }
};
} // namespace client_diagnostics_core

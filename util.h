#pragma once
namespace rapid {
namespace internal {
constexpr size_t cstring_length(const char *s) {
  const char *ps = s;
  while (*ps != '\0') ++ps;
  return ps - s;
}
constexpr uint64_t hash_string(const char *ps, size_t len) {
  uint64_t ret = 14695981039346656037ULL;
  size_t step = (len >> 8) + 1;  //²ÉÑù<=256¸ö
  for (size_t i = 0; i < len; i += step) {
    ret ^= ps[i];
    ret *= 1099511628211ULL;
  }
  return ret;
}
struct CStringWrap {
  const char *s;
  size_t length;
  uint64_t hash;
  constexpr CStringWrap(const char *str) : s(str), length(0), hash(0) {
    length = cstring_length(str);
    hash = hash_string(str, length);
  }
};
}
}
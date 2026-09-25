#include "spec.h"

#include <cctype>
#include <cstring>
#include <map>
#include <string>

namespace anycanvas {

const char* ac_lower(const char* upper) {
  static std::map<std::string, std::string> cache;
  auto it = cache.find(upper);
  if (it != cache.end()) return it->second.c_str();
  std::string s(upper);
  for (auto& c : s) c = (char)std::tolower((unsigned char)c);
  return cache.emplace(upper, s).first->second.c_str();
}

namespace {

// camelCase of an UPPER_SNAKE name, built once.
const char* camel(const char* upper) {
  static std::map<std::string, std::string> cache;
  auto it = cache.find(upper);
  if (it != cache.end()) return it->second.c_str();
  std::string s;
  bool up = false;
  for (const char* p = upper; *p; ++p) {
    if (*p == '_') { up = true; continue; }
    s += up ? (char)std::toupper((unsigned char)*p) : (char)std::tolower((unsigned char)*p);
    up = false;
  }
  return cache.emplace(upper, s).first->second.c_str();
}

struct OpTable {
  OpLayout layouts[256] = {};
  int32_t count = 0;
  OpTable() {
#define AC_OP(NAME, ID, ARGS, REPEAT, DOC) layouts[ID] = { #NAME, ARGS, REPEAT }; if (ID + 1 > count) count = ID + 1;
    ANYCANVAS_OPS(AC_OP)
#undef AC_OP
  }
};
const OpTable& ops() { static OpTable t; return t; }

}  // namespace

const OpLayout* opLayout(int32_t id) {
  if (id < 0 || id >= 256) return nullptr;
  const OpLayout& l = ops().layouts[id];
  return l.name ? &l : nullptr;
}

int32_t opCount() { return ops().count; }

const char* drawName(int32_t id) {
  switch (id) {
#define AC_DRAW(NAME, ID, DOC) case ID: return camel(#NAME);
    ANYCANVAS_DRAW(AC_DRAW)
#undef AC_DRAW
    default: return nullptr;
  }
}

}  // namespace anycanvas

// An opcode stream file: {"cmd": [numbers...], "refs": [strings...]} — what the TS recorder's tests
// write and the core tools / golden tests read.
#pragma once

#include "json.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace acstream {

struct Stream {
  std::vector<float> cmd;
  std::vector<std::string> refs;
  float scale = 1;   // optional "scale" in the file: the device-pixel multiplier of the golden
};

inline bool readFile(const std::string& path, std::string& out) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return false;
  std::ostringstream ss;
  ss << in.rdbuf();
  out = ss.str();
  return true;
}

inline bool parse(const std::string& text, Stream& out, std::string* err = nullptr) {
  acjson::Value root;
  if (!acjson::parse(text, root, err)) return false;
  const acjson::Value* cmd = root.get("cmd");
  const acjson::Value* refs = root.get("refs");
  if (!cmd || !cmd->isArray() || !refs || !refs->isArray()) { if (err) *err = "expected {cmd: [], refs: []}"; return false; }
  for (const auto& v : cmd->arr) out.cmd.push_back((float)v.num);
  for (const auto& v : refs->arr) out.refs.push_back(v.str);
  if (const acjson::Value* s = root.get("scale")) if (s->isNumber()) out.scale = (float)s->num;
  return true;
}

inline bool load(const std::string& path, Stream& out, std::string* err = nullptr) {
  std::string text;
  if (!readFile(path, text)) { if (err) *err = "cannot read " + path; return false; }
  return parse(text, out, err);
}

}  // namespace acstream

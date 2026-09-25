// A minimal JSON reader for the tools and tests (stream files, expected draw lists). Not a library:
// objects, arrays, strings (with escapes), numbers, true / false / null; no comments.
#pragma once

#include <cmath>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace acjson {

struct Value {
  enum Kind { Null, Bool, Number, String, Array, Object } kind = Null;
  bool b = false;
  double num = 0;
  std::string str;
  std::vector<Value> arr;
  std::map<std::string, Value> obj;

  const Value* get(const std::string& key) const {
    if (kind != Object) return nullptr;
    auto it = obj.find(key);
    return it == obj.end() ? nullptr : &it->second;
  }
  bool isArray() const { return kind == Array; }
  bool isNumber() const { return kind == Number; }
  bool isString() const { return kind == String; }
};

class Parser {
 public:
  explicit Parser(const std::string& text) : s_(text) {}

  bool parse(Value& out) {
    skip();
    if (!value(out)) return false;
    skip();
    return pos_ == s_.size();
  }
  const std::string& error() const { return err_; }

 private:
  const std::string& s_;
  size_t pos_ = 0;
  std::string err_;

  bool fail(const char* what) { if (err_.empty()) err_ = std::string(what) + " at " + std::to_string(pos_); return false; }
  void skip() { while (pos_ < s_.size() && (s_[pos_] == ' ' || s_[pos_] == '\n' || s_[pos_] == '\r' || s_[pos_] == '\t')) pos_++; }
  bool peek(char c) const { return pos_ < s_.size() && s_[pos_] == c; }

  bool value(Value& v) {
    if (pos_ >= s_.size()) return fail("unexpected end");
    const char c = s_[pos_];
    if (c == '{') return object(v);
    if (c == '[') return array(v);
    if (c == '"') { v.kind = Value::String; return string(v.str); }
    if (c == 't' && s_.compare(pos_, 4, "true") == 0) { v.kind = Value::Bool; v.b = true; pos_ += 4; return true; }
    if (c == 'f' && s_.compare(pos_, 5, "false") == 0) { v.kind = Value::Bool; v.b = false; pos_ += 5; return true; }
    if (c == 'n' && s_.compare(pos_, 4, "null") == 0) { v.kind = Value::Null; pos_ += 4; return true; }
    if (c == '-' || (c >= '0' && c <= '9')) {
      const char* start = s_.c_str() + pos_;
      char* end = nullptr;
      v.num = std::strtod(start, &end);
      if (end == start) return fail("bad number");
      pos_ += (size_t)(end - start);
      v.kind = Value::Number;
      return true;
    }
    return fail("unexpected character");
  }

  bool string(std::string& out) {
    if (!peek('"')) return fail("expected string");
    pos_++;
    while (pos_ < s_.size()) {
      const char c = s_[pos_++];
      if (c == '"') return true;
      if (c != '\\') { out += c; continue; }
      if (pos_ >= s_.size()) return fail("bad escape");
      const char e = s_[pos_++];
      switch (e) {
        case '"': out += '"'; break;
        case '\\': out += '\\'; break;
        case '/': out += '/'; break;
        case 'b': out += '\b'; break;
        case 'f': out += '\f'; break;
        case 'n': out += '\n'; break;
        case 'r': out += '\r'; break;
        case 't': out += '\t'; break;
        case 'u': {
          if (pos_ + 4 > s_.size()) return fail("bad \\u");
          unsigned cp = (unsigned)std::strtoul(s_.substr(pos_, 4).c_str(), nullptr, 16);
          pos_ += 4;
          if (cp >= 0xD800 && cp <= 0xDBFF && pos_ + 6 <= s_.size() && s_[pos_] == '\\' && s_[pos_ + 1] == 'u') {
            unsigned lo = (unsigned)std::strtoul(s_.substr(pos_ + 2, 4).c_str(), nullptr, 16);
            if (lo >= 0xDC00 && lo <= 0xDFFF) { cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00); pos_ += 6; }
          }
          if (cp < 0x80) out += (char)cp;
          else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
          else if (cp < 0x10000) { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
          else { out += (char)(0xF0 | (cp >> 18)); out += (char)(0x80 | ((cp >> 12) & 0x3F)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
          break;
        }
        default: return fail("bad escape");
      }
    }
    return fail("unterminated string");
  }

  bool array(Value& v) {
    v.kind = Value::Array;
    pos_++;
    skip();
    if (peek(']')) { pos_++; return true; }
    while (true) {
      Value item;
      skip();
      if (!value(item)) return false;
      v.arr.push_back(std::move(item));
      skip();
      if (peek(',')) { pos_++; continue; }
      if (peek(']')) { pos_++; return true; }
      return fail("expected , or ]");
    }
  }

  bool object(Value& v) {
    v.kind = Value::Object;
    pos_++;
    skip();
    if (peek('}')) { pos_++; return true; }
    while (true) {
      skip();
      std::string key;
      if (!string(key)) return false;
      skip();
      if (!peek(':')) return fail("expected :");
      pos_++;
      skip();
      Value item;
      if (!value(item)) return false;
      v.obj[key] = std::move(item);
      skip();
      if (peek(',')) { pos_++; continue; }
      if (peek('}')) { pos_++; return true; }
      return fail("expected , or }");
    }
  }
};

inline bool parse(const std::string& text, Value& out, std::string* err = nullptr) {
  Parser p(text);
  const bool ok = p.parse(out);
  if (!ok && err) *err = p.error();
  return ok;
}

}  // namespace acjson

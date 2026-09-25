// The golden tests: every tests/golden/streams/*.json (an opcode stream) and tests/golden/svg/*.svg
// becomes a draw list whose JSON must equal tests/golden/expected/<name>.json byte for byte. The
// binary twin tests/golden/expected/<name>.bin is what the TS reader test decodes.
//
//   anycanvas-golden <tests/golden>            compare (ctest)
//   anycanvas-golden <tests/golden> --update   rewrite the expected files (review the diff!)
//
// SVG goldens render at the natural size unless a sidecar <name>.svg.json says {"fit": [w, h], "tint": "#..."}.
//
// The color golden: tests/golden/colors/corpus.json (the syntax cases, by group) + every name of
// anycanvas/css_color.h and its keywords, lower and UPPER case → tests/golden/colors/expected.json,
// one line per input: {"in", "rgba8": "#rrggbbaa" | null, "rgba": [r, g, b, a] | null}. The TS twin's
// test (tests/ts/cssColor.test.ts) must produce the same bits from the same file.
#include "anycanvas/css_color.h"
#include "css.h"
#include "drawlist.h"
#include "interpreter.h"
#include "svg.h"
#include "common/stream.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace anycanvas;

namespace {

bool writeText(const fs::path& p, const std::string& text) {
  std::ofstream out(p, std::ios::binary);
  out.write(text.data(), (std::streamsize)text.size());
  return (bool)out;
}

bool writeBinary(const fs::path& path, const DrawList& list) {
  std::ofstream out(path, std::ios::binary);
  if (!out) return false;
  auto u32 = [&](uint32_t v) { out.write((const char*)&v, 4); };
  u32((uint32_t)list.words.size());
  if (!list.words.empty()) out.write((const char*)list.words.data(), (std::streamsize)(list.words.size() * 4));
  u32((uint32_t)list.strings.size());
  for (const auto& s : list.strings) { u32((uint32_t)s.size()); out.write(s.data(), (std::streamsize)s.size()); }
  return (bool)out;
}

struct Case { std::string name; DrawList list; };

// Structural equality with a numeric tolerance: |a - b| <= 1e-3 + 1e-4 * max(|a|, |b|). Float trig
// (nanosvg's arcs, our rotations) differs in the last digits between libms and with FMA contraction,
// so a golden written on one platform is byte-exact there and numerically exact everywhere else.
bool valuesEqual(const acjson::Value& a, const acjson::Value& b) {
  if (a.kind != b.kind) return false;
  switch (a.kind) {
    case acjson::Value::Null: return true;
    case acjson::Value::Bool: return a.b == b.b;
    case acjson::Value::Number: return std::fabs(a.num - b.num) <= 1e-3 + 1e-4 * std::fmax(std::fabs(a.num), std::fabs(b.num));
    case acjson::Value::String: return a.str == b.str;
    case acjson::Value::Array:
      if (a.arr.size() != b.arr.size()) return false;
      for (size_t i = 0; i < a.arr.size(); i++) if (!valuesEqual(a.arr[i], b.arr[i])) return false;
      return true;
    case acjson::Value::Object:
      if (a.obj.size() != b.obj.size()) return false;
      for (const auto& kv : a.obj) {
        auto it = b.obj.find(kv.first);
        if (it == b.obj.end() || !valuesEqual(kv.second, it->second)) return false;
      }
      return true;
  }
  return false;
}

bool numericallyEqual(const std::string& expectedText, const std::string& actualText) {
  acjson::Value e, a;
  if (!acjson::parse(expectedText, e) || !acjson::parse(actualText, a)) return false;
  return valuesEqual(e, a);
}

// Shows the first differing line of a failed golden.
void printFirstDiff(const std::string& expected, const std::string& actual) {
  size_t i = 0;
  while (i < expected.size() && i < actual.size() && expected[i] == actual[i]) i++;
  const size_t ls = expected.rfind('\n', i) == std::string::npos ? 0 : expected.rfind('\n', i) + 1;
  std::printf("    expected: %s\n", expected.substr(ls, expected.find('\n', ls) - ls).c_str());
  const size_t la = actual.rfind('\n', i) == std::string::npos ? 0 : actual.rfind('\n', i) + 1;
  std::printf("    actual:   %s\n", actual.substr(la, actual.find('\n', la) - la).c_str());
}

bool buildStream(const fs::path& file, DrawList& list) {
  acstream::Stream st;
  std::string err;
  if (!acstream::load(file.string(), st, &err)) { std::fprintf(stderr, "  %s: %s\n", file.string().c_str(), err.c_str()); return false; }
  std::vector<const char*> refs;
  for (const auto& r : st.refs) refs.push_back(r.c_str());
  interpret(st.cmd.data(), (int32_t)st.cmd.size(), refs.data(), (int32_t)refs.size(), st.scale, list);
  return true;
}

bool buildSvg(const fs::path& file, DrawList& list) {
  std::string text;
  if (!acstream::readFile(file.string(), text)) return false;
  float fitW = 0, fitH = 0;
  bool hasTint = false;
  Color tint;
  const fs::path sidecar = file.string() + ".json";
  if (fs::exists(sidecar)) {
    std::string side;
    acjson::Value v;
    if (acstream::readFile(sidecar.string(), side) && acjson::parse(side, v)) {
      if (const acjson::Value* fit = v.get("fit")) if (fit->isArray() && fit->arr.size() == 2) { fitW = (float)fit->arr[0].num; fitH = (float)fit->arr[1].num; }
      if (const acjson::Value* t = v.get("tint")) if (t->isString() && parseCssColor(t->str.c_str(), tint)) hasTint = true;
    }
  }
  Svg* svg = parseSvg(text.data(), text.size());
  list.clear();
  if (!svg) return true;   // a hostile input: an empty draw list is the expected outcome
  drawSvg(list, *svg, fitW, fitH, hasTint ? &tint : nullptr);
  delete svg;
  return true;
}

std::string jsonQuote(const std::string& s) {
  std::string out = "\"";
  for (const unsigned char c : s) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (c < 0x20 || c == 0x7f) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); out += b; }
        else out += (char)c;
    }
  }
  return out + "\"";
}

bool buildColors(const fs::path& dir, std::string& json) {
  std::string text, err;
  acjson::Value corpus;
  if (!acstream::readFile((dir / "corpus.json").string(), text) || !acjson::parse(text, corpus, &err) || corpus.kind != acjson::Value::Object) {
    std::fprintf(stderr, "  %s: %s\n", (dir / "corpus.json").string().c_str(), err.empty() ? "expected {group: [strings]}" : err.c_str());
    return false;
  }
  std::vector<std::string> inputs;
  for (const auto& group : corpus.obj)
    for (const auto& v : group.second.arr) if (v.isString()) inputs.push_back(v.str);
  std::vector<std::string> names;
  size_t count = 0;
  const css::NamedColor* table = css::namedColors(count);
  for (size_t i = 0; i < count; i++) names.push_back(table[i].name);
  names.push_back("transparent");
  names.push_back("clear");
  for (const auto& name : names) {
    std::string upper = name;
    for (char& ch : upper) if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
    inputs.push_back(name);
    inputs.push_back(upper);
  }
  json = "{\"names\":[";
  for (size_t i = 0; i < names.size(); i++) json += (i ? "," : "") + jsonQuote(names[i]);
  json += "],\n\"cases\":[\n";
  for (size_t i = 0; i < inputs.size(); i++) {
    css::Rgba c;
    json += "{\"in\":" + jsonQuote(inputs[i]);
    if (css::parseColor(inputs[i].c_str(), c)) {
      char hex[16];
      std::snprintf(hex, sizeof hex, "#%08x", (unsigned)css::toRGBA8(c));
      json += ",\"rgba8\":\"" + std::string(hex) + "\",\"rgba\":[" + formatFloat(c.r) + "," + formatFloat(c.g) + "," + formatFloat(c.b) + "," + formatFloat(c.a) + "]}";
    } else {
      json += ",\"rgba8\":null,\"rgba\":null}";
    }
    json += i + 1 < inputs.size() ? ",\n" : "\n";
  }
  json += "]}\n";
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) { std::fprintf(stderr, "usage: anycanvas-golden <tests/golden> [--update]\n"); return 2; }
  const fs::path root = argv[1];
  const bool update = argc > 2 && std::string(argv[2]) == "--update";
  const fs::path expectedDir = root / "expected";
  fs::create_directories(expectedDir);

  std::vector<Case> cases;
  std::vector<fs::path> files;
  for (const auto& e : fs::directory_iterator(root / "streams")) if (e.path().extension() == ".json") files.push_back(e.path());
  std::sort(files.begin(), files.end());
  for (const auto& f : files) {
    Case c; c.name = "stream-" + f.stem().string();
    if (!buildStream(f, c.list)) return 1;
    cases.push_back(std::move(c));
  }
  files.clear();
  for (const auto& e : fs::directory_iterator(root / "svg")) if (e.path().extension() == ".svg") files.push_back(e.path());
  std::sort(files.begin(), files.end());
  for (const auto& f : files) {
    Case c; c.name = "svg-" + f.stem().string();
    if (!buildSvg(f, c.list)) return 1;
    cases.push_back(std::move(c));
  }

  int failed = 0;
  for (const auto& c : cases) {
    const std::string json = c.list.toJson();
    const fs::path jsonPath = expectedDir / (c.name + ".json");
    const fs::path binPath = expectedDir / (c.name + ".bin");
    if (update) {
      writeText(jsonPath, json);
      writeBinary(binPath, c.list);
      std::printf("  wrote %s\n", jsonPath.filename().string().c_str());
      continue;
    }
    std::string expected;
    if (!acstream::readFile(jsonPath.string(), expected)) { std::printf("  MISSING %s (run --update)\n", jsonPath.filename().string().c_str()); failed++; continue; }
    if (expected != json && numericallyEqual(expected, json)) {
      // Another platform's libm / FMA rounding: the same structure, numbers within tolerance.
      std::printf("  ok~  %s (numerically equal, not byte-equal: goldens were written elsewhere)\n", c.name.c_str());
      continue;
    }
    if (expected != json) {
      std::printf("  FAIL %s\n", c.name.c_str());
      printFirstDiff(expected, json);
      failed++;
      continue;
    }
    std::printf("  ok   %s\n", c.name.c_str());
  }

  // The color golden (not under expected/: the TS tests read every expected/*.json as a draw list).
  std::string colors;
  if (!buildColors(root / "colors", colors)) return 1;
  const fs::path colorsPath = root / "colors" / "expected.json";
  if (update) {
    writeText(colorsPath, colors);
    std::printf("  wrote colors/%s\n", colorsPath.filename().string().c_str());
  } else {
    std::string expected;
    if (!acstream::readFile(colorsPath.string(), expected)) { std::printf("  MISSING colors/%s (run --update)\n", colorsPath.filename().string().c_str()); failed++; }
    else if (expected == colors) std::printf("  ok   colors\n");
    else if (numericallyEqual(expected, colors)) std::printf("  ok~  colors (numerically equal, not byte-equal: goldens were written elsewhere)\n");
    else {
      std::printf("  FAIL colors\n");
      printFirstDiff(expected, colors);
      failed++;
    }
  }

  if (update) { std::printf("updated %zu goldens\n", cases.size() + 1); return 0; }
  std::printf("%zu goldens, %d failed\n", cases.size() + 1, failed);
  return failed ? 1 : 0;
}
